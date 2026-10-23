#pragma once

#include "tesla_headup/application/dashboard/dashboard.hpp"
#include "tesla_headup/application/dashboard/selected_gear_indicator.hpp"
#include "tesla_headup/application/hud_runtime/dashboard_render_scheduler.hpp"
#include "tesla_headup/application/telemetry/telemetry_store.hpp"

namespace tesla_headup::application {

class SelectedGearPresenter final {
public:
    constexpr SelectedGearPresenter(
        const TelemetryStore& telemetry,
        SelectedGearIndicator& feature,
        Dashboard& dashboard,
        DashboardRenderScheduler& scheduler) noexcept
        : telemetry_(telemetry),
          feature_(feature),
          dashboard_(dashboard),
          scheduler_(scheduler) {}

    [[nodiscard]] bool refresh(TelemetryTime now) noexcept;
    void set_feature_allowed(bool allowed) noexcept;
    [[nodiscard]] bool feature_allowed() const noexcept;

private:
    const TelemetryStore& telemetry_;
    SelectedGearIndicator& feature_;
    Dashboard& dashboard_;
    DashboardRenderScheduler& scheduler_;
    bool feature_allowed_{true};
};

}  // namespace tesla_headup::application
