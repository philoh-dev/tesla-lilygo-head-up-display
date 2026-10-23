#include "tesla_headup/application/hud_runtime/exterior_light_presenter.hpp"

namespace tesla_headup::application {

bool ExteriorLightPresenter::refresh(TelemetryTime now) noexcept {
    const auto current = telemetry_.exterior_lights(now).current_value;
    if (feature_.lights() == current) {
        return false;
    }

    feature_.set_lights(current);
    scheduler_.request_render();
    return true;
}

}  // namespace tesla_headup::application
