#pragma once

#include "tesla_headup/application/dashboard/centered_vehicle_speed.hpp"
#include "tesla_headup/application/dashboard/dashboard.hpp"
#include "tesla_headup/application/hud_runtime/dashboard_render_scheduler.hpp"
#include "tesla_headup/application/hud_runtime/vehicle_speed_presenter.hpp"
#include "tesla_headup/application/hud_runtime/selected_gear_presenter.hpp"
#include "tesla_headup/domain/settings.hpp"

namespace tesla_headup::application {

[[nodiscard]] bool apply_dashboard_settings(
    const domain::Settings& settings,
    CenteredVehicleSpeed& speed_feature,
    VehicleSpeedPresenter& speed_presenter,
    SelectedGearPresenter& gear_presenter,
    Dashboard& dashboard,
    DashboardRenderScheduler& scheduler) noexcept;

}  // namespace tesla_headup::application
