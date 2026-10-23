#pragma once

#include "tesla_headup/application/dashboard/blind_spot_warnings.hpp"
#include "tesla_headup/application/hud_runtime/dashboard_render_scheduler.hpp"
#include "tesla_headup/application/telemetry/telemetry_store.hpp"

namespace tesla_headup::application {

class BlindSpotPresenter final {
public:
    constexpr BlindSpotPresenter(
        const TelemetryStore& telemetry,
        BlindSpotWarnings& feature,
        DashboardRenderScheduler& scheduler) noexcept
        : telemetry_(telemetry), feature_(feature), scheduler_(scheduler) {}

    [[nodiscard]] bool refresh(TelemetryTime now) noexcept;

private:
    const TelemetryStore& telemetry_;
    BlindSpotWarnings& feature_;
    DashboardRenderScheduler& scheduler_;
};

}  // namespace tesla_headup::application
