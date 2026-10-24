#include "test.hpp"

#include "tesla_headup/application/dashboard/autopilot_indicator.hpp"
#include "tesla_headup/application/dashboard/blind_spot_warnings.hpp"
#include "tesla_headup/application/dashboard/centered_vehicle_speed.hpp"
#include "tesla_headup/application/dashboard/dashboard.hpp"
#include "tesla_headup/application/dashboard/system_status_indicator.hpp"
#include "tesla_headup/application/dashboard/selected_gear_indicator.hpp"
#include "tesla_headup/application/dashboard/turn_signal_indicators.hpp"
#include "tesla_headup/application/hud_runtime/dashboard_render_scheduler.hpp"
#include "tesla_headup/application/hud_runtime/dashboard_settings_applier.hpp"
#include "tesla_headup/application/hud_runtime/vehicle_speed_presenter.hpp"
#include "tesla_headup/application/hud_runtime/selected_gear_presenter.hpp"
#include "tesla_headup/application/telemetry/telemetry_store.hpp"
#include "tesla_headup/domain/settings.hpp"

namespace {

class NullCanvas final : public tesla_headup::application::DashboardCanvas {
public:
    void begin_frame() override {}
    void draw_text(const tesla_headup::application::TextElement&) override {}
    void draw_symbol(const tesla_headup::application::SymbolElement&) override {}
    void draw_edge_gradient(
        const tesla_headup::application::EdgeGradientElement&) override {}
    void end_frame() override {}
};

struct SettingsRuntime {
    NullCanvas canvas;
    tesla_headup::application::TelemetryStore telemetry;
    tesla_headup::application::BlindSpotWarnings blind_spot;
    tesla_headup::application::CenteredVehicleSpeed speed;
    tesla_headup::application::TurnSignalIndicators turn_signals;
    tesla_headup::application::AutopilotIndicator autopilot;
    tesla_headup::application::SystemStatusIndicator status;
    tesla_headup::application::SelectedGearIndicator gear;
    tesla_headup::application::Dashboard dashboard{canvas};
    tesla_headup::application::DashboardRenderScheduler scheduler{dashboard};
    tesla_headup::application::VehicleSpeedPresenter speed_presenter{
        telemetry, speed, dashboard, scheduler};
    tesla_headup::application::SelectedGearPresenter gear_presenter{
        telemetry, gear, dashboard, scheduler};

    SettingsRuntime() {
        CHECK(dashboard.add_feature(blind_spot));
        CHECK(dashboard.add_feature(speed, false));
        CHECK(dashboard.add_feature(turn_signals));
        CHECK(dashboard.add_feature(autopilot));
        CHECK(dashboard.add_feature(status));
        CHECK(dashboard.add_feature(gear, false));
    }
};

TEST_CASE("dashboard settings map onto every registered configurable feature") {
    SettingsRuntime runtime;
    tesla_headup::domain::Settings settings;
    settings.units = tesla_headup::domain::UnitSystem::Imperial;
    settings.enabled_features.vehicle_speed = false;
    settings.enabled_features.turn_signals = false;
    settings.enabled_features.autopilot = false;
    settings.enabled_features.blind_spot_warnings = false;
    settings.enabled_features.selected_gear = false;

    CHECK(tesla_headup::application::apply_dashboard_settings(
        settings,
        runtime.speed,
        runtime.speed_presenter,
        runtime.gear_presenter,
        runtime.dashboard,
        runtime.scheduler));

    CHECK_EQ(runtime.speed.unit_system(), tesla_headup::domain::UnitSystem::Imperial);
    CHECK(!runtime.speed_presenter.feature_allowed());
    CHECK(!runtime.dashboard.is_enabled(
        tesla_headup::application::FeatureId::VehicleSpeed));
    CHECK(!runtime.dashboard.is_enabled(
        tesla_headup::application::FeatureId::TurnSignals));
    CHECK(!runtime.dashboard.is_enabled(
        tesla_headup::application::FeatureId::Autopilot));
    CHECK(!runtime.dashboard.is_enabled(
        tesla_headup::application::FeatureId::BlindSpotWarnings));
    CHECK(runtime.dashboard.is_enabled(
        tesla_headup::application::FeatureId::SystemStatus));
    CHECK(!runtime.gear_presenter.feature_allowed());
    CHECK(runtime.scheduler.dirty());
}

TEST_CASE("re-enabling dashboard settings preserves speed freshness gating") {
    SettingsRuntime runtime;
    tesla_headup::domain::Settings disabled;
    disabled.enabled_features.vehicle_speed = false;
    CHECK(tesla_headup::application::apply_dashboard_settings(
        disabled,
        runtime.speed,
        runtime.speed_presenter,
        runtime.gear_presenter,
        runtime.dashboard,
        runtime.scheduler));

    const tesla_headup::domain::Settings enabled;
    CHECK(tesla_headup::application::apply_dashboard_settings(
        enabled,
        runtime.speed,
        runtime.speed_presenter,
        runtime.gear_presenter,
        runtime.dashboard,
        runtime.scheduler));

    CHECK(runtime.speed_presenter.feature_allowed());
    CHECK(!runtime.dashboard.is_enabled(
        tesla_headup::application::FeatureId::VehicleSpeed));
    CHECK(runtime.dashboard.is_enabled(
        tesla_headup::application::FeatureId::TurnSignals));
    CHECK(runtime.dashboard.is_enabled(
        tesla_headup::application::FeatureId::Autopilot));
    CHECK(runtime.dashboard.is_enabled(
        tesla_headup::application::FeatureId::BlindSpotWarnings));
    CHECK(runtime.gear_presenter.feature_allowed());
}

}  // namespace
