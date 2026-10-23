#include "test.hpp"

#include "tesla_headup/adapters/dashkit_tesla/dashkit_telemetry_ingestor.hpp"
#include "tesla_headup/application/dashboard/dashboard.hpp"
#include "tesla_headup/application/dashboard/exterior_light_indicators.hpp"
#include "tesla_headup/application/hud_runtime/dashboard_render_scheduler.hpp"
#include "tesla_headup/application/hud_runtime/exterior_light_presenter.hpp"
#include "tesla_headup/application/telemetry/telemetry_store.hpp"

#include <array>
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
using tesla_headup::application::ExteriorLightIndicators;
using tesla_headup::application::ExteriorLightPresenter;
using tesla_headup::application::SignalStatus;
using tesla_headup::application::Symbol;
using tesla_headup::application::SymbolElement;
using tesla_headup::application::TelemetryStore;
using tesla_headup::application::TextElement;
using tesla_headup::dashkit::DashKitTelemetryIngestor;
using tesla_headup::dashkit::protocol::PacketError;
using tesla_headup::domain::HighBeamMode;

class RecordingCanvas final : public DashboardCanvas {
public:
    void begin_frame() override { current_symbols.clear(); }
    void draw_text(const TextElement&) override {}
    void draw_symbol(const SymbolElement& element) override {
        current_symbols.push_back(element.symbol);
    }
    void draw_edge_gradient(const EdgeGradientElement&) override {}
    void end_frame() override { last_frame_symbols = current_symbols; }

    std::vector<Symbol> current_symbols;
    std::vector<Symbol> last_frame_symbols;
};

std::array<std::uint8_t, 19> frame_with_eight_bytes(
    std::uint32_t address,
    const std::array<std::uint8_t, 8>& data,
    std::uint32_t timestamp = 1) {
    return {
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
        8,
        data[0], data[1], data[2], data[3],
        data[4], data[5], data[6], data[7],
    };
}

std::array<std::uint8_t, 13> rear_lighting_packet(
    std::uint8_t rear_fog_status,
    std::uint32_t timestamp = 1) {
    return {
        1,
        static_cast<std::uint8_t>(timestamp),
        static_cast<std::uint8_t>(timestamp >> 8U),
        static_cast<std::uint8_t>(timestamp >> 16U),
        static_cast<std::uint8_t>(timestamp >> 24U),
        1,
        0xE3, 0x03, 0x00, 0x00,
        2,
        0,
        rear_fog_status,
    };
}

std::array<std::uint8_t, 15> frame_with_four_bytes(
    std::uint32_t address,
    const std::array<std::uint8_t, 4>& data,
    std::uint32_t timestamp = 1) {
    return {
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
        4,
        data[0], data[1], data[2], data[3],
    };
}

struct ExteriorLightRuntime {
    RecordingCanvas canvas;
    TelemetryStore telemetry;
    ExteriorLightIndicators feature;
    Dashboard dashboard{canvas};
    DashboardRenderScheduler scheduler{dashboard};
    ExteriorLightPresenter presenter{telemetry, feature, scheduler};
    DashKitTelemetryIngestor ingestor{telemetry};

    ExteriorLightRuntime() { CHECK(dashboard.add_feature(feature)); }
};

TEST_CASE("physical exterior-light payloads reach every dashboard slot") {
    ExteriorLightRuntime runtime;
    CHECK(runtime.scheduler.tick(0ms));
    const std::array<std::uint8_t, 8> front{
        0, 0, 0, 0x10, 0x01, 0x01, 0x40, 0};

    CHECK(runtime.ingestor.ingest(
        frame_with_eight_bytes(1009, front), 10ms).accepted_packet());
    const std::array<std::uint8_t, 8> padded_rear_fog{
        0, 1, 0, 0, 0, 0, 0, 0};
    CHECK(runtime.ingestor.ingest(
        frame_with_eight_bytes(995, padded_rear_fog), 10ms)
              .accepted_packet());
    CHECK(runtime.presenter.refresh(10ms));
    CHECK(runtime.scheduler.tick(40ms));

    CHECK_EQ(runtime.canvas.last_frame_symbols.size(), 5U);
    CHECK_EQ(runtime.canvas.last_frame_symbols.at(0), Symbol::PositionLights);
    CHECK_EQ(runtime.canvas.last_frame_symbols.at(1), Symbol::LowBeam);
    CHECK_EQ(runtime.canvas.last_frame_symbols.at(2), Symbol::HighBeam);
    CHECK_EQ(runtime.canvas.last_frame_symbols.at(3), Symbol::FrontFogLight);
    CHECK_EQ(runtime.canvas.last_frame_symbols.at(4), Symbol::RearFogLight);
}

TEST_CASE("Juniper forwarded messages reach every exterior-light slot") {
    ExteriorLightRuntime runtime;
    CHECK(runtime.scheduler.tick(0ms));
    const std::array<std::uint8_t, 8> body_controls{
        0x01, 0x10, 0, 0, 0, 0, 0, 0};
    const std::array<std::uint8_t, 8> lighting_controls{
        0x08, 0, 0, 0, 0, 0, 0, 0};
    const std::array<std::uint8_t, 4> rear_fog{0, 1, 0, 0};

    CHECK(runtime.ingestor.ingest(
        frame_with_eight_bytes(1001, body_controls), 10ms)
              .accepted_packet());
    CHECK(runtime.ingestor.ingest(
        frame_with_eight_bytes(627, lighting_controls), 10ms)
              .accepted_packet());
    CHECK(runtime.ingestor.ingest(
        frame_with_four_bytes(995, rear_fog), 10ms).accepted_packet());
    CHECK(runtime.presenter.refresh(10ms));
    CHECK(runtime.scheduler.tick(40ms));

    CHECK_EQ(runtime.canvas.last_frame_symbols.size(), 5U);
    CHECK_EQ(runtime.canvas.last_frame_symbols.at(0), Symbol::PositionLights);
    CHECK_EQ(runtime.canvas.last_frame_symbols.at(1), Symbol::LowBeam);
    CHECK_EQ(runtime.canvas.last_frame_symbols.at(2), Symbol::HighBeam);
    CHECK_EQ(runtime.canvas.last_frame_symbols.at(3), Symbol::FrontFogLight);
    CHECK_EQ(runtime.canvas.last_frame_symbols.at(4), Symbol::RearFogLight);
}

TEST_CASE("adaptive setting replaces the manual high-beam symbol") {
    ExteriorLightRuntime runtime;
    CHECK(runtime.scheduler.tick(0ms));
    const std::array<std::uint8_t, 8> high_beam{
        0, 0, 0, 0, 0x01, 0, 0, 0};
    const std::array<std::uint8_t, 8> adaptive_enabled{
        0, 0, 0, 0, 0, 0x02, 0, 0};

    CHECK(runtime.ingestor.ingest(
        frame_with_eight_bytes(1009, high_beam), 10ms).accepted_packet());
    CHECK(runtime.ingestor.ingest(
        frame_with_eight_bytes(627, adaptive_enabled), 10ms)
            .accepted_packet());
    CHECK(runtime.presenter.refresh(10ms));
    CHECK(runtime.scheduler.tick(40ms));

    CHECK_EQ(runtime.feature.lights().high_beam_mode(),
             HighBeamMode::Adaptive);
    CHECK_EQ(runtime.canvas.last_frame_symbols.size(), 1U);
    CHECK_EQ(
        runtime.canvas.last_frame_symbols.front(),
        Symbol::AdaptiveHighBeam);
}

TEST_CASE("stale adaptive setting safely falls back to manual high beam") {
    ExteriorLightRuntime runtime;
    const std::array<std::uint8_t, 8> high_beam{
        0, 0, 0, 0, 0x01, 0, 0, 0};
    const std::array<std::uint8_t, 8> adaptive_enabled{
        0, 0, 0, 0, 0, 0x02, 0, 0};
    CHECK(runtime.ingestor.ingest(
        frame_with_eight_bytes(627, adaptive_enabled), 0ms)
            .accepted_packet());
    CHECK(runtime.ingestor.ingest(
        frame_with_eight_bytes(1009, high_beam), 0ms).accepted_packet());
    CHECK(runtime.presenter.refresh(0ms));

    CHECK(runtime.ingestor.ingest(
        frame_with_eight_bytes(1009, high_beam, 2), 501ms)
            .accepted_packet());
    CHECK(runtime.presenter.refresh(501ms));

    CHECK_EQ(runtime.feature.lights().high_beam_mode(),
             HighBeamMode::Manual);
    CHECK_EQ(
        runtime.telemetry.exterior_lights(501ms)
            .adaptive_high_beam_status,
        SignalStatus::Stale);
}

TEST_CASE("faulted and unavailable exterior lights fail dark") {
    ExteriorLightRuntime runtime;
    const std::array<std::uint8_t, 8> invalid_front{
        0, 0, 0, 0x20, 0x03, 0, 0, 0};

    const auto front = runtime.ingestor.ingest(
        frame_with_eight_bytes(1009, invalid_front), 10ms);
    const auto rear = runtime.ingestor.ingest(
        rear_lighting_packet(3), 10ms);

    CHECK_EQ(front.faulted_exterior_light_fields, 1);
    CHECK_EQ(front.unavailable_exterior_light_fields, 1);
    CHECK_EQ(rear.unavailable_exterior_light_fields, 1);
    CHECK(!runtime.presenter.refresh(10ms));
    CHECK_EQ(runtime.feature.lights().high_beam_mode(), HighBeamMode::Off);
}

TEST_CASE("malformed exterior-light packets cannot alter telemetry") {
    ExteriorLightRuntime runtime;
    const std::array<std::uint8_t, 8> active{
        0, 0, 0, 0x10, 0, 0, 0, 0};
    CHECK(runtime.ingestor.ingest(
        frame_with_eight_bytes(1009, active), 10ms).accepted_packet());
    const auto malformed = frame_with_eight_bytes(1009, {}, 2);

    const auto ingest = runtime.ingestor.ingest(
        std::span{malformed}.first(malformed.size() - 1), 20ms);

    CHECK_EQ(ingest.packet_error, PacketError::TruncatedPayload);
    CHECK(runtime.telemetry.exterior_lights(20ms)
              .current_value.low_beam_active());
}

}  // namespace
