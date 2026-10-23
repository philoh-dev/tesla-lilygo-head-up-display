#pragma once

#include "tesla_headup/application/dashboard/battery_charge_indicator.hpp"
#include "tesla_headup/application/hud_runtime/dashboard_render_scheduler.hpp"
#include "tesla_headup/application/telemetry/telemetry_store.hpp"

namespace tesla_headup::application {
class BatteryChargePresenter final {
public:
    constexpr BatteryChargePresenter(const TelemetryStore& telemetry,
        BatteryChargeIndicator& feature, DashboardRenderScheduler& scheduler) noexcept
        : telemetry_(telemetry), feature_(feature), scheduler_(scheduler) {}
    [[nodiscard]] bool refresh(TelemetryTime now) noexcept {
        const auto value = telemetry_.battery_charge(now);
        if (feature_.charge() == value) return false;
        feature_.set_charge(value);
        scheduler_.request_render();
        return true;
    }
private:
    const TelemetryStore& telemetry_;
    BatteryChargeIndicator& feature_;
    DashboardRenderScheduler& scheduler_;
};
}  // namespace tesla_headup::application
