#include "sdkconfig.h"
#if CONFIG_TESLA_HEADUP_DISPLAY_RGB
#include "tesla_headup/adapters/lvgl_rgb/newhaven_rgb_display.hpp"
#else
#include "tesla_headup/adapters/lvgl_rm67162/rm67162_display.hpp"
#endif
#include "tesla_headup/adapters/esp_nvs_settings/esp_nvs_settings_store.hpp"
#include "tesla_headup/application/hud_runtime/display_self_test_source.hpp"

#if !CONFIG_TESLA_HEADUP_DISPLAY_SELF_TEST
#include "tesla_headup/adapters/dashkit_ble/dashkit_connection.hpp"
#include "tesla_headup/adapters/dashkit_tesla/dashkit_status_mapper.hpp"
#include "tesla_headup/adapters/dashkit_tesla/dashkit_telemetry_ingestor.hpp"
#include "tesla_headup/adapters/esp_diagnostics/esp_log_can_frame_sink.hpp"
#include "tesla_headup/adapters/esp_nimble/esp_nimble_central.hpp"
#endif
#include "tesla_headup/application/dashboard/autopilot_indicator.hpp"
#include "tesla_headup/application/dashboard/blind_spot_warnings.hpp"
#include "tesla_headup/application/dashboard/centered_vehicle_speed.hpp"
#include "tesla_headup/application/dashboard/dashboard.hpp"
#include "tesla_headup/application/dashboard/selected_gear_indicator.hpp"
#include "tesla_headup/application/dashboard/speed_limit_indicator.hpp"
#include "tesla_headup/application/dashboard/system_status_indicator.hpp"
#include "tesla_headup/application/dashboard/turn_signal_indicators.hpp"
#if !CONFIG_TESLA_HEADUP_DISPLAY_SELF_TEST
#include "tesla_headup/application/diagnostics/bounded_can_frame_capture.hpp"
#endif
#include "tesla_headup/application/hud_runtime/autopilot_presenter.hpp"
#include "tesla_headup/application/hud_runtime/blind_spot_presenter.hpp"
#include "tesla_headup/application/hud_runtime/dashboard_render_scheduler.hpp"
#include "tesla_headup/application/hud_runtime/dashboard_settings_applier.hpp"
#include "tesla_headup/application/hud_runtime/selected_gear_presenter.hpp"
#include "tesla_headup/application/hud_runtime/speed_limit_presenter.hpp"
#include "tesla_headup/application/hud_runtime/power_presenter.hpp"
#include "tesla_headup/application/hud_runtime/battery_charge_presenter.hpp"
#include "tesla_headup/application/hud_runtime/system_status_presenter.hpp"
#include "tesla_headup/application/hud_runtime/turn_signal_presenter.hpp"
#include "tesla_headup/application/hud_runtime/vehicle_speed_presenter.hpp"
#include "tesla_headup/application/settings/settings_service.hpp"
#include "tesla_headup/application/telemetry/telemetry_store.hpp"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include <chrono>
#include <cstdint>
#include <span>

namespace {

constexpr char tag[] = "tesla_headup";
constexpr auto event_loop_period = 10U;
constexpr bool self_test_projection_mirrored = true;
constexpr std::uint8_t self_test_brightness_percent = 100;

tesla_headup::application::TelemetryTime now() {
    return tesla_headup::application::TelemetryTime{esp_timer_get_time() / 1'000};
}

#if !CONFIG_TESLA_HEADUP_DISPLAY_SELF_TEST
class FirmwareConnectionObserver final
    : public tesla_headup::dashkit::DashKitConnectionObserver {
public:
    explicit FirmwareConnectionObserver(
        tesla_headup::dashkit::DashKitTelemetryIngestor& telemetry_ingestor) noexcept
        : telemetry_ingestor_(telemetry_ingestor) {}

    void attach_status_presenter(
        tesla_headup::application::SystemStatusPresenter& presenter) noexcept {
        status_presenter_ = &presenter;
        (void)status_presenter_->set_connectivity(
            tesla_headup::dashkit::map_connectivity(last_snapshot_), now());
    }

    void on_connection_state_changed(
        tesla_headup::dashkit::ConnectionSnapshot snapshot) override {
        last_snapshot_ = snapshot;
        if (status_presenter_ != nullptr) {
            (void)status_presenter_->set_connectivity(
                tesla_headup::dashkit::map_connectivity(snapshot), now());
        }
        ESP_LOGI(
            tag,
            "DashKit connection state=%u attempt=%u failure=%u",
            static_cast<unsigned>(snapshot.state),
            static_cast<unsigned>(snapshot.attempt),
            static_cast<unsigned>(snapshot.last_failure));
    }

    void on_can_payload(
        std::span<const std::uint8_t> payload,
        tesla_headup::dashkit::MonotonicTime received_at) override {
        ++received_packet_count_;
        const auto result = telemetry_ingestor_.ingest(payload, received_at);
        if (received_packet_count_ == 1) {
            ESP_LOGI(
                tag,
                "Received first DashKit CAN packet (%u bytes at %lld ms)",
                static_cast<unsigned>(payload.size()),
                static_cast<long long>(received_at.count()));
        }
        if (!result.accepted_packet()) {
            ESP_LOGW(
                tag,
                "Rejected malformed DashKit packet: error=%u bytes=%u",
                static_cast<unsigned>(result.packet_error),
                static_cast<unsigned>(payload.size()));
        } else if (status_presenter_ != nullptr) {
            (void)status_presenter_->on_telemetry_received(received_at);
        }
    }

private:
    tesla_headup::dashkit::DashKitTelemetryIngestor& telemetry_ingestor_;
    tesla_headup::application::SystemStatusPresenter* status_presenter_{nullptr};
    tesla_headup::dashkit::ConnectionSnapshot last_snapshot_{};
    std::uint32_t received_packet_count_{0};
};
#endif

void initialize_nvs() {
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES ||
        result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(tag, "NVS requires reinitialization; stored BLE bonds are lost");
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);
}

}  // namespace

extern "C" void app_main() {
    ESP_LOGI(tag, "Tesla Head-Up Display starting");
    initialize_nvs();

    static tesla_headup::platform::EspNvsSettingsStore settings_store;
    static tesla_headup::application::SettingsService settings_service{settings_store};
    const auto loaded_settings = settings_service.load();
    const auto& settings = loaded_settings.settings;
    auto runtime_settings = settings;
#if CONFIG_TESLA_HEADUP_DISPLAY_SELF_TEST
    runtime_settings.units = tesla_headup::domain::UnitSystem::Metric;
    runtime_settings.enabled_features = {};
#endif
    if (loaded_settings.status ==
            tesla_headup::application::LoadSettingsStatus::NotFound ||
        loaded_settings.status ==
            tesla_headup::application::LoadSettingsStatus::InvalidData) {
        if (settings_service.save(settings) !=
            tesla_headup::application::SaveSettingsStatus::Saved) {
            ESP_LOGW(tag, "Could not persist default settings");
        }
    } else if (loaded_settings.status ==
               tesla_headup::application::LoadSettingsStatus::StorageError) {
        ESP_LOGW(tag, "Could not load settings; using defaults for this boot");
    }

    static tesla_headup::application::TelemetryStore telemetry;
#if !CONFIG_TESLA_HEADUP_DISPLAY_SELF_TEST
    // Static storage keeps the bounded BLE event queue out of app_main's task
    // stack and guarantees that the platform port outlives the connection.
    static tesla_headup::platform::EspNimbleCentral ble_central;
    ESP_ERROR_CHECK(ble_central.initialize());

#if CONFIG_TESLA_HEADUP_CAN_DIAGNOSTIC_CAPTURE
    static tesla_headup::platform::EspLogCanFrameSink diagnostic_sink;
    static tesla_headup::application::BoundedCanFrameCapture diagnostic_capture{
        diagnostic_sink,
        {
            .maximum_frames_per_window =
                CONFIG_TESLA_HEADUP_CAN_DIAGNOSTIC_FRAMES_PER_SECOND,
            .window_duration = std::chrono::seconds{1},
        }};
    static tesla_headup::dashkit::DashKitTelemetryIngestor telemetry_ingestor{
        telemetry, &diagnostic_capture};
#else
    static tesla_headup::dashkit::DashKitTelemetryIngestor telemetry_ingestor{
        telemetry};
#endif
    static tesla_headup::dashkit::DashKitConnection dashkit_connection{ble_central};
    static FirmwareConnectionObserver connection_observer{telemetry_ingestor};
    dashkit_connection.set_observer(&connection_observer);

    // Start immediately. If the NimBLE controller is still synchronizing, the
    // adapter remembers the request and begins scanning from its sync callback.
    dashkit_connection.start(now());
#else
    ESP_LOGW(tag, "Display self-test active; DashKit BLE is disabled");
#endif

#if CONFIG_TESLA_HEADUP_DISPLAY_RGB
    static tesla_headup::platform::NewhavenRgbDisplay display;
#else
    static tesla_headup::platform::Rm67162Display display;
#endif
#if CONFIG_TESLA_HEADUP_DISPLAY_SELF_TEST
    display.set_projection_mirrored(self_test_projection_mirrored);
    ESP_ERROR_CHECK(
        display.set_brightness_percent(self_test_brightness_percent));
#else
    display.set_projection_mirrored(runtime_settings.projection_mirrored);
    ESP_ERROR_CHECK(display.set_brightness_percent(
        runtime_settings.brightness.percent()));
#endif
    ESP_ERROR_CHECK(display.initialize());

    static tesla_headup::application::BlindSpotWarnings blind_spot_feature;
    static tesla_headup::application::CenteredVehicleSpeed speed_feature;
    static tesla_headup::application::TurnSignalIndicators turn_signal_feature;
    static tesla_headup::application::AutopilotIndicator autopilot_feature;
    static tesla_headup::application::SystemStatusIndicator system_status_feature;
    static tesla_headup::application::SelectedGearIndicator selected_gear_feature;
    static tesla_headup::application::SpeedLimitIndicator speed_limit_feature;
    static tesla_headup::application::PowerIndicator power_feature;
    static tesla_headup::application::BatteryChargeIndicator battery_charge_feature;
    static tesla_headup::application::Dashboard dashboard{display.canvas()};
    ESP_ERROR_CHECK(
        dashboard.add_feature(blind_spot_feature) &&
                dashboard.add_feature(speed_feature, false) &&
                dashboard.add_feature(turn_signal_feature) &&
                dashboard.add_feature(autopilot_feature) &&
                dashboard.add_feature(system_status_feature) &&
                dashboard.add_feature(selected_gear_feature, false) &&
                dashboard.add_feature(speed_limit_feature, false) &&
                dashboard.add_feature(power_feature) &&
                dashboard.add_feature(battery_charge_feature)
            ? ESP_OK
            : ESP_ERR_NO_MEM);
    static tesla_headup::application::DashboardRenderScheduler render_scheduler{
        dashboard};
    static tesla_headup::application::VehicleSpeedPresenter speed_presenter{
        telemetry,
        speed_feature,
        dashboard,
        render_scheduler};
    static tesla_headup::application::TurnSignalPresenter turn_signal_presenter{
        telemetry,
        turn_signal_feature,
        render_scheduler};
    static tesla_headup::application::BlindSpotPresenter blind_spot_presenter{
        telemetry,
        blind_spot_feature,
        render_scheduler};
    static tesla_headup::application::AutopilotPresenter autopilot_presenter{
        telemetry,
        autopilot_feature,
        render_scheduler};
    static tesla_headup::application::SystemStatusPresenter system_status_presenter{
        system_status_feature,
        render_scheduler};
    static tesla_headup::application::SelectedGearPresenter selected_gear_presenter{
        telemetry,
        selected_gear_feature,
        dashboard,
        render_scheduler};
    static tesla_headup::application::SpeedLimitPresenter speed_limit_presenter{
        telemetry,
        speed_limit_feature,
        dashboard,
        render_scheduler};
    static tesla_headup::application::PowerPresenter power_presenter{
        telemetry, power_feature, render_scheduler};
    static tesla_headup::application::BatteryChargePresenter battery_charge_presenter{
        telemetry, battery_charge_feature, render_scheduler};
    ESP_ERROR_CHECK(tesla_headup::application::apply_dashboard_settings(
                        runtime_settings,
                        speed_feature,
                        speed_presenter,
                        selected_gear_presenter,
                        dashboard,
                        render_scheduler)
                        ? ESP_OK
                        : ESP_ERR_INVALID_STATE);
#if CONFIG_TESLA_HEADUP_DISPLAY_SELF_TEST
    static tesla_headup::application::DisplaySelfTestSource self_test_source{
        telemetry};
    const auto initial_time = now();
    (void)system_status_presenter.set_connectivity(
        tesla_headup::application::ConnectivityState::Ready, initial_time);
    if (self_test_source.tick(initial_time)) {
        (void)system_status_presenter.on_telemetry_received(initial_time);
    }
#else
    connection_observer.attach_status_presenter(system_status_presenter);
#endif
    (void)render_scheduler.tick(now());

    while (true) {
        auto loop_now = now();
#if CONFIG_TESLA_HEADUP_DISPLAY_SELF_TEST
        if (self_test_source.tick(loop_now)) {
            (void)system_status_presenter.on_telemetry_received(loop_now);
        }
#else
        ble_central.process_events();
        loop_now = now();
        dashkit_connection.tick(loop_now);
#endif
        (void)speed_presenter.refresh(loop_now);
        (void)turn_signal_presenter.refresh(loop_now);
        (void)blind_spot_presenter.refresh(loop_now);
        (void)autopilot_presenter.refresh(loop_now);
        (void)system_status_presenter.refresh(loop_now);
        (void)selected_gear_presenter.refresh(loop_now);
        (void)speed_limit_presenter.refresh(loop_now);
        (void)power_presenter.refresh(loop_now);
        (void)battery_charge_presenter.refresh(loop_now);
        (void)render_scheduler.tick(loop_now);
        display.process();
        vTaskDelay(pdMS_TO_TICKS(event_loop_period));
    }
}
