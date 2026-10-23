#include "tesla_headup/adapters/dashkit_ble/dashkit_connection.hpp"

#include "tesla_headup/adapters/dashkit_ble/dashkit_profile.hpp"

#include <limits>
#include <utility>

namespace tesla_headup::dashkit {
namespace {

bool parse_component(
    std::span<const std::uint8_t> value,
    std::size_t& cursor,
    std::uint16_t& component) {
    if (cursor >= value.size() || value[cursor] < '0' || value[cursor] > '9') {
        return false;
    }

    std::uint32_t parsed = 0;
    do {
        parsed = parsed * 10U + static_cast<std::uint32_t>(value[cursor] - '0');
        if (parsed > std::numeric_limits<std::uint16_t>::max()) {
            return false;
        }
        ++cursor;
    } while (
        cursor < value.size() && value[cursor] >= '0' && value[cursor] <= '9');

    component = static_cast<std::uint16_t>(parsed);
    return true;
}

FirmwareVersion parse_version(std::span<const std::uint8_t> value) {
    FirmwareVersion version;
    std::size_t cursor = 0;
    if (!parse_component(value, cursor, version.major) ||
        cursor >= value.size() || value[cursor++] != '.' ||
        !parse_component(value, cursor, version.minor) ||
        cursor >= value.size() || value[cursor++] != '.' ||
        !parse_component(value, cursor, version.patch) ||
        cursor != value.size()) {
        return {};
    }
    version.known = true;
    return version;
}

bool supports_keepalive(FirmwareVersion version) {
    if (!version.known) {
        return false;
    }
    const auto& minimum = profile::keepalive_since;
    return version.major > minimum[0] ||
           (version.major == minimum[0] && version.minor > minimum[1]) ||
           (version.major == minimum[0] && version.minor == minimum[1] &&
            version.patch >= minimum[2]);
}

}  // namespace

DashKitConnection::DashKitConnection(
    BleCentral& central,
    ConnectionPolicy policy)
    : central_(central), policy_(policy) {
    central_.set_observer(this);
}

DashKitConnection::~DashKitConnection() {
    const auto previous_state = snapshot_.state;
    requested_ = false;
    central_.set_observer(nullptr);
    cleanup_active_operation(previous_state);
}

void DashKitConnection::set_observer(DashKitConnectionObserver* observer) {
    observer_ = observer;
    if (observer_ != nullptr) {
        observer_->on_connection_state_changed(snapshot_);
    }
}

void DashKitConnection::start(MonotonicTime now) {
    if (requested_) {
        return;
    }

    requested_ = true;
    snapshot_.attempt = 0;
    snapshot_.firmware_version = {};
    snapshot_.keepalive_enabled = false;
    pairing_recovery_attempted_ = false;
    keepalive_pending_ = false;
    begin_scan(now);
}

void DashKitConnection::stop() {
    if (!requested_ && snapshot_.state == ConnectionState::Idle) {
        return;
    }

    requested_ = false;
    keepalive_pending_ = false;
    const auto previous_state = snapshot_.state;
    snapshot_.attempt = 0;
    transition(ConnectionState::Idle, MonotonicTime{}, BleFailure::None);
    cleanup_active_operation(previous_state);
}

void DashKitConnection::tick(MonotonicTime now) {
    if (!requested_) {
        return;
    }

    if (snapshot_.state == ConnectionState::WaitingToRetry) {
        if (now >= deadline_) {
            begin_scan(now);
        }
        return;
    }

    switch (snapshot_.state) {
    case ConnectionState::Scanning:
    case ConnectionState::Connecting:
    case ConnectionState::Securing:
    case ConnectionState::Discovering:
    case ConnectionState::Subscribing:
        if (now >= deadline_) {
            schedule_retry_or_fail(BleFailure::Timeout, now);
        }
        break;
    case ConnectionState::ReadingFirmwareVersion:
        // Version probing is optional for compatibility with firmware that
        // predates CADA0005. An explicit failed read disables pings but not
        // telemetry; a hung GATT operation requires a clean reconnect.
        if (now >= deadline_) {
            schedule_retry_or_fail(BleFailure::Timeout, now);
        }
        break;
    case ConnectionState::Ready:
        if (keepalive_pending_ && now >= deadline_) {
            schedule_retry_or_fail(BleFailure::KeepaliveFailed, now);
        } else if (
            snapshot_.keepalive_enabled && !keepalive_pending_ &&
            now >= next_keepalive_) {
            dispatch_keepalive(now);
        }
        break;
    case ConnectionState::Idle:
    case ConnectionState::WaitingToRetry:
    case ConnectionState::Failed:
        break;
    }
}

ConnectionSnapshot DashKitConnection::snapshot() const noexcept {
    return snapshot_;
}

void DashKitConnection::on_device_found(
    DiscoveredDevice device,
    MonotonicTime now) {
    if (snapshot_.state != ConnectionState::Scanning ||
        device.name != profile::device_name) {
        return;
    }

    central_.stop_scan();
    transition(
        ConnectionState::Connecting,
        now + policy_.connect_timeout);
    if (!central_.connect(device.id)) {
        schedule_retry_or_fail(BleFailure::OperationRejected, now);
    }
}

void DashKitConnection::on_scan_failed(
    BleFailure failure,
    MonotonicTime now) {
    if (snapshot_.state == ConnectionState::Scanning) {
        schedule_retry_or_fail(failure, now);
    }
}

void DashKitConnection::on_connected(MonotonicTime now) {
    if (snapshot_.state != ConnectionState::Connecting) {
        return;
    }

    transition(ConnectionState::Securing, now + policy_.security_timeout);
    if (!central_.secure_connection()) {
        schedule_retry_or_fail(BleFailure::OperationRejected, now);
    }
}

void DashKitConnection::on_connection_failed(
    BleFailure failure,
    MonotonicTime now) {
    if (snapshot_.state == ConnectionState::Connecting) {
        schedule_retry_or_fail(failure, now);
    }
}

void DashKitConnection::on_secured(MonotonicTime now) {
    if (snapshot_.state != ConnectionState::Securing) {
        return;
    }

    transition(ConnectionState::Discovering, now + policy_.discovery_timeout);
    if (!central_.discover_service(profile::can_service_uuid)) {
        schedule_retry_or_fail(BleFailure::OperationRejected, now);
    }
}

void DashKitConnection::on_security_failed(
    BleFailure failure,
    MonotonicTime now) {
    if (snapshot_.state == ConnectionState::Securing) {
        if (!pairing_recovery_attempted_ && central_.remove_peer_bond()) {
            pairing_recovery_attempted_ = true;
            schedule_retry_or_fail(failure, now);
        } else {
            schedule_retry_or_fail(BleFailure::PairingRequired, now);
        }
    }
}

void DashKitConnection::on_service_discovered(
    bool found,
    MonotonicTime now) {
    if (snapshot_.state != ConnectionState::Discovering) {
        return;
    }

    if (!found) {
        schedule_retry_or_fail(BleFailure::ServiceMissing, now);
        return;
    }

    transition(
        ConnectionState::ReadingFirmwareVersion,
        now + policy_.firmware_version_timeout);
    if (!central_.read_firmware_version()) {
        begin_subscription(now);
    }
}

void DashKitConnection::on_firmware_version_read(
    bool success,
    std::span<const std::uint8_t> value,
    MonotonicTime now) {
    if (snapshot_.state != ConnectionState::ReadingFirmwareVersion) {
        return;
    }

    snapshot_.firmware_version = success ? parse_version(value) : FirmwareVersion{};
    snapshot_.keepalive_enabled =
        supports_keepalive(snapshot_.firmware_version);
    begin_subscription(now);
}

void DashKitConnection::on_subscription_changed(
    bool enabled,
    MonotonicTime now) {
    if (snapshot_.state != ConnectionState::Subscribing) {
        return;
    }

    if (!enabled) {
        schedule_retry_or_fail(BleFailure::SubscriptionFailed, now);
        return;
    }

    transition(ConnectionState::Ready, MonotonicTime{});
    pairing_recovery_attempted_ = false;
    if (snapshot_.keepalive_enabled) {
        dispatch_keepalive(now);
    }
}

void DashKitConnection::on_keepalive_written(
    bool success,
    MonotonicTime now) {
    if (snapshot_.state != ConnectionState::Ready || !keepalive_pending_) {
        return;
    }

    keepalive_pending_ = false;
    if (!success) {
        schedule_retry_or_fail(BleFailure::KeepaliveFailed, now);
        return;
    }
    next_keepalive_ = now + policy_.keepalive_interval;
}

void DashKitConnection::on_disconnected(MonotonicTime now) {
    if (!requested_ || snapshot_.state == ConnectionState::Idle ||
        snapshot_.state == ConnectionState::WaitingToRetry ||
        snapshot_.state == ConnectionState::Failed) {
        return;
    }

    if (snapshot_.state == ConnectionState::Ready) {
        // A previously healthy link receives a fresh retry budget.
        snapshot_.attempt = 0;
    }
    keepalive_pending_ = false;

    // The platform has already torn the link down, so transition without
    // asking it to disconnect a second time.
    if (snapshot_.attempt >= policy_.max_attempts) {
        requested_ = false;
        transition(
            ConnectionState::Failed,
            MonotonicTime{},
            BleFailure::LinkLost);
    } else {
        transition(
            ConnectionState::WaitingToRetry,
            now + policy_.retry_delay,
            BleFailure::LinkLost);
    }
}

void DashKitConnection::on_notification(
    std::string_view characteristic_uuid,
    std::span<const std::uint8_t> payload,
    MonotonicTime now) {
    if (snapshot_.state == ConnectionState::Ready && observer_ != nullptr &&
        characteristic_uuid == profile::can_characteristic_uuid &&
        !payload.empty()) {
        observer_->on_can_payload(payload, now);
    }
}

void DashKitConnection::begin_scan(MonotonicTime now) {
    if (!requested_) {
        return;
    }

    if (policy_.max_attempts == 0 || snapshot_.attempt >= policy_.max_attempts) {
        requested_ = false;
        transition(ConnectionState::Failed, MonotonicTime{}, snapshot_.last_failure);
        return;
    }

    ++snapshot_.attempt;
    snapshot_.firmware_version = {};
    snapshot_.keepalive_enabled = false;
    keepalive_pending_ = false;
    transition(ConnectionState::Scanning, now + policy_.scan_timeout);
    if (!central_.scan_for_name(profile::device_name)) {
        schedule_retry_or_fail(BleFailure::AdapterUnavailable, now);
    }
}

void DashKitConnection::begin_subscription(MonotonicTime now) {
    transition(
        ConnectionState::Subscribing,
        now + policy_.subscription_timeout);
    if (!central_.subscribe(
            profile::can_service_uuid,
            profile::can_characteristic_uuid)) {
        schedule_retry_or_fail(BleFailure::OperationRejected, now);
    }
}

void DashKitConnection::dispatch_keepalive(MonotonicTime now) {
    keepalive_pending_ = true;
    deadline_ = now + policy_.keepalive_write_timeout;
    if (!central_.send_keepalive()) {
        keepalive_pending_ = false;
        schedule_retry_or_fail(BleFailure::KeepaliveFailed, now);
    }
}

void DashKitConnection::schedule_retry_or_fail(
    BleFailure failure,
    MonotonicTime now) {
    const auto previous_state = snapshot_.state;
    if (snapshot_.attempt >= policy_.max_attempts) {
        requested_ = false;
        transition(ConnectionState::Failed, MonotonicTime{}, failure);
    } else {
        transition(
            ConnectionState::WaitingToRetry,
            now + policy_.retry_delay,
            failure);
    }
    cleanup_active_operation(previous_state);
}

void DashKitConnection::transition(
    ConnectionState state,
    MonotonicTime deadline,
    BleFailure failure) {
    snapshot_.state = state;
    snapshot_.last_failure = failure;
    deadline_ = deadline;
    if (observer_ != nullptr) {
        observer_->on_connection_state_changed(snapshot_);
    }
}

void DashKitConnection::cleanup_active_operation(ConnectionState previous_state) {
    if (previous_state == ConnectionState::Scanning) {
        central_.stop_scan();
        return;
    }

    switch (previous_state) {
    case ConnectionState::Connecting:
    case ConnectionState::Securing:
    case ConnectionState::Discovering:
    case ConnectionState::ReadingFirmwareVersion:
    case ConnectionState::Subscribing:
    case ConnectionState::Ready:
        central_.disconnect();
        break;
    case ConnectionState::Idle:
    case ConnectionState::Scanning:
    case ConnectionState::WaitingToRetry:
    case ConnectionState::Failed:
        break;
    }
}

}  // namespace tesla_headup::dashkit
