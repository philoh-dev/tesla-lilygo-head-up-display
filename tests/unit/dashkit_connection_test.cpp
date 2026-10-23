#include "test.hpp"

#include "tesla_headup/adapters/dashkit_ble/dashkit_connection.hpp"
#include "tesla_headup/adapters/dashkit_ble/dashkit_profile.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace std::chrono_literals;
using tesla_headup::dashkit::BleCentral;
using tesla_headup::dashkit::BleCentralObserver;
using tesla_headup::dashkit::BleDeviceId;
using tesla_headup::dashkit::BleFailure;
using tesla_headup::dashkit::ConnectionSnapshot;
using tesla_headup::dashkit::ConnectionState;
using tesla_headup::dashkit::DashKitConnection;
using tesla_headup::dashkit::DashKitConnectionObserver;
using tesla_headup::dashkit::DiscoveredDevice;
using tesla_headup::dashkit::MonotonicTime;

class FakeBleCentral final : public BleCentral {
public:
    void set_observer(BleCentralObserver* value) override { observer = value; }

    bool scan_for_name(std::string_view name) override {
        ++scan_calls;
        scanned_name = name;
        return accept_scan;
    }

    void stop_scan() override { ++stop_scan_calls; }

    bool connect(BleDeviceId id) override {
        ++connect_calls;
        connected_id = id;
        return accept_connect;
    }

    bool secure_connection() override {
        ++secure_calls;
        return accept_secure;
    }

    bool discover_service(std::string_view uuid) override {
        ++discover_calls;
        discovered_service = uuid;
        return accept_discovery;
    }

    bool subscribe(
        std::string_view service_uuid,
        std::string_view characteristic_uuid) override {
        ++subscribe_calls;
        subscribed_service = service_uuid;
        subscribed_characteristic = characteristic_uuid;
        return accept_subscription;
    }

    bool read_firmware_version() override {
        ++version_read_calls;
        return accept_version_read;
    }

    bool send_keepalive() override {
        ++keepalive_calls;
        return accept_keepalive;
    }

    bool remove_peer_bond() override {
        ++remove_bond_calls;
        return remove_bond_result;
    }

    void disconnect() override { ++disconnect_calls; }

    BleCentralObserver* observer{nullptr};
    bool accept_scan{true};
    bool accept_connect{true};
    bool accept_secure{true};
    bool accept_discovery{true};
    bool accept_subscription{true};
    bool accept_version_read{true};
    bool accept_keepalive{true};
    bool remove_bond_result{false};
    int scan_calls{0};
    int stop_scan_calls{0};
    int connect_calls{0};
    int secure_calls{0};
    int discover_calls{0};
    int subscribe_calls{0};
    int version_read_calls{0};
    int keepalive_calls{0};
    int remove_bond_calls{0};
    int disconnect_calls{0};
    std::string scanned_name;
    BleDeviceId connected_id;
    std::string discovered_service;
    std::string subscribed_service;
    std::string subscribed_characteristic;
};

constexpr BleDeviceId dashkit_id() {
    return {{{0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6}}, 1};
}

constexpr std::array<std::uint8_t, 5> current_version{'0', '.', '1', '.', '2'};

class RecordingObserver final : public DashKitConnectionObserver {
public:
    void on_connection_state_changed(ConnectionSnapshot value) override {
        states.push_back(value);
    }

    void on_can_payload(
        std::span<const std::uint8_t> payload,
        MonotonicTime received_at) override {
        payloads.emplace_back(payload.begin(), payload.end());
        received_times.push_back(received_at);
    }

    std::vector<ConnectionSnapshot> states;
    std::vector<std::vector<std::uint8_t>> payloads;
    std::vector<MonotonicTime> received_times;
};

void reach_ready(
    DashKitConnection& connection,
    FakeBleCentral& central,
    MonotonicTime start = 0ms) {
    connection.start(start);
    central.observer->on_device_found({dashkit_id(), "DashKit"}, start + 1ms);
    central.observer->on_connected(start + 2ms);
    central.observer->on_secured(start + 3ms);
    central.observer->on_service_discovered(true, start + 4ms);
    central.observer->on_firmware_version_read(
        true, current_version, start + 5ms);
    central.observer->on_subscription_changed(true, start + 6ms);
    central.observer->on_keepalive_written(true, start + 7ms);
}

TEST_CASE("startup immediately dispatches exactly one DashKit scan") {
    FakeBleCentral central;
    DashKitConnection connection{central};

    connection.start(42ms);
    connection.start(43ms);

    CHECK_EQ(connection.snapshot().state, ConnectionState::Scanning);
    CHECK_EQ(connection.snapshot().attempt, 1);
    CHECK_EQ(central.scan_calls, 1);
    CHECK_EQ(central.scanned_name, "DashKit");
}

TEST_CASE("connects, reads firmware, subscribes, and starts keepalive") {
    FakeBleCentral central;
    DashKitConnection connection{central};
    RecordingObserver observer;
    connection.set_observer(&observer);

    reach_ready(connection, central, 100ms);

    CHECK_EQ(connection.snapshot().state, ConnectionState::Ready);
    CHECK_EQ(connection.snapshot().attempt, 1);
    CHECK_EQ(central.scanned_name, "DashKit");
    CHECK_EQ(central.connected_id, dashkit_id());
    CHECK_EQ(central.secure_calls, 1);
    CHECK_EQ(central.version_read_calls, 1);
    CHECK_EQ(
        central.discovered_service,
        tesla_headup::dashkit::profile::can_service_uuid);
    CHECK_EQ(
        central.subscribed_characteristic,
        tesla_headup::dashkit::profile::can_characteristic_uuid);
    CHECK_EQ(central.keepalive_calls, 1);
    CHECK_EQ(connection.snapshot().firmware_version.major, 0);
    CHECK_EQ(connection.snapshot().firmware_version.minor, 1);
    CHECK_EQ(connection.snapshot().firmware_version.patch, 2);
    CHECK(connection.snapshot().firmware_version.known);
    CHECK(connection.snapshot().keepalive_enabled);
}

TEST_CASE("ignores advertisements that are not DashKit") {
    FakeBleCentral central;
    DashKitConnection connection{central};

    connection.start(0ms);
    central.observer->on_device_found({dashkit_id(), "OtherDevice"}, 1ms);

    CHECK_EQ(connection.snapshot().state, ConnectionState::Scanning);
    CHECK_EQ(central.connect_calls, 0);
    CHECK_EQ(central.stop_scan_calls, 0);
}

TEST_CASE("retries scan timeouts and fails after the configured budget") {
    FakeBleCentral central;
    tesla_headup::dashkit::ConnectionPolicy policy;
    policy.max_attempts = 3;
    policy.scan_timeout = 10ms;
    policy.retry_delay = 2ms;
    DashKitConnection connection{central, policy};

    connection.start(0ms);
    connection.tick(10ms);
    CHECK_EQ(connection.snapshot().state, ConnectionState::WaitingToRetry);
    connection.tick(12ms);
    CHECK_EQ(connection.snapshot().attempt, 2);
    connection.tick(22ms);
    connection.tick(24ms);
    CHECK_EQ(connection.snapshot().attempt, 3);
    connection.tick(34ms);

    CHECK_EQ(connection.snapshot().state, ConnectionState::Failed);
    CHECK_EQ(connection.snapshot().last_failure, BleFailure::Timeout);
    CHECK_EQ(central.scan_calls, 3);
    CHECK_EQ(central.stop_scan_calls, 3);

    connection.start(40ms);
    CHECK_EQ(connection.snapshot().state, ConnectionState::Scanning);
    CHECK_EQ(connection.snapshot().attempt, 1);
    CHECK_EQ(central.scan_calls, 4);
}

TEST_CASE("a healthy link gets a fresh reconnect budget after link loss") {
    FakeBleCentral central;
    tesla_headup::dashkit::ConnectionPolicy policy;
    policy.retry_delay = 5ms;
    DashKitConnection connection{central, policy};
    reach_ready(connection, central);

    central.observer->on_disconnected(20ms);

    CHECK_EQ(connection.snapshot().state, ConnectionState::WaitingToRetry);
    CHECK_EQ(connection.snapshot().attempt, 0);
    CHECK_EQ(connection.snapshot().last_failure, BleFailure::LinkLost);
    CHECK_EQ(central.disconnect_calls, 0);
    connection.tick(25ms);
    CHECK_EQ(connection.snapshot().state, ConnectionState::Scanning);
    CHECK_EQ(connection.snapshot().attempt, 1);
    CHECK_EQ(central.scan_calls, 2);
}

TEST_CASE("an explicit stop disconnects without reconnecting") {
    FakeBleCentral central;
    DashKitConnection connection{central};
    reach_ready(connection, central);

    connection.stop();
    central.observer->on_disconnected(10ms);
    connection.tick(60s);

    CHECK_EQ(connection.snapshot().state, ConnectionState::Idle);
    CHECK_EQ(central.disconnect_calls, 1);
    CHECK_EQ(central.scan_calls, 1);
}

TEST_CASE("destruction releases an active BLE link") {
    FakeBleCentral central;
    {
        DashKitConnection connection{central};
        reach_ready(connection, central);
        CHECK(central.observer != nullptr);
    }

    CHECK(central.observer == nullptr);
    CHECK_EQ(central.disconnect_calls, 1);
}

TEST_CASE("forwards only non-empty CAN notifications while ready") {
    FakeBleCentral central;
    DashKitConnection connection{central};
    RecordingObserver observer;
    connection.set_observer(&observer);
    const std::array<std::uint8_t, 3> payload{1, 2, 3};

    connection.start(0ms);
    central.observer->on_notification(
        tesla_headup::dashkit::profile::can_characteristic_uuid,
        payload,
        1ms);
    reach_ready(connection, central, 10ms);
    central.observer->on_notification("wrong-characteristic", payload, 20ms);
    central.observer->on_notification(
        tesla_headup::dashkit::profile::can_characteristic_uuid,
        std::span<const std::uint8_t>{},
        21ms);
    central.observer->on_notification(
        tesla_headup::dashkit::profile::can_characteristic_uuid,
        payload,
        22ms);

    CHECK_EQ(observer.payloads.size(), 1U);
    CHECK_EQ(observer.payloads.front().size(), 3U);
    CHECK_EQ(observer.payloads.front().at(1), 2);
    CHECK_EQ(observer.received_times.front(), 22ms);
}

TEST_CASE("a missing service is retried and disconnects the partial link") {
    FakeBleCentral central;
    DashKitConnection connection{central};

    connection.start(0ms);
    central.observer->on_device_found({dashkit_id(), "DashKit"}, 1ms);
    central.observer->on_connected(2ms);
    central.observer->on_secured(3ms);
    central.observer->on_service_discovered(false, 4ms);

    CHECK_EQ(connection.snapshot().state, ConnectionState::WaitingToRetry);
    CHECK_EQ(connection.snapshot().last_failure, BleFailure::ServiceMissing);
    CHECK_EQ(central.disconnect_calls, 1);
}

TEST_CASE("a platform that cannot start scanning fails without hanging") {
    FakeBleCentral central;
    central.accept_scan = false;
    tesla_headup::dashkit::ConnectionPolicy policy;
    policy.max_attempts = 1;
    DashKitConnection connection{central, policy};

    connection.start(0ms);

    CHECK_EQ(connection.snapshot().state, ConnectionState::Failed);
    CHECK_EQ(
        connection.snapshot().last_failure,
        BleFailure::AdapterUnavailable);
    CHECK_EQ(central.stop_scan_calls, 1);
}

TEST_CASE("security rejection retries the complete connection") {
    FakeBleCentral central;
    central.remove_bond_result = true;
    DashKitConnection connection{central};

    connection.start(0ms);
    central.observer->on_device_found({dashkit_id(), "DashKit"}, 1ms);
    central.observer->on_connected(2ms);
    central.observer->on_security_failed(BleFailure::SecurityFailed, 3ms);

    CHECK_EQ(connection.snapshot().state, ConnectionState::WaitingToRetry);
    CHECK_EQ(connection.snapshot().last_failure, BleFailure::SecurityFailed);
    CHECK_EQ(central.remove_bond_calls, 1);
    CHECK_EQ(central.disconnect_calls, 1);
}

TEST_CASE("subscription rejection retries rather than reporting ready") {
    FakeBleCentral central;
    DashKitConnection connection{central};

    connection.start(0ms);
    central.observer->on_device_found({dashkit_id(), "DashKit"}, 1ms);
    central.observer->on_connected(2ms);
    central.observer->on_secured(3ms);
    central.observer->on_service_discovered(true, 4ms);
    central.observer->on_firmware_version_read(true, current_version, 5ms);
    central.observer->on_subscription_changed(false, 6ms);

    CHECK_EQ(connection.snapshot().state, ConnectionState::WaitingToRetry);
    CHECK_EQ(
        connection.snapshot().last_failure,
        BleFailure::SubscriptionFailed);
    CHECK_EQ(central.disconnect_calls, 1);
}

TEST_CASE("firmware older than 0.1.2 remains connected without keepalive") {
    FakeBleCentral central;
    DashKitConnection connection{central};
    constexpr std::array<std::uint8_t, 5> old_version{'0', '.', '1', '.', '1'};

    connection.start(0ms);
    central.observer->on_device_found({dashkit_id(), "DashKit"}, 1ms);
    central.observer->on_connected(2ms);
    central.observer->on_secured(3ms);
    central.observer->on_service_discovered(true, 4ms);
    central.observer->on_firmware_version_read(true, old_version, 5ms);
    central.observer->on_subscription_changed(true, 6ms);
    connection.tick(60s);

    CHECK_EQ(connection.snapshot().state, ConnectionState::Ready);
    CHECK(connection.snapshot().firmware_version.known);
    CHECK(!connection.snapshot().keepalive_enabled);
    CHECK_EQ(central.keepalive_calls, 0);
}

TEST_CASE("missing or malformed firmware versions fail open without pings") {
    for (const auto malformed : std::array{
             std::string_view{""},
             std::string_view{"0.1"},
             std::string_view{"0.1.2-beta"},
             std::string_view{"0.1.70000"},
             std::string_view{"v0.2.0"}}) {
        FakeBleCentral central;
        DashKitConnection connection{central};
        connection.start(0ms);
        central.observer->on_device_found({dashkit_id(), "DashKit"}, 1ms);
        central.observer->on_connected(2ms);
        central.observer->on_secured(3ms);
        central.observer->on_service_discovered(true, 4ms);
        central.observer->on_firmware_version_read(
            true,
            std::span<const std::uint8_t>{
                reinterpret_cast<const std::uint8_t*>(malformed.data()),
                malformed.size()},
            5ms);
        central.observer->on_subscription_changed(true, 6ms);

        CHECK_EQ(connection.snapshot().state, ConnectionState::Ready);
        CHECK(!connection.snapshot().firmware_version.known);
        CHECK(!connection.snapshot().keepalive_enabled);
        CHECK_EQ(central.keepalive_calls, 0);
    }
}

TEST_CASE("an unavailable version characteristic stays compatible") {
    FakeBleCentral central;
    DashKitConnection connection{central};

    connection.start(0ms);
    central.observer->on_device_found({dashkit_id(), "DashKit"}, 1ms);
    central.observer->on_connected(2ms);
    central.observer->on_secured(3ms);
    central.observer->on_service_discovered(true, 4ms);
    central.observer->on_firmware_version_read(false, {}, 5ms);
    central.observer->on_subscription_changed(true, 6ms);

    CHECK_EQ(connection.snapshot().state, ConnectionState::Ready);
    CHECK(!connection.snapshot().keepalive_enabled);
    CHECK_EQ(central.subscribe_calls, 1);
}

TEST_CASE("a version read that cannot be dispatched also stays compatible") {
    FakeBleCentral central;
    central.accept_version_read = false;
    DashKitConnection connection{central};

    connection.start(0ms);
    central.observer->on_device_found({dashkit_id(), "DashKit"}, 1ms);
    central.observer->on_connected(2ms);
    central.observer->on_secured(3ms);
    central.observer->on_service_discovered(true, 4ms);
    central.observer->on_subscription_changed(true, 5ms);

    CHECK_EQ(connection.snapshot().state, ConnectionState::Ready);
    CHECK(!connection.snapshot().keepalive_enabled);
    CHECK_EQ(central.version_read_calls, 1);
    CHECK_EQ(central.subscribe_calls, 1);
}

TEST_CASE("keepalive is repeated every fifteen seconds after completion") {
    FakeBleCentral central;
    DashKitConnection connection{central};
    reach_ready(connection, central);

    connection.tick(15'006ms);
    CHECK_EQ(central.keepalive_calls, 1);
    connection.tick(15'007ms);
    CHECK_EQ(central.keepalive_calls, 2);
    central.observer->on_keepalive_written(true, 15'008ms);
    connection.tick(30'007ms);
    CHECK_EQ(central.keepalive_calls, 2);
    connection.tick(30'008ms);
    CHECK_EQ(central.keepalive_calls, 3);
}

TEST_CASE("a rejected or timed-out keepalive reconnects the link") {
    {
        FakeBleCentral central;
        DashKitConnection connection{central};
        reach_ready(connection, central);
        connection.tick(15'007ms);
        central.observer->on_keepalive_written(false, 15'008ms);

        CHECK_EQ(connection.snapshot().state, ConnectionState::WaitingToRetry);
        CHECK_EQ(connection.snapshot().last_failure, BleFailure::KeepaliveFailed);
        CHECK_EQ(central.disconnect_calls, 1);
    }

    {
        FakeBleCentral central;
        tesla_headup::dashkit::ConnectionPolicy policy;
        policy.keepalive_write_timeout = 3ms;
        DashKitConnection connection{central, policy};
        reach_ready(connection, central);
        connection.tick(15'007ms);
        connection.tick(15'010ms);

        CHECK_EQ(connection.snapshot().state, ConnectionState::WaitingToRetry);
        CHECK_EQ(connection.snapshot().last_failure, BleFailure::KeepaliveFailed);
    }

    {
        FakeBleCentral central;
        central.accept_keepalive = false;
        DashKitConnection connection{central};
        connection.start(0ms);
        central.observer->on_device_found({dashkit_id(), "DashKit"}, 1ms);
        central.observer->on_connected(2ms);
        central.observer->on_secured(3ms);
        central.observer->on_service_discovered(true, 4ms);
        central.observer->on_firmware_version_read(
            true, current_version, 5ms);
        central.observer->on_subscription_changed(true, 6ms);

        CHECK_EQ(connection.snapshot().state, ConnectionState::WaitingToRetry);
        CHECK_EQ(
            connection.snapshot().last_failure,
            BleFailure::KeepaliveFailed);
    }
}

TEST_CASE("stale local bond recovery is attempted only once before guidance") {
    FakeBleCentral central;
    central.remove_bond_result = true;
    tesla_headup::dashkit::ConnectionPolicy policy;
    policy.retry_delay = 1ms;
    DashKitConnection connection{central, policy};

    connection.start(0ms);
    central.observer->on_device_found({dashkit_id(), "DashKit"}, 1ms);
    central.observer->on_connected(2ms);
    central.observer->on_security_failed(BleFailure::SecurityFailed, 3ms);
    connection.tick(4ms);
    central.observer->on_device_found({dashkit_id(), "DashKit"}, 5ms);
    central.observer->on_connected(6ms);
    central.observer->on_security_failed(BleFailure::SecurityFailed, 7ms);

    CHECK_EQ(central.remove_bond_calls, 1);
    CHECK_EQ(connection.snapshot().state, ConnectionState::WaitingToRetry);
    CHECK_EQ(connection.snapshot().last_failure, BleFailure::PairingRequired);
}

TEST_CASE("security failure without a local bond requests pairing") {
    FakeBleCentral central;
    DashKitConnection connection{central};

    connection.start(0ms);
    central.observer->on_device_found({dashkit_id(), "DashKit"}, 1ms);
    central.observer->on_connected(2ms);
    central.observer->on_security_failed(BleFailure::SecurityFailed, 3ms);

    CHECK_EQ(central.remove_bond_calls, 1);
    CHECK_EQ(connection.snapshot().state, ConnectionState::WaitingToRetry);
    CHECK_EQ(connection.snapshot().last_failure, BleFailure::PairingRequired);
}

TEST_CASE("every asynchronous setup stage has a timeout") {
    tesla_headup::dashkit::ConnectionPolicy policy;
    policy.max_attempts = 2;
    policy.scan_timeout = 10ms;
    policy.connect_timeout = 10ms;
    policy.security_timeout = 10ms;
    policy.discovery_timeout = 10ms;
    policy.firmware_version_timeout = 10ms;
    policy.subscription_timeout = 10ms;

    {
        FakeBleCentral central;
        DashKitConnection connection{central, policy};
        connection.start(0ms);
        central.observer->on_device_found({dashkit_id(), "DashKit"}, 1ms);
        connection.tick(11ms);
        CHECK_EQ(connection.snapshot().state, ConnectionState::WaitingToRetry);
        CHECK_EQ(connection.snapshot().last_failure, BleFailure::Timeout);
    }

    {
        FakeBleCentral central;
        DashKitConnection connection{central, policy};
        connection.start(0ms);
        central.observer->on_device_found({dashkit_id(), "DashKit"}, 1ms);
        central.observer->on_connected(2ms);
        connection.tick(12ms);
        CHECK_EQ(connection.snapshot().state, ConnectionState::WaitingToRetry);
        CHECK_EQ(connection.snapshot().last_failure, BleFailure::Timeout);
    }

    {
        FakeBleCentral central;
        DashKitConnection connection{central, policy};
        connection.start(0ms);
        central.observer->on_device_found({dashkit_id(), "DashKit"}, 1ms);
        central.observer->on_connected(2ms);
        central.observer->on_secured(3ms);
        connection.tick(13ms);
        CHECK_EQ(connection.snapshot().state, ConnectionState::WaitingToRetry);
        CHECK_EQ(connection.snapshot().last_failure, BleFailure::Timeout);
    }

    {
        FakeBleCentral central;
        DashKitConnection connection{central, policy};
        connection.start(0ms);
        central.observer->on_device_found({dashkit_id(), "DashKit"}, 1ms);
        central.observer->on_connected(2ms);
        central.observer->on_secured(3ms);
        central.observer->on_service_discovered(true, 4ms);
        connection.tick(14ms);
        CHECK_EQ(connection.snapshot().state, ConnectionState::WaitingToRetry);
        CHECK_EQ(connection.snapshot().last_failure, BleFailure::Timeout);
    }

    {
        FakeBleCentral central;
        DashKitConnection connection{central, policy};
        connection.start(0ms);
        central.observer->on_device_found({dashkit_id(), "DashKit"}, 1ms);
        central.observer->on_connected(2ms);
        central.observer->on_secured(3ms);
        central.observer->on_service_discovered(true, 4ms);
        central.observer->on_firmware_version_read(
            true, current_version, 5ms);
        connection.tick(15ms);
        CHECK_EQ(connection.snapshot().state, ConnectionState::WaitingToRetry);
        CHECK_EQ(connection.snapshot().last_failure, BleFailure::Timeout);
    }
}

}  // namespace
