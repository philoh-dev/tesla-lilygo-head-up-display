#include "tesla_headup/application/hud_runtime/autopilot_presenter.hpp"

namespace tesla_headup::application {

bool AutopilotPresenter::refresh(TelemetryTime now) noexcept {
    const bool illuminated = telemetry_.autopilot(now).active();
    if (feature_.illuminated() == illuminated) {
        return false;
    }

    feature_.set_illuminated(illuminated);
    scheduler_.request_render();
    return true;
}

}  // namespace tesla_headup::application
