#pragma once

#include "tesla_headup/application/dashboard/turn_signal_indicators.hpp"
#include "tesla_headup/application/hud_runtime/dashboard_render_scheduler.hpp"
#include "tesla_headup/application/telemetry/telemetry_store.hpp"

namespace tesla_headup::application {

class TurnSignalPresenter final {
public:
    constexpr TurnSignalPresenter(
        const TelemetryStore& telemetry,
        TurnSignalIndicators& feature,
        DashboardRenderScheduler& scheduler) noexcept
        : telemetry_(telemetry), feature_(feature), scheduler_(scheduler) {}

    // Applies both persistent intent (left/right/hazard) and the current lamp
    // phase. A state change requests a coalesced dashboard render.
    [[nodiscard]] bool refresh(TelemetryTime now) noexcept;

private:
    const TelemetryStore& telemetry_;
    TurnSignalIndicators& feature_;
    DashboardRenderScheduler& scheduler_;
};

}  // namespace tesla_headup::application
