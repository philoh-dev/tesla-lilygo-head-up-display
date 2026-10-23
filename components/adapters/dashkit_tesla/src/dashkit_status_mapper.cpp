#include "tesla_headup/adapters/dashkit_tesla/dashkit_status_mapper.hpp"

namespace tesla_headup::dashkit {

application::ConnectivityState map_connectivity(
    ConnectionSnapshot snapshot) noexcept {
    using application::ConnectivityState;
    switch (snapshot.state) {
    case ConnectionState::Idle:
        return ConnectivityState::Disconnected;
    case ConnectionState::Scanning:
    case ConnectionState::Connecting:
    case ConnectionState::Securing:
    case ConnectionState::Discovering:
    case ConnectionState::ReadingFirmwareVersion:
    case ConnectionState::Subscribing:
        return snapshot.attempt <= 1 ? ConnectivityState::Connecting
                                     : ConnectivityState::Reconnecting;
    case ConnectionState::Ready:
        return ConnectivityState::Ready;
    case ConnectionState::WaitingToRetry:
        return ConnectivityState::Reconnecting;
    case ConnectionState::Failed:
        return snapshot.last_failure == BleFailure::PairingRequired
                   ? ConnectivityState::PairingRequired
                   : ConnectivityState::Failed;
    }
    return ConnectivityState::Failed;
}

}  // namespace tesla_headup::dashkit
