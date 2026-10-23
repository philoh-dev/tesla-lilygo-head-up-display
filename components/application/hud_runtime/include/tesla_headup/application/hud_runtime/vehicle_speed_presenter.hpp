#pragma once

#include "tesla_headup/application/dashboard/centered_vehicle_speed.hpp"
#include "tesla_headup/application/dashboard/dashboard.hpp"
#include "tesla_headup/application/hud_runtime/dashboard_render_scheduler.hpp"
#include "tesla_headup/application/telemetry/telemetry_store.hpp"

namespace tesla_headup::application {

class VehicleSpeedPresenter final {
public:
    constexpr VehicleSpeedPresenter(
        const TelemetryStore& telemetry,
        CenteredVehicleSpeed& feature,
        Dashboard& dashboard,
        DashboardRenderScheduler& scheduler) noexcept
        : telemetry_(telemetry),
          feature_(feature),
          dashboard_(dashboard),
          scheduler_(scheduler) {}

    // Applies the latest fresh value to the dashboard. A missing, unavailable,
    // or stale value disables the speed feature instead of showing old data.
    [[nodiscard]] bool refresh(TelemetryTime now) noexcept;
    void set_feature_allowed(bool allowed) noexcept;
    [[nodiscard]] bool feature_allowed() const noexcept;

private:
    const TelemetryStore& telemetry_;
    CenteredVehicleSpeed& feature_;
    Dashboard& dashboard_;
    DashboardRenderScheduler& scheduler_;
    bool feature_allowed_{true};
};

}  // namespace tesla_headup::application
