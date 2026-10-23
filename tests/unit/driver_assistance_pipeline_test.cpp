#include "test.hpp"

#include "tesla_headup/adapters/dashkit_tesla/dashkit_telemetry_ingestor.hpp"
#include "tesla_headup/application/dashboard/autopilot_indicator.hpp"
#include "tesla_headup/application/dashboard/blind_spot_warnings.hpp"
#include "tesla_headup/application/dashboard/dashboard.hpp"
#include "tesla_headup/application/hud_runtime/autopilot_presenter.hpp"
#include "tesla_headup/application/hud_runtime/blind_spot_presenter.hpp"
#include "tesla_headup/application/hud_runtime/dashboard_render_scheduler.hpp"
#include "tesla_headup/application/telemetry/telemetry_store.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using namespace std::chrono_literals;
using tesla_headup::application::AutopilotIndicator;
using tesla_headup::application::AutopilotPresenter;
using tesla_headup::application::BlindSpotPresenter;
using tesla_headup::application::BlindSpotWarnings;
using tesla_headup::application::Dashboard;
using tesla_headup::application::DashboardCanvas;
using tesla_headup::application::DashboardRenderScheduler;
using tesla_headup::application::DisplayEdge;
using tesla_headup::application::EdgeGradientElement;
using tesla_headup::application::SignalStatus;
using tesla_headup::application::Symbol;
using tesla_headup::application::SymbolElement;
using tesla_headup::application::TelemetryStore;
using tesla_headup::application::TextElement;
using tesla_headup::dashkit::DashKitTelemetryIngestor;
using tesla_headup::dashkit::protocol::PacketError;

class RecordingCanvas final : public DashboardCanvas {
public:
    void begin_frame() override {
        current_symbols.clear();
        current_edges.clear();
    }
    void draw_text(const TextElement&) override {}
    void draw_symbol(const SymbolElement& element) override {
        current_symbols.push_back(element.symbol);
    }
    void draw_edge_gradient(const EdgeGradientElement& element) override {
        current_edges.push_back(element.edge);
    }
    void end_frame() override {
        last_frame_symbols = current_symbols;
        last_frame_edges = current_edges;
    }

    std::vector<Symbol> current_symbols;
    std::vector<Symbol> last_frame_symbols;
    std::vector<DisplayEdge> current_edges;
    std::vector<DisplayEdge> last_frame_edges;
};

std::array<std::uint8_t, 19> das_status_packet(
    std::uint8_t packed_state,
    std::uint32_t timestamp = 1) {
    return {
        1,
        static_cast<std::uint8_t>(timestamp),
        static_cast<std::uint8_t>(timestamp >> 8U),
        static_cast<std::uint8_t>(timestamp >> 16U),
        static_cast<std::uint8_t>(timestamp >> 24U),
        0,
        0x99, 0x03, 0x00, 0x00,
        8,
        packed_state, 0, 0, 0, 0, 0, 0, 0,
    };
}

struct DriverAssistanceRuntime {
    RecordingCanvas canvas;
    TelemetryStore telemetry;
    BlindSpotWarnings blind_spot_feature;
    AutopilotIndicator autopilot_feature;
    Dashboard dashboard{canvas};
    DashboardRenderScheduler scheduler{dashboard};
    BlindSpotPresenter blind_spot_presenter{
        telemetry, blind_spot_feature, scheduler};
    AutopilotPresenter autopilot_presenter{
        telemetry, autopilot_feature, scheduler};
    DashKitTelemetryIngestor ingestor{telemetry};

    DriverAssistanceRuntime() {
        CHECK(dashboard.add_feature(blind_spot_feature));
        CHECK(dashboard.add_feature(autopilot_feature));
    }

    bool refresh(std::chrono::milliseconds now) {
        const bool blind_spot_changed = blind_spot_presenter.refresh(now);
        const bool autopilot_changed = autopilot_presenter.refresh(now);
        return blind_spot_changed || autopilot_changed;
    }
};

TEST_CASE("DAS status drives blind-spot and active Autopilot visuals") {
    DriverAssistanceRuntime runtime;
    CHECK(runtime.scheduler.tick(0ms));

    const auto ingest = runtime.ingestor.ingest(
        das_status_packet(0x13), 10ms);

    CHECK(ingest.accepted_packet());
    CHECK_EQ(ingest.decoded_driver_assistance_frames, 1);
    CHECK(runtime.refresh(10ms));
    CHECK(runtime.scheduler.tick(40ms));
    CHECK_EQ(runtime.canvas.last_frame_edges.size(), 1U);
    CHECK_EQ(runtime.canvas.last_frame_edges.front(), DisplayEdge::Left);
    CHECK_EQ(runtime.canvas.last_frame_symbols.size(), 1U);
    CHECK_EQ(runtime.canvas.last_frame_symbols.front(), Symbol::Autopilot);
}

TEST_CASE("both blind spots combine while available Autopilot stays dark") {
    DriverAssistanceRuntime runtime;
    CHECK(runtime.scheduler.tick(0ms));

    CHECK(runtime.ingestor.ingest(
        das_status_packet(0x62), 10ms).accepted_packet());
    CHECK(runtime.refresh(10ms));
    CHECK(runtime.scheduler.tick(40ms));

    CHECK_EQ(runtime.canvas.last_frame_edges.size(), 2U);
    CHECK_EQ(runtime.canvas.last_frame_edges.at(0), DisplayEdge::Left);
    CHECK_EQ(runtime.canvas.last_frame_edges.at(1), DisplayEdge::Right);
    CHECK(runtime.canvas.last_frame_symbols.empty());
}

TEST_CASE("SNA driver-assistance fields fail all visuals dark") {
    DriverAssistanceRuntime runtime;
    CHECK(runtime.scheduler.tick(0ms));
    CHECK(runtime.ingestor.ingest(
        das_status_packet(0x53), 10ms).accepted_packet());
    CHECK(runtime.refresh(10ms));
    CHECK(runtime.scheduler.tick(40ms));

    const auto ingest = runtime.ingestor.ingest(
        das_status_packet(0xFF, 2), 50ms);
    CHECK_EQ(ingest.unavailable_driver_assistance_fields, 3);
    CHECK(runtime.refresh(50ms));
    CHECK(runtime.scheduler.tick(80ms));

    CHECK(runtime.canvas.last_frame_edges.empty());
    CHECK(runtime.canvas.last_frame_symbols.empty());
    CHECK_EQ(runtime.telemetry.blind_spots(80ms).left_status,
             SignalStatus::Unavailable);
    CHECK_EQ(runtime.telemetry.autopilot(80ms).status,
             SignalStatus::Unavailable);
}

TEST_CASE("reserved Autopilot state is rejected without erasing prior state") {
    DriverAssistanceRuntime runtime;
    CHECK(runtime.ingestor.ingest(
        das_status_packet(0x03), 10ms).accepted_packet());

    const auto ingest = runtime.ingestor.ingest(
        das_status_packet(0x07, 2), 20ms);

    CHECK_EQ(ingest.rejected_driver_assistance_frames, 1);
    CHECK(runtime.telemetry.autopilot(20ms).active());
}

TEST_CASE("Autopilot fault is retained but presented dark") {
    DriverAssistanceRuntime runtime;

    const auto ingest = runtime.ingestor.ingest(
        das_status_packet(0x0E), 10ms);

    CHECK_EQ(ingest.faulted_driver_assistance_fields, 1);
    const auto snapshot = runtime.telemetry.autopilot(10ms);
    CHECK(snapshot.fresh());
    CHECK_EQ(*snapshot.current_value, tesla_headup::domain::AutopilotState::Fault);
    CHECK(!snapshot.active());
    CHECK(!runtime.autopilot_feature.illuminated());
}

TEST_CASE("stale driver-assistance telemetry is removed") {
    DriverAssistanceRuntime runtime;
    CHECK(runtime.scheduler.tick(0ms));
    CHECK(runtime.ingestor.ingest(
        das_status_packet(0x93), 10ms).accepted_packet());
    CHECK(runtime.refresh(10ms));
    CHECK(runtime.scheduler.tick(40ms));

    CHECK(runtime.refresh(511ms));
    CHECK(runtime.scheduler.tick(511ms));

    CHECK(runtime.canvas.last_frame_edges.empty());
    // Blind spots retain their independent, shorter freshness policy.
    CHECK_EQ(runtime.canvas.last_frame_symbols.size(), 1U);
    CHECK_EQ(runtime.canvas.last_frame_symbols.front(), Symbol::Autopilot);

    CHECK(runtime.refresh(1511ms));
    CHECK(runtime.scheduler.tick(1511ms));
    CHECK(runtime.canvas.last_frame_symbols.empty());
}

TEST_CASE("active Autopilot does not flicker across jitter or a missed DAS cycle") {
    DriverAssistanceRuntime runtime;
    CHECK(runtime.ingestor.ingest(das_status_packet(0x03), 10ms).accepted_packet());
    CHECK(runtime.refresh(10ms));
    CHECK(runtime.scheduler.tick(40ms));
    auto previous = 40ms;
    for (const auto received : {530ms, 1060ms, 2110ms}) {
        // Exercise presentation between notifications, not just at receipt.
        for (auto now = previous + 10ms; now < received; now += 10ms) {
            CHECK(!runtime.refresh(now));
            CHECK(!runtime.scheduler.tick(now));
            CHECK(runtime.autopilot_feature.illuminated());
            CHECK_EQ(runtime.canvas.last_frame_symbols.size(), 1U);
            CHECK_EQ(runtime.canvas.last_frame_symbols.front(), Symbol::Autopilot);
        }
        CHECK(runtime.ingestor.ingest(
            das_status_packet(0x03), received).accepted_packet());
        CHECK(!runtime.refresh(received));
        CHECK(!runtime.scheduler.dirty());
        previous = received;
    }
    CHECK(!runtime.refresh(3610ms)); // Exactly 1,500 ms since last receipt.
    CHECK(runtime.autopilot_feature.illuminated());
    CHECK(runtime.refresh(3611ms));
    CHECK(runtime.scheduler.tick(3611ms));
    CHECK(runtime.canvas.last_frame_symbols.empty());
}

TEST_CASE("explicit inactive fault and SNA Autopilot states bypass jitter grace") {
    for (const std::uint8_t state : {0U, 1U, 2U, 8U, 9U, 14U, 15U}) {
        DriverAssistanceRuntime runtime;
        CHECK(runtime.ingestor.ingest(
            das_status_packet(0x03), 10ms).accepted_packet());
        CHECK(runtime.refresh(10ms));
        CHECK(runtime.scheduler.tick(40ms));
        CHECK(runtime.ingestor.ingest(
            das_status_packet(state), 600ms).accepted_packet());
        CHECK(runtime.refresh(600ms));
        CHECK(runtime.scheduler.tick(600ms));
        CHECK(!runtime.autopilot_feature.illuminated());
        CHECK(runtime.canvas.last_frame_symbols.empty());
    }
}

TEST_CASE("malformed and reserved DAS data cannot extend Autopilot freshness") {
    DriverAssistanceRuntime runtime;
    CHECK(runtime.ingestor.ingest(
        das_status_packet(0x03), 10ms).accepted_packet());
    CHECK(runtime.refresh(10ms));
    CHECK(runtime.scheduler.tick(40ms));
    const auto malformed = das_status_packet(0x03);
    CHECK(!runtime.ingestor.ingest(
        std::span{malformed}.first(malformed.size() - 1), 1400ms).accepted_packet());
    CHECK_EQ(runtime.ingestor.ingest(
        das_status_packet(0x07), 1450ms).rejected_driver_assistance_frames, 1);
    CHECK(!runtime.refresh(1510ms));
    CHECK(runtime.refresh(1511ms));
    CHECK(runtime.scheduler.tick(1511ms));
    CHECK(runtime.canvas.last_frame_symbols.empty());
}

TEST_CASE("malformed DAS status packets cannot alter telemetry") {
    DriverAssistanceRuntime runtime;
    const auto visible = das_status_packet(0x13);
    CHECK(runtime.ingestor.ingest(visible, 10ms).accepted_packet());
    const auto malformed = das_status_packet(0x00, 2);

    const auto ingest = runtime.ingestor.ingest(
        std::span{malformed}.first(malformed.size() - 1), 20ms);

    CHECK_EQ(ingest.packet_error, PacketError::TruncatedPayload);
    CHECK(runtime.telemetry.blind_spots(20ms).current_value.left_occupied());
    CHECK(runtime.telemetry.autopilot(20ms).active());
}

}  // namespace
