#pragma once

#include "tesla_headup/application/dashboard/exterior_light_indicators.hpp"
#include "tesla_headup/application/hud_runtime/dashboard_render_scheduler.hpp"
#include "tesla_headup/application/telemetry/telemetry_store.hpp"

namespace tesla_headup::application {

class ExteriorLightPresenter final {
public:
    constexpr ExteriorLightPresenter(
        const TelemetryStore& telemetry,
        ExteriorLightIndicators& feature,
        DashboardRenderScheduler& scheduler) noexcept
        : telemetry_(telemetry), feature_(feature), scheduler_(scheduler) {}

    [[nodiscard]] bool refresh(TelemetryTime now) noexcept;

private:
    const TelemetryStore& telemetry_;
    ExteriorLightIndicators& feature_;
    DashboardRenderScheduler& scheduler_;
};

}  // namespace tesla_headup::application
