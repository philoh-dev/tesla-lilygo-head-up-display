#pragma once

#include "tesla_headup/adapters/dashkit_ble/ble_central.hpp"

#include <chrono>
#include <cstdint>
#include <span>

namespace tesla_headup::dashkit {

enum class ConnectionState : std::uint8_t {
    Idle,
    Scanning,
    Connecting,
    Securing,
    Discovering,
    ReadingFirmwareVersion,
    Subscribing,
    Ready,
    WaitingToRetry,
    Failed,
};

struct FirmwareVersion {
    std::uint16_t major{0};
    std::uint16_t minor{0};
    std::uint16_t patch{0};
    bool known{false};

    friend constexpr bool operator==(
        const FirmwareVersion&,
        const FirmwareVersion&) = default;
};

struct ConnectionSnapshot {
    ConnectionState state{ConnectionState::Idle};
    std::uint8_t attempt{0};
    BleFailure last_failure{BleFailure::None};
    FirmwareVersion firmware_version{};
    bool keepalive_enabled{false};

    friend constexpr bool operator==(
        const ConnectionSnapshot&,
        const ConnectionSnapshot&) = default;
};

struct ConnectionPolicy {
    std::uint8_t max_attempts{3};
    std::chrono::milliseconds scan_timeout{15'000};
    std::chrono::milliseconds connect_timeout{10'000};
    std::chrono::milliseconds security_timeout{30'000};
    std::chrono::milliseconds discovery_timeout{10'000};
    std::chrono::milliseconds firmware_version_timeout{5'000};
    std::chrono::milliseconds subscription_timeout{10'000};
    std::chrono::milliseconds keepalive_interval{15'000};
    std::chrono::milliseconds keepalive_write_timeout{5'000};
    std::chrono::milliseconds retry_delay{2'000};
};

class DashKitConnectionObserver {
public:
    virtual ~DashKitConnectionObserver() = default;

    virtual void on_connection_state_changed(ConnectionSnapshot snapshot) = 0;
    virtual void on_can_payload(
        std::span<const std::uint8_t> payload,
        MonotonicTime received_at) = 0;
};

// Host-testable DashKit connection orchestration. It owns retry, timeout, and
// protocol-profile behavior but delegates every platform BLE operation.
class DashKitConnection final : public BleCentralObserver {
public:
    explicit DashKitConnection(
        BleCentral& central,
        ConnectionPolicy policy = {});
    ~DashKitConnection() override;

    DashKitConnection(const DashKitConnection&) = delete;
    DashKitConnection& operator=(const DashKitConnection&) = delete;

    void set_observer(DashKitConnectionObserver* observer);

    void start(MonotonicTime now);
    void stop();
    void tick(MonotonicTime now);

    [[nodiscard]] ConnectionSnapshot snapshot() const noexcept;

    void on_device_found(DiscoveredDevice device, MonotonicTime now) override;
    void on_scan_failed(BleFailure failure, MonotonicTime now) override;
    void on_connected(MonotonicTime now) override;
    void on_connection_failed(BleFailure failure, MonotonicTime now) override;
    void on_secured(MonotonicTime now) override;
    void on_security_failed(BleFailure failure, MonotonicTime now) override;
    void on_service_discovered(bool found, MonotonicTime now) override;
    void on_subscription_changed(bool enabled, MonotonicTime now) override;
    void on_firmware_version_read(
        bool success,
        std::span<const std::uint8_t> value,
        MonotonicTime now) override;
    void on_keepalive_written(bool success, MonotonicTime now) override;
    void on_disconnected(MonotonicTime now) override;
    void on_notification(
        std::string_view characteristic_uuid,
        std::span<const std::uint8_t> payload,
        MonotonicTime now) override;

private:
    void begin_scan(MonotonicTime now);
    void begin_subscription(MonotonicTime now);
    void dispatch_keepalive(MonotonicTime now);
    void schedule_retry_or_fail(BleFailure failure, MonotonicTime now);
    void transition(
        ConnectionState state,
        MonotonicTime deadline,
        BleFailure failure = BleFailure::None);
    void cleanup_active_operation(ConnectionState previous_state);

    BleCentral& central_;
    ConnectionPolicy policy_;
    DashKitConnectionObserver* observer_{nullptr};
    ConnectionSnapshot snapshot_{};
    MonotonicTime deadline_{};
    MonotonicTime next_keepalive_{};
    bool requested_{false};
    bool keepalive_pending_{false};
    bool pairing_recovery_attempted_{false};
};

}  // namespace tesla_headup::dashkit
