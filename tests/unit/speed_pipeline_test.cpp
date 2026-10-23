#include "test.hpp"

#include "tesla_headup/adapters/dashkit_tesla/dashkit_telemetry_ingestor.hpp"
#include "tesla_headup/application/dashboard/centered_vehicle_speed.hpp"
#include "tesla_headup/application/dashboard/dashboard.hpp"
#include "tesla_headup/application/hud_runtime/dashboard_render_scheduler.hpp"
#include "tesla_headup/application/hud_runtime/vehicle_speed_presenter.hpp"
#include "tesla_headup/application/telemetry/telemetry_store.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace {

using namespace std::chrono_literals;
using tesla_headup::application::CenteredVehicleSpeed;
using tesla_headup::application::Dashboard;
using tesla_headup::application::DashboardCanvas;
using tesla_headup::application::DashboardRenderScheduler;
using tesla_headup::application::EdgeGradientElement;
using tesla_headup::application::RenderSchedulePolicy;
using tesla_headup::application::SignalStatus;
using tesla_headup::application::SymbolElement;
using tesla_headup::application::TelemetryStore;
using tesla_headup::application::TextElement;
using tesla_headup::application::VehicleSpeedPresenter;
using tesla_headup::dashkit::DashKitTelemetryIngestor;
using tesla_headup::dashkit::protocol::PacketError;

class RecordingCanvas final : public DashboardCanvas {
public:
    void begin_frame() override {
        ++frame_count;
        current_text.clear();
    }

    void draw_text(const TextElement& element) override {
        current_text.emplace_back(element.text);
    }

    void draw_symbol(const SymbolElement&) override {}
    void draw_edge_gradient(const EdgeGradientElement&) override {}

    void end_frame() override { last_frame_text = current_text; }

    int frame_count{0};
    std::vector<std::string> current_text;
    std::vector<std::string> last_frame_text;
};

std::array<std::uint8_t, 19> metric_speed_packet(
    std::uint8_t kilometres_per_hour,
    std::uint32_t timestamp = 0x12345678U) {
    const auto raw = kilometres_per_hour == 0xFFU ? 0x0FFFU
        : ((static_cast<std::uint32_t>(kilometres_per_hour) + 40U) * 100U + 4U) / 8U;
    return {
        1,
        static_cast<std::uint8_t>(timestamp),
        static_cast<std::uint8_t>(timestamp >> 8U),
        static_cast<std::uint8_t>(timestamp >> 16U),
        static_cast<std::uint8_t>(timestamp >> 24U),
        0,
        0x57, 0x02, 0x00, 0x00,
        8,
        0, static_cast<std::uint8_t>(raw << 4U),
        static_cast<std::uint8_t>(raw >> 4U), kilometres_per_hour, 1, 0, 0, 0,
    };
}

struct SpeedRuntime {
    RecordingCanvas canvas;
    TelemetryStore telemetry;
    CenteredVehicleSpeed speed_feature;
    Dashboard dashboard{canvas};
    DashboardRenderScheduler scheduler{dashboard};
    VehicleSpeedPresenter presenter{
        telemetry, speed_feature, dashboard, scheduler};
    DashKitTelemetryIngestor ingestor{telemetry};

    SpeedRuntime() { CHECK(dashboard.add_feature(speed_feature, false)); }
};

TEST_CASE("DashKit speed payload reaches telemetry and the dashboard") {
    SpeedRuntime runtime;
    CHECK(runtime.scheduler.tick(0ms));
    CHECK(runtime.canvas.last_frame_text.empty());
    const auto packet = metric_speed_packet(135);

    const auto ingest = runtime.ingestor.ingest(packet, 10ms);
    CHECK(ingest.accepted_packet());
    CHECK_EQ(ingest.decoded_speed_frames, 1);
    CHECK_EQ(ingest.unavailable_speed_frames, 0);
    CHECK(runtime.presenter.refresh(10ms));

    CHECK(!runtime.scheduler.tick(39ms));
    CHECK(runtime.scheduler.tick(40ms));
    CHECK_EQ(runtime.canvas.frame_count, 2);
    CHECK_EQ(runtime.canvas.last_frame_text.size(), 2U);
    CHECK_EQ(runtime.canvas.last_frame_text[0], "135");
    CHECK_EQ(runtime.canvas.last_frame_text[1], "km/h");
}

TEST_CASE("render scheduler coalesces speed changes and renders the latest") {
    SpeedRuntime runtime;
    CHECK(runtime.scheduler.tick(0ms));
    const auto first = metric_speed_packet(41, 1);
    const auto second = metric_speed_packet(42, 2);

    CHECK(runtime.ingestor.ingest(first, 10ms).accepted_packet());
    CHECK(runtime.presenter.refresh(10ms));
    CHECK(runtime.ingestor.ingest(second, 20ms).accepted_packet());
    CHECK(runtime.presenter.refresh(20ms));

    CHECK(!runtime.scheduler.tick(20ms));
    CHECK(runtime.scheduler.tick(40ms));
    CHECK_EQ(runtime.canvas.frame_count, 2);
    CHECK_EQ(runtime.canvas.last_frame_text[0], "42");
}

TEST_CASE("metric 16 kmh reaches the HUD as 16 even with an imperial UI flag") {
    SpeedRuntime runtime;
    // Raw vehicle speed 700: 700 * 0.08 - 40 = 16 km/h.
    // UI also says 16, but its unit flag is clear: the previous decoder showed 26.
    const std::array<std::uint8_t, 19> packet{
        1, 1, 0, 0, 0, 0, 0x57, 0x02, 0, 0, 8,
        0, 0xC0, 0x2B, 16, 0, 0, 0, 0};
    CHECK(runtime.ingestor.ingest(packet, 10ms).accepted_packet());
    CHECK(runtime.presenter.refresh(10ms));
    CHECK(runtime.scheduler.tick(40ms));
    CHECK_EQ(runtime.canvas.last_frame_text[0], "16");
    CHECK_EQ(runtime.canvas.last_frame_text[1], "km/h");
}

TEST_CASE("Tesla displayed 30 beats physical 33 through the full speed pipeline") {
    SpeedRuntime runtime;
    // Synthetic counterexample, not a captured vehicle frame:
    // DI_vehicleSpeed raw 912 = 32.96 km/h, DI_uiSpeed = 30.
    const std::array<std::uint8_t, 19> packet{
        1, 1, 0, 0, 0, 0, 0x57, 0x02, 0, 0, 8,
        0, 0x00, 0x39, 30, 0, 0, 0, 0};
    CHECK(runtime.ingestor.ingest(packet, 10ms).accepted_packet());
    CHECK(runtime.presenter.refresh(10ms));
    CHECK(runtime.scheduler.tick(40ms));
    CHECK_EQ(runtime.canvas.last_frame_text[0], "30");
    CHECK_EQ(runtime.canvas.last_frame_text[1], "km/h");
}

TEST_CASE("unchanged speed does not request redundant frames") {
    SpeedRuntime runtime;
    CHECK(runtime.scheduler.tick(0ms));
    const auto packet = metric_speed_packet(50);
    CHECK(runtime.ingestor.ingest(packet, 10ms).accepted_packet());
    CHECK(runtime.presenter.refresh(10ms));
    CHECK(runtime.scheduler.tick(40ms));

    CHECK(!runtime.presenter.refresh(41ms));
    CHECK(!runtime.scheduler.dirty());
    CHECK(!runtime.scheduler.tick(100ms));
    CHECK_EQ(runtime.canvas.frame_count, 2);
}

TEST_CASE("stale speed is removed from the next dashboard frame") {
    SpeedRuntime runtime;
    CHECK(runtime.scheduler.tick(0ms));
    const auto packet = metric_speed_packet(88);
    CHECK(runtime.ingestor.ingest(packet, 10ms).accepted_packet());
    CHECK(runtime.presenter.refresh(10ms));
    CHECK(runtime.scheduler.tick(40ms));

    CHECK(runtime.presenter.refresh(511ms));
    CHECK(runtime.scheduler.tick(511ms));

    CHECK_EQ(runtime.telemetry.vehicle_speed(511ms).status, SignalStatus::Stale);
    CHECK(runtime.canvas.last_frame_text.empty());
}

TEST_CASE("disabled speed preference cannot be overridden by telemetry") {
    SpeedRuntime runtime;
    CHECK(runtime.scheduler.tick(0ms));
    runtime.presenter.set_feature_allowed(false);
    CHECK(!runtime.presenter.feature_allowed());
    const auto packet = metric_speed_packet(88);

    CHECK(runtime.ingestor.ingest(packet, 10ms).accepted_packet());
    CHECK(!runtime.presenter.refresh(10ms));
    CHECK(!runtime.dashboard.is_enabled(
        tesla_headup::application::FeatureId::VehicleSpeed));
    CHECK(runtime.canvas.last_frame_text.empty());
}

TEST_CASE("Tesla unavailable speed immediately hides the feature") {
    SpeedRuntime runtime;
    CHECK(runtime.scheduler.tick(0ms));
    const auto visible = metric_speed_packet(70, 1);
    CHECK(runtime.ingestor.ingest(visible, 10ms).accepted_packet());
    CHECK(runtime.presenter.refresh(10ms));
    CHECK(runtime.scheduler.tick(40ms));
    const auto unavailable = metric_speed_packet(0xFF, 2);

    const auto ingest = runtime.ingestor.ingest(unavailable, 50ms);
    CHECK_EQ(ingest.unavailable_speed_frames, 1);
    CHECK(runtime.presenter.refresh(50ms));
    CHECK(runtime.scheduler.tick(80ms));

    CHECK_EQ(
        runtime.telemetry.vehicle_speed(80ms).status,
        SignalStatus::Unavailable);
    CHECK(runtime.canvas.last_frame_text.empty());
}

TEST_CASE("malformed DashKit packets cannot alter visible speed") {
    SpeedRuntime runtime;
    CHECK(runtime.scheduler.tick(0ms));
    const auto valid = metric_speed_packet(60, 1);
    CHECK(runtime.ingestor.ingest(valid, 10ms).accepted_packet());
    CHECK(runtime.presenter.refresh(10ms));
    CHECK(runtime.scheduler.tick(40ms));
    auto malformed = metric_speed_packet(120, 2);

    const auto ingest = runtime.ingestor.ingest(
        std::span{malformed}.first(malformed.size() - 1),
        50ms);

    CHECK(!ingest.accepted_packet());
    CHECK_EQ(ingest.packet_error, PacketError::TruncatedPayload);
    CHECK(!runtime.presenter.refresh(50ms));
    CHECK_EQ(
        runtime.telemetry.vehicle_speed(50ms)
            .current_value->kilometres_per_hour(),
        60);
}

TEST_CASE("non-speed frames are accepted without dirtying the dashboard") {
    SpeedRuntime runtime;
    CHECK(runtime.scheduler.tick(0ms));
    auto packet = metric_speed_packet(70);
    packet[6] = 0xE3;
    packet[7] = 0x03;

    const auto ingest = runtime.ingestor.ingest(packet, 10ms);

    CHECK(ingest.accepted_packet());
    CHECK_EQ(ingest.decoded_speed_frames, 0);
    CHECK(!runtime.presenter.refresh(10ms));
    CHECK(!runtime.scheduler.tick(100ms));
}

TEST_CASE("render scheduler safely handles a backwards local clock") {
    RecordingCanvas canvas;
    CenteredVehicleSpeed feature;
    Dashboard dashboard{canvas};
    CHECK(dashboard.add_feature(feature));
    DashboardRenderScheduler scheduler{dashboard};
    CHECK(scheduler.tick(100ms));

    scheduler.request_render();

    CHECK(scheduler.tick(90ms));
    CHECK_EQ(canvas.frame_count, 2);
}

TEST_CASE("render interval is configurable") {
    RecordingCanvas canvas;
    CenteredVehicleSpeed feature;
    Dashboard dashboard{canvas};
    CHECK(dashboard.add_feature(feature));
    RenderSchedulePolicy policy;
    policy.minimum_frame_interval = 10ms;
    DashboardRenderScheduler scheduler{dashboard, policy};
    CHECK(scheduler.tick(0ms));
    scheduler.request_render();

    CHECK(!scheduler.tick(9ms));
    CHECK(scheduler.tick(10ms));
}

}  // namespace
