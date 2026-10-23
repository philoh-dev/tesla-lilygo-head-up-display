#include "tesla_headup/application/hud_runtime/dashboard_settings_applier.hpp"

namespace tesla_headup::application {

bool apply_dashboard_settings(
    const domain::Settings& settings,
    CenteredVehicleSpeed& speed_feature,
    VehicleSpeedPresenter& speed_presenter,
    SelectedGearPresenter& gear_presenter,
    Dashboard& dashboard,
    DashboardRenderScheduler& scheduler) noexcept {
    speed_feature.set_unit_system(settings.units);
    speed_presenter.set_feature_allowed(
        settings.enabled_features.vehicle_speed);
    gear_presenter.set_feature_allowed(
        settings.enabled_features.selected_gear);

    const bool applied =
        dashboard.set_enabled(
            FeatureId::TurnSignals,
            settings.enabled_features.turn_signals) &&
        dashboard.set_enabled(
            FeatureId::Autopilot,
            settings.enabled_features.autopilot) &&
        dashboard.set_enabled(
            FeatureId::BlindSpotWarnings,
            settings.enabled_features.blind_spot_warnings) &&
        dashboard.set_enabled(FeatureId::SystemStatus, true);
    scheduler.request_render();
    return applied;
}

}  // namespace tesla_headup::application
