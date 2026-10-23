#include "test.hpp"

#include "tesla_headup/adapters/dashkit_tesla/dashkit_status_mapper.hpp"

#include <array>

namespace {

using tesla_headup::application::ConnectivityState;
using tesla_headup::dashkit::BleFailure;
using tesla_headup::dashkit::ConnectionSnapshot;
using tesla_headup::dashkit::ConnectionState;
using tesla_headup::dashkit::map_connectivity;

TEST_CASE("DashKit setup and retry states map to actionable HUD states") {
    constexpr std::array initial_setup_states{
        ConnectionState::Scanning,
        ConnectionState::Connecting,
        ConnectionState::Securing,
        ConnectionState::Discovering,
        ConnectionState::ReadingFirmwareVersion,
        ConnectionState::Subscribing,
    };

    CHECK_EQ(
        map_connectivity({ConnectionState::Idle}),
        ConnectivityState::Disconnected);
    for (const auto state : initial_setup_states) {
        CHECK_EQ(
            map_connectivity({state, 1}),
            ConnectivityState::Connecting);
    }
    CHECK_EQ(
        map_connectivity({ConnectionState::Scanning, 2}),
        ConnectivityState::Reconnecting);
    CHECK_EQ(
        map_connectivity({ConnectionState::WaitingToRetry, 1}),
        ConnectivityState::Reconnecting);
    CHECK_EQ(
        map_connectivity({ConnectionState::Ready, 1}),
        ConnectivityState::Ready);
}

TEST_CASE("pairing-required failure remains distinguishable on the HUD") {
    CHECK_EQ(
        map_connectivity(ConnectionSnapshot{
            ConnectionState::Failed,
            3,
            BleFailure::PairingRequired}),
        ConnectivityState::PairingRequired);
    CHECK_EQ(
        map_connectivity(ConnectionSnapshot{
            ConnectionState::Failed,
            3,
            BleFailure::Timeout}),
        ConnectivityState::Failed);
}

}  // namespace
