#include "tesla_headup/application/hud_runtime/turn_signal_presenter.hpp"

namespace tesla_headup::application {

bool TurnSignalPresenter::refresh(TelemetryTime now) noexcept {
    const auto current = telemetry_.turn_signals(now);
    if (feature_.signals() == current.signals &&
        feature_.illuminated() == current.illuminated) {
        return false;
    }

    feature_.set_signals(current.signals);
    feature_.set_illuminated(current.illuminated);
    scheduler_.request_render();
    return true;
}

}  // namespace tesla_headup::application
