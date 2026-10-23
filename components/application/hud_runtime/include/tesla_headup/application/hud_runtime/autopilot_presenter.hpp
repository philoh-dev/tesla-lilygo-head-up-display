#pragma once

#include "tesla_headup/application/dashboard/autopilot_indicator.hpp"
#include "tesla_headup/application/hud_runtime/dashboard_render_scheduler.hpp"
#include "tesla_headup/application/telemetry/telemetry_store.hpp"

namespace tesla_headup::application {

class AutopilotPresenter final {
public:
    constexpr AutopilotPresenter(
        const TelemetryStore& telemetry,
        AutopilotIndicator& feature,
        DashboardRenderScheduler& scheduler) noexcept
        : telemetry_(telemetry), feature_(feature), scheduler_(scheduler) {}

    [[nodiscard]] bool refresh(TelemetryTime now) noexcept;

private:
    const TelemetryStore& telemetry_;
    AutopilotIndicator& feature_;
    DashboardRenderScheduler& scheduler_;
};

}  // namespace tesla_headup::application
