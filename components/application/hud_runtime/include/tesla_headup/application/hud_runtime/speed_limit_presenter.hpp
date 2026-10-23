#pragma once

#include "tesla_headup/application/dashboard/dashboard.hpp"
#include "tesla_headup/application/dashboard/speed_limit_indicator.hpp"
#include "tesla_headup/application/hud_runtime/dashboard_render_scheduler.hpp"
#include "tesla_headup/application/telemetry/telemetry_store.hpp"

namespace tesla_headup::application {

class SpeedLimitPresenter final {
public:
    constexpr SpeedLimitPresenter(
        const TelemetryStore& telemetry,
        SpeedLimitIndicator& feature,
        Dashboard& dashboard,
        DashboardRenderScheduler& scheduler) noexcept
        : telemetry_(telemetry),
          feature_(feature),
          dashboard_(dashboard),
          scheduler_(scheduler) {}

    [[nodiscard]] bool refresh(TelemetryTime now) noexcept;

private:
    const TelemetryStore& telemetry_;
    SpeedLimitIndicator& feature_;
    Dashboard& dashboard_;
    DashboardRenderScheduler& scheduler_;
};

}  // namespace tesla_headup::application
