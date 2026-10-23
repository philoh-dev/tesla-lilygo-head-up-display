#include "tesla_headup/application/hud_runtime/selected_gear_presenter.hpp"

namespace tesla_headup::application {

bool SelectedGearPresenter::refresh(TelemetryTime now) noexcept {
    const auto telemetry = telemetry_.selected_gear(now);
    const bool visible = feature_allowed_ && telemetry.fresh();
    const bool visibility_changed =
        dashboard_.is_enabled(FeatureId::SelectedGear) != visible;
    const bool value_changed =
        visible && feature_.gear() != *telemetry.current_value;
    if (!visibility_changed && !value_changed) {
        return false;
    }
    if (value_changed) {
        feature_.set_gear(*telemetry.current_value);
    }
    (void)dashboard_.set_enabled(FeatureId::SelectedGear, visible);
    scheduler_.request_render();
    return true;
}

void SelectedGearPresenter::set_feature_allowed(bool allowed) noexcept {
    feature_allowed_ = allowed;
    if (!allowed && dashboard_.is_enabled(FeatureId::SelectedGear)) {
        (void)dashboard_.set_enabled(FeatureId::SelectedGear, false);
        scheduler_.request_render();
    }
}

bool SelectedGearPresenter::feature_allowed() const noexcept {
    return feature_allowed_;
}

}  // namespace tesla_headup::application
