#pragma once

#include "tesla_headup/application/dashboard/power_indicator.hpp"
#include "tesla_headup/application/hud_runtime/dashboard_render_scheduler.hpp"
#include "tesla_headup/application/telemetry/telemetry_store.hpp"

namespace tesla_headup::application {

class PowerPresenter final {
public:
    constexpr PowerPresenter(const TelemetryStore& telemetry,
                             PowerIndicator& feature,
                             DashboardRenderScheduler& scheduler) noexcept
        : telemetry_(telemetry), feature_(feature), scheduler_(scheduler) {}

    [[nodiscard]] bool refresh(TelemetryTime now) noexcept {
        const auto value = telemetry_.power_flow(now);
        if (feature_.power() == value) {
            return false;
        }
        feature_.set_power(value);
        scheduler_.request_render();
        return true;
    }

private:
    const TelemetryStore& telemetry_;
    PowerIndicator& feature_;
    DashboardRenderScheduler& scheduler_;
};

}  // namespace tesla_headup::application
