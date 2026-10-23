#include "tesla_headup/application/hud_runtime/system_status_presenter.hpp"

namespace tesla_headup::application {

bool SystemStatusPresenter::set_connectivity(
    ConnectivityState state,
    TelemetryTime now) noexcept {
    if (connectivity_ == state) {
        return apply(now);
    }

    connectivity_ = state;
    if (state == ConnectivityState::Ready) {
        last_telemetry_at_.reset();
    }
    return apply(now);
}

bool SystemStatusPresenter::on_telemetry_received(TelemetryTime now) noexcept {
    if (connectivity_ != ConnectivityState::Ready) {
        return false;
    }
    if (last_telemetry_at_.has_value() && now < *last_telemetry_at_) {
        return false;
    }
    last_telemetry_at_ = now;
    return apply(now);
}

bool SystemStatusPresenter::refresh(TelemetryTime now) noexcept {
    return apply(now);
}

SystemStatus SystemStatusPresenter::desired_status(
    TelemetryTime now) const noexcept {
    switch (connectivity_) {
    case ConnectivityState::Disconnected:
        return SystemStatus::Disconnected;
    case ConnectivityState::Connecting:
        return SystemStatus::Connecting;
    case ConnectivityState::Reconnecting:
        return SystemStatus::Reconnecting;
    case ConnectivityState::PairingRequired:
        return SystemStatus::PairingRequired;
    case ConnectivityState::Failed:
        return SystemStatus::ConnectionFailed;
    case ConnectivityState::Ready:
        break;
    }

    if (!last_telemetry_at_.has_value()) {
        return SystemStatus::WaitingForData;
    }
    const auto timeout = policy_.telemetry_timeout < TelemetryTime::zero()
                             ? TelemetryTime::zero()
                             : policy_.telemetry_timeout;
    if (now < *last_telemetry_at_ ||
        (now - *last_telemetry_at_) <= timeout) {
        return SystemStatus::Hidden;
    }
    return SystemStatus::NoData;
}

bool SystemStatusPresenter::apply(TelemetryTime now) noexcept {
    const auto desired = desired_status(now);
    if (feature_.status() == desired) {
        return false;
    }
    feature_.set_status(desired);
    scheduler_.request_render();
    return true;
}

}  // namespace tesla_headup::application
