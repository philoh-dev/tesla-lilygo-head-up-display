#include "test.hpp"

#include "tesla_headup/adapters/dashkit_tesla/dashkit_telemetry_ingestor.hpp"
#include "tesla_headup/application/dashboard/dashboard.hpp"
#include "tesla_headup/application/dashboard/turn_signal_indicators.hpp"
#include "tesla_headup/application/hud_runtime/dashboard_render_scheduler.hpp"
#include "tesla_headup/application/hud_runtime/turn_signal_presenter.hpp"
#include "tesla_headup/application/telemetry/telemetry_store.hpp"

#include <chrono>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using namespace std::chrono_literals;
using tesla_headup::application::Dashboard;
using tesla_headup::application::DashboardCanvas;
using tesla_headup::application::DashboardRenderScheduler;
using tesla_headup::application::EdgeGradientElement;
using tesla_headup::application::SignalStatus;
using tesla_headup::application::Symbol;
using tesla_headup::application::SymbolElement;
using tesla_headup::application::TelemetryStore;
using tesla_headup::application::TextElement;
using tesla_headup::application::TurnSignalIndicators;
using tesla_headup::application::TurnSignalPresenter;
using tesla_headup::dashkit::DashKitTelemetryIngestor;
using tesla_headup::dashkit::protocol::PacketError;
using tesla_headup::domain::TurnSignals;

class RecordingCanvas final : public DashboardCanvas {
public:
    void begin_frame() override { current_symbols.clear(); }
    void draw_text(const TextElement&) override {}
    void draw_symbol(const SymbolElement& element) override {
        if (element.visible) {
            current_symbols.push_back(element.symbol);
        }
    }
    void draw_edge_gradient(const EdgeGradientElement&) override {}
    void end_frame() override { last_frame_symbols = current_symbols; }

    std::vector<Symbol> current_symbols;
    std::vector<Symbol> last_frame_symbols;
};

std::vector<std::uint8_t> turn_signal_packet(
    bool left,
    std::uint8_t raw_status,
    std::uint32_t timestamp = 1,
    bool pad_to_classic_can_size = false) {
    const std::uint32_t address = left ? 994U : 995U;
    const std::uint8_t payload_size =
        pad_to_classic_can_size ? 8U : (left ? 7U : 2U);
    std::vector<std::uint8_t> packet{
        1,
        static_cast<std::uint8_t>(timestamp),
        static_cast<std::uint8_t>(timestamp >> 8U),
        static_cast<std::uint8_t>(timestamp >> 16U),
        static_cast<std::uint8_t>(timestamp >> 24U),
        1,
        static_cast<std::uint8_t>(address),
        static_cast<std::uint8_t>(address >> 8U),
        static_cast<std::uint8_t>(address >> 16U),
        static_cast<std::uint8_t>(address >> 24U),
        payload_size,
        static_cast<std::uint8_t>(raw_status << 4U),
    };
    packet.resize(11U + payload_size, 0);
    return packet;
}

struct TurnSignalRuntime {
    RecordingCanvas canvas;
    TelemetryStore telemetry;
    TurnSignalIndicators feature;
    Dashboard dashboard{canvas};
    DashboardRenderScheduler scheduler{dashboard};
    TurnSignalPresenter presenter{telemetry, feature, scheduler};
    DashKitTelemetryIngestor ingestor{telemetry};

    TurnSignalRuntime() { CHECK(dashboard.add_feature(feature)); }
};

TEST_CASE("DashKit left turn payload reaches the dashboard") {
    TurnSignalRuntime runtime;
    CHECK(runtime.scheduler.tick(0ms));
    const auto packet = turn_signal_packet(true, 1);

    const auto ingest = runtime.ingestor.ingest(packet, 10ms);
    CHECK(ingest.accepted_packet());
    CHECK_EQ(ingest.decoded_turn_signal_frames, 1);
    CHECK(runtime.presenter.refresh(10ms));
    CHECK(runtime.scheduler.tick(40ms));

    CHECK_EQ(runtime.feature.signals(), TurnSignals::left());
    CHECK(runtime.feature.illuminated());
    CHECK_EQ(runtime.canvas.last_frame_symbols.size(), 1U);
    CHECK_EQ(runtime.canvas.last_frame_symbols.front(), Symbol::LeftArrow);
}

TEST_CASE("padded DashKit right turn payload reaches the dashboard") {
    TurnSignalRuntime runtime;
    CHECK(runtime.scheduler.tick(0ms));
    const auto packet = turn_signal_packet(false, 1, 1, true);

    const auto ingest = runtime.ingestor.ingest(packet, 10ms);
    CHECK(ingest.accepted_packet());
    CHECK_EQ(ingest.decoded_turn_signal_frames, 1);
    CHECK(runtime.presenter.refresh(10ms));
    CHECK(runtime.scheduler.tick(40ms));

    CHECK_EQ(runtime.feature.signals(), TurnSignals::right());
    CHECK(runtime.feature.illuminated());
    CHECK_EQ(runtime.canvas.last_frame_symbols.size(), 1U);
    CHECK_EQ(runtime.canvas.last_frame_symbols.front(), Symbol::RightArrow);
}

TEST_CASE("physical dark phase dims but retains the active direction") {
    TurnSignalRuntime runtime;
    CHECK(runtime.scheduler.tick(0ms));
    CHECK(runtime.ingestor.ingest(
        turn_signal_packet(true, 1, 1), 10ms).accepted_packet());
    CHECK(runtime.presenter.refresh(10ms));
    CHECK(runtime.scheduler.tick(40ms));

    CHECK(runtime.ingestor.ingest(
        turn_signal_packet(true, 0, 2), 50ms).accepted_packet());
    CHECK(runtime.presenter.refresh(50ms));
    CHECK(runtime.scheduler.tick(80ms));

    CHECK_EQ(runtime.feature.signals(), TurnSignals::left());
    CHECK(!runtime.feature.illuminated());
    CHECK_EQ(runtime.canvas.last_frame_symbols.size(), 1U);
    CHECK_EQ(runtime.canvas.last_frame_symbols.front(), Symbol::LeftArrow);
}

TEST_CASE("repeated 100ms lamp updates keep each dark blink phase visible") {
    for (const bool left : {true, false}) {
        TurnSignalRuntime runtime;
        for (int sample = 0; sample < 32; ++sample) {
            const auto now = 100ms * (sample + 1);
            const bool illuminated = (sample / 4) % 2 == 0;
            CHECK(runtime.ingestor.ingest(
                turn_signal_packet(left, illuminated ? 1 : 0,
                                   static_cast<std::uint32_t>(sample)),
                now).accepted_packet());
            (void)runtime.presenter.refresh(now);
            (void)runtime.scheduler.tick(now);
            CHECK_EQ(runtime.feature.signals(),
                     left ? TurnSignals::left() : TurnSignals::right());
            CHECK_EQ(runtime.feature.illuminated(), illuminated);
            CHECK_EQ(runtime.canvas.last_frame_symbols.size(), 1U);
            CHECK_EQ(runtime.canvas.last_frame_symbols.front(),
                     left ? Symbol::LeftArrow : Symbol::RightArrow);
        }
    }
}

TEST_CASE("left and right lamp frames combine into hazards") {
    TurnSignalRuntime runtime;
    CHECK(runtime.scheduler.tick(0ms));

    CHECK(runtime.ingestor.ingest(
        turn_signal_packet(true, 1, 1), 10ms).accepted_packet());
    CHECK(runtime.ingestor.ingest(
        turn_signal_packet(false, 1, 2), 10ms).accepted_packet());
    CHECK(runtime.presenter.refresh(10ms));
    CHECK(runtime.scheduler.tick(40ms));

    CHECK_EQ(runtime.feature.signals(), TurnSignals::hazard());
    CHECK_EQ(runtime.canvas.last_frame_symbols.size(), 2U);
    CHECK_EQ(runtime.canvas.last_frame_symbols.at(0), Symbol::LeftArrow);
    CHECK_EQ(runtime.canvas.last_frame_symbols.at(1), Symbol::RightArrow);
}

TEST_CASE("switching turn direction never renders a retained opposite arrow") {
    for (const bool initially_left : {true, false}) {
        TurnSignalRuntime runtime;
        CHECK(runtime.scheduler.tick(0ms));
        CHECK(runtime.ingestor.ingest(
            turn_signal_packet(initially_left, 1, 1), 10ms).accepted_packet());
        CHECK(runtime.presenter.refresh(10ms));
        CHECK(runtime.scheduler.tick(40ms));
        CHECK(runtime.ingestor.ingest(
            turn_signal_packet(initially_left, 0, 2), 50ms).accepted_packet());
        CHECK(runtime.presenter.refresh(50ms));
        CHECK(runtime.scheduler.tick(80ms));
        CHECK_EQ(runtime.canvas.last_frame_symbols.size(), 1U);
        CHECK(!runtime.feature.illuminated());

        CHECK(runtime.ingestor.ingest(
            turn_signal_packet(!initially_left, 1, 3), 90ms).accepted_packet());
        CHECK(runtime.presenter.refresh(90ms));
        CHECK(runtime.scheduler.tick(120ms));
        CHECK_EQ(runtime.feature.signals(),
                 initially_left ? TurnSignals::right() : TurnSignals::left());
        CHECK_EQ(runtime.canvas.last_frame_symbols.size(), 1U);
        CHECK_EQ(runtime.canvas.last_frame_symbols.front(),
                 initially_left ? Symbol::RightArrow : Symbol::LeftArrow);

        // Repeated OFF feedback must not resurrect the cancelled direction.
        CHECK(runtime.ingestor.ingest(
            turn_signal_packet(initially_left, 0, 4), 130ms).accepted_packet());
        CHECK(!runtime.presenter.refresh(130ms));
        CHECK_EQ(runtime.feature.signals(),
                 initially_left ? TurnSignals::right() : TurnSignals::left());
    }
}

TEST_CASE("hazards survive dark phases and staggered lamp updates") {
    TurnSignalRuntime runtime;
    CHECK(runtime.ingestor.ingest(
        turn_signal_packet(true, 1, 1), 10ms).accepted_packet());
    CHECK(runtime.ingestor.ingest(
        turn_signal_packet(false, 1, 2), 20ms).accepted_packet());
    CHECK(runtime.presenter.refresh(20ms));
    CHECK(runtime.scheduler.tick(40ms));
    CHECK_EQ(runtime.feature.signals(), TurnSignals::hazard());

    CHECK(runtime.ingestor.ingest(
        turn_signal_packet(true, 0, 3), 250ms).accepted_packet());
    CHECK(runtime.ingestor.ingest(
        turn_signal_packet(false, 0, 4), 260ms).accepted_packet());
    CHECK(runtime.presenter.refresh(260ms));
    CHECK(runtime.scheduler.tick(280ms));
    CHECK_EQ(runtime.feature.signals(), TurnSignals::hazard());
    CHECK_EQ(runtime.canvas.last_frame_symbols.size(), 2U);
    CHECK(!runtime.feature.illuminated());

    CHECK(runtime.ingestor.ingest(
        turn_signal_packet(true, 1, 5), 500ms).accepted_packet());
    CHECK(runtime.ingestor.ingest(
        turn_signal_packet(false, 1, 6), 510ms).accepted_packet());
    CHECK(runtime.presenter.refresh(510ms));
    CHECK(runtime.scheduler.tick(520ms));
    CHECK_EQ(runtime.feature.signals(), TurnSignals::hazard());
    CHECK_EQ(runtime.canvas.last_frame_symbols.size(), 2U);
}

TEST_CASE("unavailable and fault lamp states clear the indicator") {
    TurnSignalRuntime runtime;
    CHECK(runtime.scheduler.tick(0ms));
    CHECK(runtime.ingestor.ingest(
        turn_signal_packet(false, 1, 1), 10ms).accepted_packet());
    CHECK(runtime.presenter.refresh(10ms));
    CHECK(runtime.scheduler.tick(40ms));

    const auto unavailable = runtime.ingestor.ingest(
        turn_signal_packet(false, 3, 2), 50ms);
    CHECK_EQ(unavailable.unavailable_turn_signal_frames, 1);
    CHECK(runtime.presenter.refresh(50ms));
    CHECK(runtime.scheduler.tick(80ms));
    CHECK_EQ(runtime.telemetry.turn_signals(80ms).right_status,
             SignalStatus::Unavailable);
    CHECK(runtime.canvas.last_frame_symbols.empty());

    const auto fault = runtime.ingestor.ingest(
        turn_signal_packet(false, 2, 3), 90ms);
    CHECK_EQ(fault.rejected_turn_signal_frames, 1);
}

TEST_CASE("malformed turn packets cannot partially alter telemetry") {
    TurnSignalRuntime runtime;
    const auto valid = turn_signal_packet(true, 1, 1);
    CHECK(runtime.ingestor.ingest(valid, 10ms).accepted_packet());
    auto malformed = turn_signal_packet(true, 0, 2);

    const auto ingest = runtime.ingestor.ingest(
        std::span{malformed}.first(malformed.size() - 1), 20ms);

    CHECK_EQ(ingest.packet_error, PacketError::TruncatedPayload);
    const auto snapshot = runtime.telemetry.turn_signals(20ms);
    CHECK_EQ(snapshot.signals, TurnSignals::left());
    CHECK(snapshot.illuminated);
}

}  // namespace
