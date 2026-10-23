#include "tesla_headup/application/hud_runtime/vehicle_speed_presenter.hpp"

namespace tesla_headup::application {

bool VehicleSpeedPresenter::refresh(TelemetryTime now) noexcept {
    const auto telemetry = telemetry_.vehicle_speed(now);
    const bool enabled = dashboard_.is_enabled(FeatureId::VehicleSpeed);

    if (!feature_allowed_ || !telemetry.fresh()) {
        if (!enabled) {
            return false;
        }
        (void)dashboard_.set_enabled(FeatureId::VehicleSpeed, false);
        scheduler_.request_render();
        return true;
    }

    const auto speed = *telemetry.current_value;
    if (enabled && feature_.speed() == speed) {
        return false;
    }

    feature_.set_speed(speed);
    (void)dashboard_.set_enabled(FeatureId::VehicleSpeed, true);
    scheduler_.request_render();
    return true;
}

void VehicleSpeedPresenter::set_feature_allowed(bool allowed) noexcept {
    feature_allowed_ = allowed;
    if (!allowed && dashboard_.is_enabled(FeatureId::VehicleSpeed)) {
        (void)dashboard_.set_enabled(FeatureId::VehicleSpeed, false);
        scheduler_.request_render();
    }
}

bool VehicleSpeedPresenter::feature_allowed() const noexcept {
    return feature_allowed_;
}

}  // namespace tesla_headup::application
