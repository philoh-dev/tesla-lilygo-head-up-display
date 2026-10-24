#pragma once

#include "tesla_headup/adapters/dashkit_ble/ble_central.hpp"

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

struct ble_gap_event;
struct ble_gatt_attr;
struct ble_gatt_chr;
struct ble_gatt_dsc;
struct ble_gatt_error;
struct ble_gatt_svc;

namespace tesla_headup::platform {

class EspNimbleCentral final : public dashkit::BleCentral {
public:
    EspNimbleCentral();
    ~EspNimbleCentral() override;

    EspNimbleCentral(const EspNimbleCentral&) = delete;
    EspNimbleCentral& operator=(const EspNimbleCentral&) = delete;

    [[nodiscard]] esp_err_t initialize();

    // Drains a snapshot of bounded NimBLE callback events. Call this regularly
    // from the same task that owns DashKitConnection.
    void process_events();

    void set_observer(dashkit::BleCentralObserver* observer) override;
    bool scan_for_name(std::string_view advertised_name) override;
    void stop_scan() override;
    bool connect(dashkit::BleDeviceId device_id) override;
    bool secure_connection() override;
    bool discover_service(std::string_view service_uuid) override;
    bool subscribe(
        std::string_view service_uuid,
        std::string_view characteristic_uuid) override;
    bool read_firmware_version() override;
    bool send_keepalive() override;
    bool remove_peer_bond() override;
    void disconnect() override;

private:
    static constexpr std::size_t max_notification_size = 512;
    static constexpr std::size_t event_queue_depth = 16;

    enum class EventType : std::uint8_t {
        DeviceFound,
        ScanFailed,
        Connected,
        ConnectionFailed,
        Secured,
        SecurityFailed,
        ServiceDiscovered,
        SubscriptionChanged,
        FirmwareVersionRead,
        KeepaliveWritten,
        Disconnected,
        Notification,
    };

    enum class CharacteristicOperation : std::uint8_t {
        None,
        Subscribe,
        ReadFirmwareVersion,
        WriteKeepalive,
    };

    struct Event {
        constexpr Event() = default;
        constexpr Event(
            EventType event_type,
            dashkit::MonotonicTime event_timestamp,
            dashkit::BleFailure event_failure = dashkit::BleFailure::None)
            : type(event_type),
              timestamp(event_timestamp),
              failure(event_failure) {}

        EventType type{};
        dashkit::MonotonicTime timestamp{};
        dashkit::BleDeviceId device{};
        dashkit::BleFailure failure{dashkit::BleFailure::None};
        bool flag{false};
        std::uint16_t payload_size{0};
        std::array<std::uint8_t, max_notification_size> payload{};
    };

    static void host_task(void* argument);
    static void on_host_reset(int reason);
    static void on_host_sync();
    static int gap_event(ble_gap_event* event, void* argument);
    static int service_discovery(
        std::uint16_t connection_handle,
        const ble_gatt_error* error,
        const ble_gatt_svc* service,
        void* argument);
    static int characteristic_discovery(
        std::uint16_t connection_handle,
        const ble_gatt_error* error,
        const ble_gatt_chr* characteristic,
        void* argument);
    static int descriptor_discovery(
        std::uint16_t connection_handle,
        const ble_gatt_error* error,
        std::uint16_t characteristic_value_handle,
        const ble_gatt_dsc* descriptor,
        void* argument);
    static int subscription_write(
        std::uint16_t connection_handle,
        const ble_gatt_error* error,
        ble_gatt_attr* attribute,
        void* argument);
    static int firmware_version_read(
        std::uint16_t connection_handle,
        const ble_gatt_error* error,
        ble_gatt_attr* attribute,
        void* argument);
    static int keepalive_write(
        std::uint16_t connection_handle,
        const ble_gatt_error* error,
        ble_gatt_attr* attribute,
        void* argument);

    [[nodiscard]] bool start_scan_native();
    int handle_gap_event(const ble_gap_event& event);
    void handle_service_discovery(
        const ble_gatt_error& error,
        const ble_gatt_svc* service);
    void handle_characteristic_discovery(
        const ble_gatt_error& error,
        const ble_gatt_chr* characteristic);
    void handle_descriptor_discovery(
        const ble_gatt_error& error,
        const ble_gatt_dsc* descriptor);
    void handle_subscription_write(const ble_gatt_error& error);
    void handle_firmware_version_read(
        const ble_gatt_error& error,
        const ble_gatt_attr* attribute);
    void handle_keepalive_write(const ble_gatt_error& error);
    void fail_characteristic_operation();
    void enqueue(Event event);
    [[nodiscard]] static dashkit::MonotonicTime now();

    static EspNimbleCentral* instance_;

    dashkit::BleCentralObserver* observer_{nullptr};
    QueueHandle_t event_queue_{nullptr};
    StaticQueue_t event_queue_storage_{};
    alignas(std::max_align_t)
    std::array<std::uint8_t, event_queue_depth * sizeof(Event)>
        event_queue_buffer_{};
    std::atomic<bool> host_synced_{false};
    std::atomic<bool> scan_pending_{false};
    std::atomic<std::uint32_t> dropped_notifications_{0};
    std::atomic<std::uint32_t> dropped_control_events_{0};
    dashkit::MonotonicTime last_queue_report_{};
    std::atomic<std::uint16_t> connection_handle_;
    std::uint8_t own_address_type_;
    dashkit::BleDeviceId bond_peer_{};
    bool bond_peer_known_{false};
    std::uint16_t service_start_handle_{0};
    std::uint16_t service_end_handle_{0};
    std::uint16_t can_value_handle_{0};
    std::uint16_t firmware_version_value_handle_{0};
    std::uint16_t control_value_handle_{0};
    std::uint16_t pending_value_handle_{0};
    std::uint16_t cccd_handle_{0};
    bool service_found_{false};
    bool characteristic_found_{false};
    bool descriptor_found_{false};
    CharacteristicOperation characteristic_operation_{
        CharacteristicOperation::None};
    bool initialized_{false};
};

}  // namespace tesla_headup::platform
