#pragma once

#include <chrono>
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace tesla_headup::dashkit {

using MonotonicTime = std::chrono::milliseconds;

enum class BleFailure : std::uint8_t {
    None,
    AdapterUnavailable,
    OperationRejected,
    Timeout,
    ConnectionFailed,
    SecurityFailed,
    ServiceMissing,
    SubscriptionFailed,
    KeepaliveFailed,
    PairingRequired,
    LinkLost,
};

struct BleDeviceId {
    std::array<std::uint8_t, 6> address{};
    std::uint8_t address_type{};

    friend constexpr bool operator==(const BleDeviceId&, const BleDeviceId&) =
        default;
};

struct DiscoveredDevice {
    BleDeviceId id;
    std::string_view name;
};

class BleCentralObserver {
public:
    virtual ~BleCentralObserver() = default;

    virtual void on_device_found(DiscoveredDevice device, MonotonicTime now) = 0;
    virtual void on_scan_failed(BleFailure failure, MonotonicTime now) = 0;
    virtual void on_connected(MonotonicTime now) = 0;
    virtual void on_connection_failed(BleFailure failure, MonotonicTime now) = 0;
    virtual void on_secured(MonotonicTime now) = 0;
    virtual void on_security_failed(BleFailure failure, MonotonicTime now) = 0;
    virtual void on_service_discovered(bool found, MonotonicTime now) = 0;
    virtual void on_subscription_changed(bool enabled, MonotonicTime now) = 0;
    virtual void on_firmware_version_read(
        bool success,
        std::span<const std::uint8_t> value,
        MonotonicTime now) = 0;
    virtual void on_keepalive_written(bool success, MonotonicTime now) = 0;
    virtual void on_disconnected(MonotonicTime now) = 0;
    virtual void on_notification(
        std::string_view characteristic_uuid,
        std::span<const std::uint8_t> payload,
        MonotonicTime now) = 0;
};

// Platform seam implemented by the ESP-IDF/NimBLE adapter. All calls
// dispatch asynchronous work; completion is reported through the observer.
// Calls and callbacks must be serialized on one event-loop context.
class BleCentral {
public:
    virtual ~BleCentral() = default;

    virtual void set_observer(BleCentralObserver* observer) = 0;
    virtual bool scan_for_name(std::string_view advertised_name) = 0;
    virtual void stop_scan() = 0;
    virtual bool connect(BleDeviceId device_id) = 0;
    virtual bool secure_connection() = 0;
    virtual bool discover_service(std::string_view service_uuid) = 0;
    virtual bool subscribe(
        std::string_view service_uuid,
        std::string_view characteristic_uuid) = 0;
    virtual bool read_firmware_version() = 0;
    virtual bool send_keepalive() = 0;
    // Removes only the active peer's persisted bond. Used once after a
    // security failure to recover when either side retained stale keys.
    virtual bool remove_peer_bond() = 0;
    virtual void disconnect() = 0;
};

}  // namespace tesla_headup::dashkit
