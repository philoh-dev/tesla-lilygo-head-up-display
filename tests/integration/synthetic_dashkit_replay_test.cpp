#include "test.hpp"

#include "synthetic_dashkit_packets.hpp"
#include "tesla_headup/adapters/dashkit_ble/dashkit_connection.hpp"
#include "tesla_headup/adapters/dashkit_ble/dashkit_profile.hpp"
#include "tesla_headup/adapters/dashkit_tesla/dashkit_status_mapper.hpp"
#include "tesla_headup/adapters/dashkit_tesla/dashkit_telemetry_ingestor.hpp"
#include "tesla_headup/application/dashboard/autopilot_indicator.hpp"
#include "tesla_headup/application/dashboard/blind_spot_warnings.hpp"
#include "tesla_headup/application/dashboard/centered_vehicle_speed.hpp"
#include "tesla_headup/application/dashboard/dashboard.hpp"
#include "tesla_headup/application/dashboard/exterior_light_indicators.hpp"
#include "tesla_headup/application/dashboard/system_status_indicator.hpp"
#include "tesla_headup/application/dashboard/selected_gear_indicator.hpp"
#include "tesla_headup/application/dashboard/speed_limit_indicator.hpp"
#include "tesla_headup/application/dashboard/turn_signal_indicators.hpp"
#include "tesla_headup/application/hud_runtime/autopilot_presenter.hpp"
#include "tesla_headup/application/hud_runtime/blind_spot_presenter.hpp"
#include "tesla_headup/application/hud_runtime/dashboard_render_scheduler.hpp"
#include "tesla_headup/application/hud_runtime/exterior_light_presenter.hpp"
#include "tesla_headup/application/hud_runtime/system_status_presenter.hpp"
#include "tesla_headup/application/hud_runtime/selected_gear_presenter.hpp"
#include "tesla_headup/application/hud_runtime/speed_limit_presenter.hpp"
#include "tesla_headup/application/hud_runtime/turn_signal_presenter.hpp"
#include "tesla_headup/application/hud_runtime/vehicle_speed_presenter.hpp"
#include "tesla_headup/application/telemetry/telemetry_store.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace std::chrono_literals;
using tesla_headup::application::AutopilotIndicator;
using tesla_headup::application::AutopilotPresenter;
using tesla_headup::application::BlindSpotPresenter;
using tesla_headup::application::BlindSpotWarnings;
using tesla_headup::application::CenteredVehicleSpeed;
using tesla_headup::application::Dashboard;
using tesla_headup::application::DashboardCanvas;
using tesla_headup::application::DashboardRenderScheduler;
using tesla_headup::application::DisplayEdge;
using tesla_headup::application::EdgeGradientElement;
using tesla_headup::application::ExteriorLightIndicators;
using tesla_headup::application::ExteriorLightPresenter;
using tesla_headup::application::Symbol;
using tesla_headup::application::SymbolElement;
using tesla_headup::application::SystemStatusIndicator;
using tesla_headup::application::SystemStatusPresenter;
using tesla_headup::application::SelectedGearIndicator;
using tesla_headup::application::SelectedGearPresenter;
using tesla_headup::application::SpeedLimitIndicator;
using tesla_headup::application::SpeedLimitPresenter;
using tesla_headup::application::TelemetryStore;
using tesla_headup::application::TextElement;
using tesla_headup::application::TurnSignalIndicators;
using tesla_headup::application::TurnSignalPresenter;
using tesla_headup::application::VehicleSpeedPresenter;
using tesla_headup::dashkit::BleCentral;
using tesla_headup::dashkit::BleCentralObserver;
using tesla_headup::dashkit::BleDeviceId;
using tesla_headup::dashkit::ConnectionSnapshot;
using tesla_headup::dashkit::ConnectionState;
using tesla_headup::dashkit::DashKitConnection;
using tesla_headup::dashkit::DashKitConnectionObserver;
using tesla_headup::dashkit::DashKitTelemetryIngestor;
using tesla_headup::dashkit::MonotonicTime;
using tesla_headup::dashkit::TelemetryIngestResult;

class ReplayBleCentral final : public BleCentral {
public:
    void set_observer(BleCentralObserver* value) override { observer = value; }
    bool scan_for_name(std::string_view) override { return true; }
    void stop_scan() override {}
    bool connect(BleDeviceId) override { return true; }
    bool secure_connection() override { return true; }
    bool discover_service(std::string_view) override { return true; }
    bool subscribe(std::string_view, std::string_view) override { return true; }
    bool read_firmware_version() override { return true; }
    bool send_keepalive() override { return true; }
    bool remove_peer_bond() override { return false; }
    void disconnect() override {}

    BleCentralObserver* observer{nullptr};
};

class RecordingCanvas final : public DashboardCanvas {
public:
    void begin_frame() override {
        current_text.clear();
        current_symbols.clear();
        current_edges.clear();
    }

    void draw_text(const TextElement& element) override {
        current_text.emplace_back(element.text);
    }

    void draw_symbol(const SymbolElement& element) override {
        if (element.visible) {
            current_symbols.push_back(element.symbol);
        }
    }

    void draw_edge_gradient(const EdgeGradientElement& element) override {
        current_edges.push_back(element.edge);
    }

    void end_frame() override {
        ++frame_count;
        text = current_text;
        symbols = current_symbols;
        edges = current_edges;
    }

    int frame_count{0};
    std::vector<std::string> current_text;
    std::vector<Symbol> current_symbols;
    std::vector<DisplayEdge> current_edges;
    std::vector<std::string> text;
    std::vector<Symbol> symbols;
    std::vector<DisplayEdge> edges;
};

class ReplayConnectionObserver final : public DashKitConnectionObserver {
public:
    explicit ReplayConnectionObserver(
        DashKitTelemetryIngestor& ingestor,
        SystemStatusPresenter& status_presenter) noexcept
        : ingestor_(ingestor), status_presenter_(status_presenter) {}

    void on_connection_state_changed(ConnectionSnapshot snapshot) override {
        last_connection = snapshot;
        (void)status_presenter_.set_connectivity(
            tesla_headup::dashkit::map_connectivity(snapshot), 0ms);
    }

    void on_can_payload(
        std::span<const std::uint8_t> payload,
        MonotonicTime received_at) override {
        const auto result = ingestor_.ingest(payload, received_at);
        ingest_results.push_back(result);
        if (result.accepted_packet()) {
            (void)status_presenter_.on_telemetry_received(received_at);
        }
    }

    DashKitTelemetryIngestor& ingestor_;
    SystemStatusPresenter& status_presenter_;
    ConnectionSnapshot last_connection{};
    std::vector<TelemetryIngestResult> ingest_results;
};

constexpr BleDeviceId dashkit_id() {
    return {{{0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6}}, 1};
}

struct ReplayRuntime {
    ReplayBleCentral central;
    TelemetryStore telemetry;
    DashKitTelemetryIngestor ingestor{telemetry};
    RecordingCanvas canvas;
    BlindSpotWarnings blind_spot_feature;
    CenteredVehicleSpeed speed_feature;
    TurnSignalIndicators turn_signal_feature;
    ExteriorLightIndicators exterior_light_feature;
    AutopilotIndicator autopilot_feature;
    SystemStatusIndicator system_status_feature;
    SelectedGearIndicator selected_gear_feature;
    SpeedLimitIndicator speed_limit_feature;
    Dashboard dashboard{canvas};
    DashboardRenderScheduler scheduler{dashboard};
    VehicleSpeedPresenter speed_presenter{
        telemetry, speed_feature, dashboard, scheduler};
    TurnSignalPresenter turn_signal_presenter{
        telemetry, turn_signal_feature, scheduler};
    BlindSpotPresenter blind_spot_presenter{
        telemetry, blind_spot_feature, scheduler};
    AutopilotPresenter autopilot_presenter{
        telemetry, autopilot_feature, scheduler};
    ExteriorLightPresenter exterior_light_presenter{
        telemetry, exterior_light_feature, scheduler};
    SystemStatusPresenter system_status_presenter{
        system_status_feature, scheduler};
    SelectedGearPresenter selected_gear_presenter{
        telemetry, selected_gear_feature, dashboard, scheduler};
    SpeedLimitPresenter speed_limit_presenter{
        telemetry, speed_limit_feature, dashboard, scheduler};
    ReplayConnectionObserver connection_observer{
        ingestor, system_status_presenter};
    DashKitConnection connection{central};

    ReplayRuntime() {
        connection.set_observer(&connection_observer);
        CHECK(dashboard.add_feature(blind_spot_feature));
        CHECK(dashboard.add_feature(speed_feature, false));
        CHECK(dashboard.add_feature(turn_signal_feature));
        CHECK(dashboard.add_feature(exterior_light_feature));
        CHECK(dashboard.add_feature(autopilot_feature));
        CHECK(dashboard.add_feature(system_status_feature));
        CHECK(dashboard.add_feature(selected_gear_feature, false));
        CHECK(dashboard.add_feature(speed_limit_feature, false));
    }

    void establish_connection() {
        constexpr std::array<std::uint8_t, 5> version{
            '0', '.', '1', '.', '2'};
        connection.start(0ms);
        central.observer->on_device_found({dashkit_id(), "DashKit"}, 1ms);
        central.observer->on_connected(2ms);
        central.observer->on_secured(3ms);
        central.observer->on_service_discovered(true, 4ms);
        central.observer->on_firmware_version_read(true, version, 5ms);
        central.observer->on_subscription_changed(true, 6ms);
        central.observer->on_keepalive_written(true, 7ms);
    }

    void replay(
        std::span<const std::uint8_t> packet,
        MonotonicTime received_at) {
        central.observer->on_notification(
            tesla_headup::dashkit::profile::can_characteristic_uuid,
            packet,
            received_at);
    }

    void refresh(MonotonicTime now) {
        (void)speed_presenter.refresh(now);
        (void)turn_signal_presenter.refresh(now);
        (void)blind_spot_presenter.refresh(now);
        (void)autopilot_presenter.refresh(now);
        (void)exterior_light_presenter.refresh(now);
        (void)system_status_presenter.refresh(now);
        (void)selected_gear_presenter.refresh(now);
        (void)speed_limit_presenter.refresh(now);
    }
};

void check_active_frame(const RecordingCanvas& canvas) {
    CHECK_EQ(canvas.text.size(), 4U);
    CHECK_EQ(canvas.text.at(0), "135");
    CHECK_EQ(canvas.text.at(1), "km/h");
    CHECK_EQ(canvas.text.at(2), "D");
    CHECK_EQ(canvas.text.at(3), "50");
    CHECK_EQ(canvas.edges.size(), 1U);
    CHECK_EQ(canvas.edges.front(), DisplayEdge::Left);
    CHECK_EQ(canvas.symbols.size(), 8U);
    CHECK_EQ(canvas.symbols.at(0), Symbol::LeftArrow);
    CHECK_EQ(canvas.symbols.at(1), Symbol::RightArrow);
    CHECK_EQ(canvas.symbols.at(2), Symbol::PositionLights);
    CHECK_EQ(canvas.symbols.at(3), Symbol::LowBeam);
    CHECK_EQ(canvas.symbols.at(4), Symbol::AdaptiveHighBeam);
    CHECK_EQ(canvas.symbols.at(5), Symbol::FrontFogLight);
    CHECK_EQ(canvas.symbols.at(6), Symbol::RearFogLight);
    CHECK_EQ(canvas.symbols.at(7), Symbol::Autopilot);
}

TEST_CASE("synthetic DashKit journey replays from BLE notification to HUD") {
    ReplayRuntime runtime;
    CHECK(runtime.scheduler.tick(0ms));
    runtime.establish_connection();
    CHECK_EQ(
        runtime.connection_observer.last_connection.state,
        ConnectionState::Ready);

    runtime.replay(tesla_headup::test::fixtures::active_drive, 10ms);
    runtime.refresh(10ms);
    CHECK(runtime.scheduler.tick(40ms));

    CHECK_EQ(runtime.connection_observer.ingest_results.size(), 1U);
    const auto& active = runtime.connection_observer.ingest_results.front();
    CHECK(active.accepted_packet());
    CHECK_EQ(active.decoded_speed_frames, 1);
    CHECK_EQ(active.decoded_turn_signal_frames, 2);
    CHECK_EQ(active.decoded_driver_assistance_frames, 1);
    CHECK_EQ(active.decoded_exterior_light_frames, 3);
    CHECK_EQ(active.decoded_gear_frames, 1);
    CHECK_EQ(active.decoded_speed_limit_frames, 1);
    check_active_frame(runtime.canvas);

    runtime.replay(tesla_headup::test::fixtures::calm_drive, 100ms);
    runtime.refresh(100ms);
    CHECK(runtime.scheduler.tick(100ms));

    CHECK_EQ(runtime.connection_observer.ingest_results.size(), 2U);
    CHECK(runtime.connection_observer.ingest_results.back().accepted_packet());
    CHECK_EQ(runtime.canvas.text.size(), 4U);
    CHECK_EQ(runtime.canvas.text.front(), "42");
    CHECK_EQ(runtime.canvas.text.at(1), "km/h");
    CHECK_EQ(runtime.canvas.text.at(2), "D");
    CHECK_EQ(runtime.canvas.text.back(), "50");
    // Turn intent remains dim through the cancellation hold, then expires.
    CHECK_EQ(runtime.canvas.symbols.size(), 2U);
    CHECK(!runtime.turn_signal_feature.illuminated());
    CHECK(runtime.canvas.edges.empty());
}

TEST_CASE("malformed and late replay packets cannot corrupt the HUD") {
    ReplayRuntime runtime;
    CHECK(runtime.scheduler.tick(0ms));
    runtime.establish_connection();
    runtime.replay(tesla_headup::test::fixtures::active_drive, 10ms);
    runtime.refresh(10ms);
    CHECK(runtime.scheduler.tick(40ms));

    runtime.replay(tesla_headup::test::fixtures::truncated_active_drive(), 50ms);
    runtime.replay(tesla_headup::test::fixtures::late_old_speed, 5ms);
    runtime.refresh(50ms);

    CHECK_EQ(runtime.connection_observer.ingest_results.size(), 3U);
    CHECK(!runtime.connection_observer.ingest_results.at(1).accepted_packet());
    CHECK_EQ(
        runtime.connection_observer.ingest_results.at(1).packet_error,
        tesla_headup::dashkit::protocol::PacketError::TruncatedPayload);
    CHECK(runtime.connection_observer.ingest_results.at(2).accepted_packet());
    CHECK_EQ(
        runtime.connection_observer.ingest_results.at(2).decoded_speed_frames,
        0);
    CHECK(!runtime.scheduler.dirty());
    CHECK(!runtime.scheduler.tick(100ms));
    check_active_frame(runtime.canvas);
}

TEST_CASE("replay silence expires every displayed telemetry feature") {
    ReplayRuntime runtime;
    CHECK(runtime.scheduler.tick(0ms));
    runtime.establish_connection();
    runtime.replay(tesla_headup::test::fixtures::active_drive, 10ms);
    runtime.refresh(10ms);
    CHECK(runtime.scheduler.tick(40ms));
    check_active_frame(runtime.canvas);

    runtime.refresh(511ms);
    CHECK(runtime.scheduler.tick(511ms));

    CHECK_EQ(runtime.canvas.text.size(), 2U);
    CHECK_EQ(runtime.canvas.text.front(), "NO DATA");
    CHECK_EQ(runtime.canvas.text.back(), "50");
    CHECK_EQ(runtime.canvas.symbols.size(), 1U);
    CHECK_EQ(runtime.canvas.symbols.front(), Symbol::Autopilot);
    CHECK(runtime.canvas.edges.empty());

    runtime.refresh(1511ms);
    CHECK(runtime.scheduler.tick(1511ms));
    CHECK_EQ(runtime.canvas.text.size(), 1U);
    CHECK_EQ(runtime.canvas.text.front(), "NO DATA");
    CHECK(runtime.canvas.symbols.empty());
    CHECK(runtime.canvas.edges.empty());
}

}  // namespace
