#include "tesla_headup/application/hud_runtime/speed_limit_presenter.hpp"

namespace tesla_headup::application {

bool SpeedLimitPresenter::refresh(TelemetryTime now) noexcept {
    const auto telemetry = telemetry_.speed_limit(now);
    const bool visible = telemetry.fresh();
    const bool visibility_changed =
        dashboard_.is_enabled(FeatureId::SpeedLimit) != visible;
    const bool value_changed =
        visible && feature_.speed_limit() != *telemetry.current_value;
    if (!visibility_changed && !value_changed) {
        return false;
    }
    if (value_changed) {
        feature_.set_speed_limit(*telemetry.current_value);
    }
    (void)dashboard_.set_enabled(FeatureId::SpeedLimit, visible);
    scheduler_.request_render();
    return true;
}

}  // namespace tesla_headup::application
