#include "tesla_headup/application/hud_runtime/blind_spot_presenter.hpp"

namespace tesla_headup::application {

bool BlindSpotPresenter::refresh(TelemetryTime now) noexcept {
    const auto current = telemetry_.blind_spots(now).current_value;
    if (feature_.blind_spots() == current) {
        return false;
    }

    feature_.set_blind_spots(current);
    scheduler_.request_render();
    return true;
}

}  // namespace tesla_headup::application
