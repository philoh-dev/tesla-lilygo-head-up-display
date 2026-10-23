#include "tesla_headup/adapters/esp_nimble/esp_nimble_central.hpp"

#include "nimble_headers.hpp"
#include "tesla_headup/adapters/dashkit_ble/dashkit_profile.hpp"

#include "esp_log.h"
#include "esp_timer.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <type_traits>

#if !CONFIG_BT_NIMBLE_ROLE_PERIPHERAL
#error "DashKit compatibility requires the NimBLE peripheral role for peer-initiated ATT MTU exchange"
#endif

extern "C" void ble_store_config_init(void);

namespace tesla_headup::platform {
namespace {

constexpr char tag[] = "tesla_headup_ble";
constexpr std::uint32_t native_connect_timeout_ms = 10'000;

const ble_uuid128_t can_service_uuid = BLE_UUID128_INIT(
    0xA1, 0x00, 0x01, 0xAA, 0x00, 0xC0, 0xD6, 0xB0,
    0xE0, 0xB1, 0x00, 0xCA, 0x00, 0x00, 0xDA, 0xCA);

const ble_uuid128_t can_characteristic_uuid = BLE_UUID128_INIT(
    0xA1, 0x00, 0x01, 0xAA, 0x00, 0xC0, 0xD6, 0xB0,
    0xE0, 0xB1, 0x00, 0xCA, 0x01, 0x00, 0xDA, 0xCA);

const ble_uuid128_t control_characteristic_uuid = BLE_UUID128_INIT(
    0xA1, 0x00, 0x01, 0xAA, 0x00, 0xC0, 0xD6, 0xB0,
    0xE0, 0xB1, 0x00, 0xCA, 0x04, 0x00, 0xDA, 0xCA);

const ble_uuid128_t firmware_version_characteristic_uuid = BLE_UUID128_INIT(
    0xA1, 0x00, 0x01, 0xAA, 0x00, 0xC0, 0xD6, 0xB0,
    0xE0, 0xB1, 0x00, 0xCA, 0x05, 0x00, 0xDA, 0xCA);

}  // namespace

EspNimbleCentral* EspNimbleCentral::instance_ = nullptr;

EspNimbleCentral::EspNimbleCentral()
    : connection_handle_(BLE_HS_CONN_HANDLE_NONE),
      own_address_type_(BLE_OWN_ADDR_PUBLIC) {
    static_assert(std::is_trivially_copyable_v<Event>);
}

EspNimbleCentral::~EspNimbleCentral() {
    // Firmware owns this adapter for the process lifetime. A future explicit
    // shutdown API must stop NimBLE and join its host task before destruction.
    observer_ = nullptr;
    if (instance_ == this) {
        instance_ = nullptr;
    }
}

esp_err_t EspNimbleCentral::initialize() {
    if (initialized_ || instance_ != nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    event_queue_ = xQueueCreateStatic(
        event_queue_depth,
        sizeof(Event),
        event_queue_buffer_.data(),
        &event_queue_storage_);
    if (event_queue_ == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    const esp_err_t result = nimble_port_init();
    if (result != ESP_OK) {
        return result;
    }

    instance_ = this;
    ble_hs_cfg.reset_cb = on_host_reset;
    ble_hs_cfg.sync_cb = on_host_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_our_key_dist =
        BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist =
        BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    ble_store_config_init();

    const int mtu_result = ble_att_set_preferred_mtu(max_notification_size);
    if (mtu_result != 0) {
        ESP_LOGW(tag, "Could not set preferred ATT MTU: %d", mtu_result);
    }

    initialized_ = true;
    nimble_port_freertos_init(host_task);
    return ESP_OK;
}

void EspNimbleCentral::process_events() {
    if (event_queue_ == nullptr || observer_ == nullptr) {
        return;
    }

    Event event;
    while (xQueueReceive(event_queue_, &event, 0) == pdTRUE) {
        switch (event.type) {
        case EventType::DeviceFound:
            observer_->on_device_found(
                {event.device, dashkit::profile::device_name},
                event.timestamp);
            break;
        case EventType::ScanFailed:
            observer_->on_scan_failed(event.failure, event.timestamp);
            break;
        case EventType::Connected:
            observer_->on_connected(event.timestamp);
            break;
        case EventType::ConnectionFailed:
            observer_->on_connection_failed(event.failure, event.timestamp);
            break;
        case EventType::Secured:
            observer_->on_secured(event.timestamp);
            break;
        case EventType::SecurityFailed:
            observer_->on_security_failed(event.failure, event.timestamp);
            break;
        case EventType::ServiceDiscovered:
            observer_->on_service_discovered(event.flag, event.timestamp);
            break;
        case EventType::SubscriptionChanged:
            observer_->on_subscription_changed(event.flag, event.timestamp);
            break;
        case EventType::FirmwareVersionRead:
            observer_->on_firmware_version_read(
                event.flag,
                std::span<const std::uint8_t>{
                    event.payload.data(), event.payload_size},
                event.timestamp);
            break;
        case EventType::KeepaliveWritten:
            observer_->on_keepalive_written(event.flag, event.timestamp);
            break;
        case EventType::Disconnected:
            observer_->on_disconnected(event.timestamp);
            break;
        case EventType::Notification:
            observer_->on_notification(
                dashkit::profile::can_characteristic_uuid,
                std::span<const std::uint8_t>{
                    event.payload.data(), event.payload_size},
                event.timestamp);
            break;
        }
    }
}

void EspNimbleCentral::set_observer(dashkit::BleCentralObserver* observer) {
    observer_ = observer;
}

bool EspNimbleCentral::scan_for_name(std::string_view advertised_name) {
    if (!initialized_ || advertised_name != dashkit::profile::device_name) {
        return false;
    }

    scan_pending_.store(true, std::memory_order_release);
    if (!host_synced_.load(std::memory_order_acquire)) {
        // on_host_sync starts the pending scan as soon as the controller is
        // ready. To the state machine this is a successfully dispatched scan.
        return true;
    }

    if (!scan_pending_.exchange(false, std::memory_order_acq_rel)) {
        return true;
    }
    return start_scan_native();
}

void EspNimbleCentral::stop_scan() {
    scan_pending_.store(false, std::memory_order_release);
    if (!host_synced_.load(std::memory_order_acquire)) {
        return;
    }

    const int result = ble_gap_disc_cancel();
    if (result != 0 && result != BLE_HS_EALREADY) {
        ESP_LOGW(tag, "Could not stop BLE scan: %d", result);
    }
}

bool EspNimbleCentral::connect(dashkit::BleDeviceId device_id) {
    if (!host_synced_.load(std::memory_order_acquire)) {
        return false;
    }

    ble_addr_t peer{};
    peer.type = device_id.address_type;
    std::copy(device_id.address.begin(), device_id.address.end(), peer.val);

    const int result = ble_gap_connect(
        own_address_type_,
        &peer,
        native_connect_timeout_ms,
        nullptr,
        gap_event,
        this);
    return result == 0;
}

bool EspNimbleCentral::secure_connection() {
    const auto handle = connection_handle_.load(std::memory_order_acquire);
    if (handle == BLE_HS_CONN_HANDLE_NONE) {
        return false;
    }

    ble_gap_conn_desc description{};
    if (ble_gap_conn_find(handle, &description) == 0 &&
        description.sec_state.encrypted) {
        enqueue(Event{EventType::Secured, now()});
        return true;
    }

    return ble_gap_security_initiate(handle) == 0;
}

bool EspNimbleCentral::discover_service(std::string_view service_uuid) {
    const auto handle = connection_handle_.load(std::memory_order_acquire);
    if (handle == BLE_HS_CONN_HANDLE_NONE ||
        service_uuid != dashkit::profile::can_service_uuid) {
        return false;
    }

    service_start_handle_ = 0;
    service_end_handle_ = 0;
    service_found_ = false;
    return ble_gattc_disc_svc_by_uuid(
               handle,
               &can_service_uuid.u,
               service_discovery,
               this) == 0;
}

bool EspNimbleCentral::subscribe(
    std::string_view service_uuid,
    std::string_view characteristic_uuid) {
    const auto handle = connection_handle_.load(std::memory_order_acquire);
    if (handle == BLE_HS_CONN_HANDLE_NONE || !service_found_ ||
        service_uuid != dashkit::profile::can_service_uuid ||
        characteristic_uuid != dashkit::profile::can_characteristic_uuid) {
        return false;
    }

    if (characteristic_operation_ != CharacteristicOperation::None) {
        return false;
    }

    characteristic_operation_ = CharacteristicOperation::Subscribe;
    pending_value_handle_ = 0;
    cccd_handle_ = 0;
    characteristic_found_ = false;
    descriptor_found_ = false;
    if (can_value_handle_ != 0) {
        pending_value_handle_ = can_value_handle_;
        characteristic_found_ = true;
        if (ble_gattc_disc_all_dscs(
                handle,
                can_value_handle_,
                service_end_handle_,
                descriptor_discovery,
                this) == 0) {
            return true;
        }
        characteristic_operation_ = CharacteristicOperation::None;
        return false;
    }

    const bool dispatched = ble_gattc_disc_chrs_by_uuid(
               handle,
               service_start_handle_,
               service_end_handle_,
               &can_characteristic_uuid.u,
               characteristic_discovery,
               this) == 0;
    if (!dispatched) {
        characteristic_operation_ = CharacteristicOperation::None;
    }
    return dispatched;
}

bool EspNimbleCentral::read_firmware_version() {
    const auto handle = connection_handle_.load(std::memory_order_acquire);
    if (handle == BLE_HS_CONN_HANDLE_NONE || !service_found_ ||
        characteristic_operation_ != CharacteristicOperation::None) {
        return false;
    }

    characteristic_operation_ = CharacteristicOperation::ReadFirmwareVersion;
    if (firmware_version_value_handle_ != 0) {
        if (ble_gattc_read(
                handle,
                firmware_version_value_handle_,
                firmware_version_read,
                this) == 0) {
            return true;
        }
        characteristic_operation_ = CharacteristicOperation::None;
        return false;
    }

    pending_value_handle_ = 0;
    characteristic_found_ = false;
    const bool dispatched = ble_gattc_disc_chrs_by_uuid(
        handle,
        service_start_handle_,
        service_end_handle_,
        &firmware_version_characteristic_uuid.u,
        characteristic_discovery,
        this) == 0;
    if (!dispatched) {
        characteristic_operation_ = CharacteristicOperation::None;
    }
    return dispatched;
}

bool EspNimbleCentral::send_keepalive() {
    const auto handle = connection_handle_.load(std::memory_order_acquire);
    if (handle == BLE_HS_CONN_HANDLE_NONE || !service_found_ ||
        characteristic_operation_ != CharacteristicOperation::None) {
        return false;
    }

    characteristic_operation_ = CharacteristicOperation::WriteKeepalive;
    if (control_value_handle_ != 0) {
        if (ble_gattc_write_flat(
                handle,
                control_value_handle_,
                dashkit::profile::keepalive_command.data(),
                dashkit::profile::keepalive_command.size(),
                keepalive_write,
                this) == 0) {
            return true;
        }
        characteristic_operation_ = CharacteristicOperation::None;
        return false;
    }

    pending_value_handle_ = 0;
    characteristic_found_ = false;
    const bool dispatched = ble_gattc_disc_chrs_by_uuid(
        handle,
        service_start_handle_,
        service_end_handle_,
        &control_characteristic_uuid.u,
        characteristic_discovery,
        this) == 0;
    if (!dispatched) {
        characteristic_operation_ = CharacteristicOperation::None;
    }
    return dispatched;
}

bool EspNimbleCentral::remove_peer_bond() {
    if (!bond_peer_known_) {
        return false;
    }

    ble_addr_t peer{};
    peer.type = bond_peer_.address_type;
    std::copy(bond_peer_.address.begin(), bond_peer_.address.end(), peer.val);
    return ble_store_util_delete_peer(&peer) == 0;
}

void EspNimbleCentral::disconnect() {
    const auto handle = connection_handle_.load(std::memory_order_acquire);
    if (handle != BLE_HS_CONN_HANDLE_NONE) {
        const int result =
            ble_gap_terminate(handle, BLE_ERR_REM_USER_CONN_TERM);
        if (result != 0 && result != BLE_HS_EALREADY &&
            result != BLE_HS_ENOTCONN) {
            ESP_LOGW(tag, "Could not disconnect BLE link: %d", result);
        }
        return;
    }

    const int result = ble_gap_conn_cancel();
    if (result != 0 && result != BLE_HS_EALREADY) {
        ESP_LOGD(tag, "No pending BLE connection to cancel: %d", result);
    }
}

void EspNimbleCentral::host_task(void* argument) {
    (void)argument;
    ESP_LOGI(tag, "NimBLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void EspNimbleCentral::on_host_reset(int reason) {
    if (instance_ == nullptr) {
        return;
    }

    ESP_LOGE(tag, "NimBLE host reset: %d", reason);
    instance_->host_synced_.store(false, std::memory_order_release);
    instance_->connection_handle_.exchange(
        BLE_HS_CONN_HANDLE_NONE,
        std::memory_order_acq_rel);

    Event event{
        EventType::Disconnected,
        now(),
        dashkit::BleFailure::AdapterUnavailable};
    instance_->enqueue(event);
}

void EspNimbleCentral::on_host_sync() {
    if (instance_ == nullptr) {
        return;
    }

    const int address_result =
        ble_hs_id_infer_auto(0, &instance_->own_address_type_);
    if (address_result != 0) {
        ESP_LOGE(tag, "Could not infer local BLE address: %d", address_result);
        instance_->enqueue(Event{
            EventType::ScanFailed,
            now(),
            dashkit::BleFailure::AdapterUnavailable});
        return;
    }

    instance_->host_synced_.store(true, std::memory_order_release);
    ESP_LOGI(tag, "NimBLE host synchronized");
    if (instance_->scan_pending_.exchange(false, std::memory_order_acq_rel) &&
        !instance_->start_scan_native()) {
        instance_->enqueue(Event{
            EventType::ScanFailed,
            now(),
            dashkit::BleFailure::AdapterUnavailable});
    }
}

int EspNimbleCentral::gap_event(ble_gap_event* event, void* argument) {
    auto* self = static_cast<EspNimbleCentral*>(argument);
    if (self != nullptr && event != nullptr) {
        return self->handle_gap_event(*event);
    }
    return 0;
}

int EspNimbleCentral::service_discovery(
    std::uint16_t connection_handle,
    const ble_gatt_error* error,
    const ble_gatt_svc* service,
    void* argument) {
    auto* self = static_cast<EspNimbleCentral*>(argument);
    if (self != nullptr && error != nullptr &&
        connection_handle ==
            self->connection_handle_.load(std::memory_order_acquire)) {
        self->handle_service_discovery(*error, service);
    }
    return 0;
}

int EspNimbleCentral::characteristic_discovery(
    std::uint16_t connection_handle,
    const ble_gatt_error* error,
    const ble_gatt_chr* characteristic,
    void* argument) {
    auto* self = static_cast<EspNimbleCentral*>(argument);
    if (self != nullptr && error != nullptr &&
        connection_handle ==
            self->connection_handle_.load(std::memory_order_acquire)) {
        self->handle_characteristic_discovery(*error, characteristic);
    }
    return 0;
}

int EspNimbleCentral::descriptor_discovery(
    std::uint16_t connection_handle,
    const ble_gatt_error* error,
    std::uint16_t characteristic_value_handle,
    const ble_gatt_dsc* descriptor,
    void* argument) {
    (void)characteristic_value_handle;
    auto* self = static_cast<EspNimbleCentral*>(argument);
    if (self != nullptr && error != nullptr &&
        connection_handle ==
            self->connection_handle_.load(std::memory_order_acquire)) {
        self->handle_descriptor_discovery(*error, descriptor);
    }
    return 0;
}

int EspNimbleCentral::subscription_write(
    std::uint16_t connection_handle,
    const ble_gatt_error* error,
    ble_gatt_attr* attribute,
    void* argument) {
    (void)attribute;
    auto* self = static_cast<EspNimbleCentral*>(argument);
    if (self != nullptr && error != nullptr &&
        connection_handle ==
            self->connection_handle_.load(std::memory_order_acquire)) {
        self->handle_subscription_write(*error);
    }
    return 0;
}

int EspNimbleCentral::firmware_version_read(
    std::uint16_t connection_handle,
    const ble_gatt_error* error,
    ble_gatt_attr* attribute,
    void* argument) {
    auto* self = static_cast<EspNimbleCentral*>(argument);
    if (self != nullptr && error != nullptr &&
        connection_handle ==
            self->connection_handle_.load(std::memory_order_acquire)) {
        self->handle_firmware_version_read(*error, attribute);
    }
    return 0;
}

int EspNimbleCentral::keepalive_write(
    std::uint16_t connection_handle,
    const ble_gatt_error* error,
    ble_gatt_attr* attribute,
    void* argument) {
    (void)attribute;
    auto* self = static_cast<EspNimbleCentral*>(argument);
    if (self != nullptr && error != nullptr &&
        connection_handle ==
            self->connection_handle_.load(std::memory_order_acquire)) {
        self->handle_keepalive_write(*error);
    }
    return 0;
}

bool EspNimbleCentral::start_scan_native() {
    ble_gap_disc_params parameters{};
    parameters.filter_duplicates = 1;
    parameters.passive = 0;
    parameters.itvl = 0;
    parameters.window = 0;
    parameters.filter_policy = 0;
    parameters.limited = 0;

    const int result = ble_gap_disc(
        own_address_type_,
        BLE_HS_FOREVER,
        &parameters,
        gap_event,
        this);
    if (result != 0) {
        ESP_LOGE(tag, "Could not start BLE scan: %d", result);
        return false;
    }
    ESP_LOGI(tag, "Scanning for DashKit");
    return true;
}

int EspNimbleCentral::handle_gap_event(const ble_gap_event& event) {
    switch (event.type) {
    case BLE_GAP_EVENT_DISC: {
        if (event.disc.event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND &&
            event.disc.event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND) {
            break;
        }

        ble_hs_adv_fields fields{};
        if (ble_hs_adv_parse_fields(
                &fields, event.disc.data, event.disc.length_data) != 0 ||
            fields.name == nullptr ||
            fields.name_len != dashkit::profile::device_name.size() ||
            std::memcmp(
                fields.name,
                dashkit::profile::device_name.data(),
                fields.name_len) != 0) {
            break;
        }

        Event found{EventType::DeviceFound, now()};
        std::copy(
            std::begin(event.disc.addr.val),
            std::end(event.disc.addr.val),
            found.device.address.begin());
        found.device.address_type = event.disc.addr.type;
        enqueue(found);
        break;
    }
    case BLE_GAP_EVENT_CONNECT:
        if (event.connect.status == 0) {
            service_start_handle_ = 0;
            service_end_handle_ = 0;
            can_value_handle_ = 0;
            firmware_version_value_handle_ = 0;
            control_value_handle_ = 0;
            characteristic_operation_ = CharacteristicOperation::None;
            connection_handle_.store(
                event.connect.conn_handle, std::memory_order_release);
            bond_peer_known_ = false;
            ble_gap_conn_desc description{};
            if (ble_gap_conn_find(
                    event.connect.conn_handle, &description) == 0) {
                bond_peer_.address_type = description.peer_id_addr.type;
                std::copy(
                    std::begin(description.peer_id_addr.val),
                    std::end(description.peer_id_addr.val),
                    bond_peer_.address.begin());
                bond_peer_known_ = true;
            }
            ble_gap_set_prefered_le_phy(
                event.connect.conn_handle,
                BLE_GAP_LE_PHY_1M_MASK,
                BLE_GAP_LE_PHY_1M_MASK,
                BLE_GAP_LE_PHY_CODED_ANY);
            enqueue(Event{EventType::Connected, now()});
        } else {
            connection_handle_.store(
                BLE_HS_CONN_HANDLE_NONE, std::memory_order_release);
            enqueue(Event{
                EventType::ConnectionFailed,
                now(),
                dashkit::BleFailure::ConnectionFailed});
        }
        break;
    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(
            tag,
            "ATT MTU negotiated: handle=%u channel=%u mtu=%u",
            static_cast<unsigned>(event.mtu.conn_handle),
            static_cast<unsigned>(event.mtu.channel_id),
            static_cast<unsigned>(event.mtu.value));
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        characteristic_operation_ = CharacteristicOperation::None;
        connection_handle_.store(
            BLE_HS_CONN_HANDLE_NONE, std::memory_order_release);
        enqueue(Event{EventType::Disconnected, now()});
        break;
    case BLE_GAP_EVENT_REPEAT_PAIRING: {
        // If DashKit lost its copy of a bond while Tesla Head-Up Display retained one, NimBLE
        // reports repeat pairing. Remove only that peer and retry with fresh
        // keys. DashKit still enforces its own two-minute pairing window.
        ble_gap_conn_desc description{};
        if (ble_gap_conn_find(
                event.repeat_pairing.conn_handle, &description) == 0) {
            (void)ble_store_util_delete_peer(&description.peer_id_addr);
        }
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }
    case BLE_GAP_EVENT_ENC_CHANGE:
        if (event.enc_change.status == 0) {
            enqueue(Event{EventType::Secured, now()});
        } else {
            enqueue(Event{
                EventType::SecurityFailed,
                now(),
                dashkit::BleFailure::SecurityFailed});
        }
        break;
    case BLE_GAP_EVENT_NOTIFY_RX: {
        if (event.notify_rx.conn_handle !=
                connection_handle_.load(std::memory_order_acquire) ||
            event.notify_rx.attr_handle != can_value_handle_ ||
            event.notify_rx.om == nullptr) {
            break;
        }

        const auto packet_length = OS_MBUF_PKTLEN(event.notify_rx.om);
        if (packet_length == 0 || packet_length > max_notification_size) {
            ESP_LOGW(
                tag,
                "Dropping CAN notification of %u bytes",
                static_cast<unsigned>(packet_length));
            break;
        }

        Event notification{EventType::Notification, now()};
        notification.payload_size =
            static_cast<std::uint16_t>(packet_length);
        if (os_mbuf_copydata(
                event.notify_rx.om,
                0,
                packet_length,
                notification.payload.data()) == 0) {
            enqueue(notification);
        }
        break;
    }
    default:
        break;
    }
    return 0;
}

void EspNimbleCentral::handle_service_discovery(
    const ble_gatt_error& error,
    const ble_gatt_svc* service) {
    if (error.status == 0 && service != nullptr) {
        service_start_handle_ = service->start_handle;
        service_end_handle_ = service->end_handle;
        service_found_ = true;
        return;
    }

    if (error.status == BLE_HS_EDONE) {
        Event event{EventType::ServiceDiscovered, now()};
        event.flag = service_found_;
        enqueue(event);
        return;
    }

    Event event{EventType::ServiceDiscovered, now()};
    event.flag = false;
    enqueue(event);
}

void EspNimbleCentral::handle_characteristic_discovery(
    const ble_gatt_error& error,
    const ble_gatt_chr* characteristic) {
    if (error.status == 0 && characteristic != nullptr) {
        pending_value_handle_ = characteristic->val_handle;
        characteristic_found_ = true;
        return;
    }

    if (error.status != BLE_HS_EDONE || !characteristic_found_) {
        fail_characteristic_operation();
        return;
    }

    const auto handle = connection_handle_.load(std::memory_order_acquire);
    switch (characteristic_operation_) {
    case CharacteristicOperation::Subscribe:
        can_value_handle_ = pending_value_handle_;
        if (ble_gattc_disc_all_dscs(
                handle,
                can_value_handle_,
                service_end_handle_,
                descriptor_discovery,
                this) != 0) {
            fail_characteristic_operation();
        }
        break;
    case CharacteristicOperation::ReadFirmwareVersion:
        firmware_version_value_handle_ = pending_value_handle_;
        if (ble_gattc_read(
                handle,
                firmware_version_value_handle_,
                firmware_version_read,
                this) != 0) {
            fail_characteristic_operation();
        }
        break;
    case CharacteristicOperation::WriteKeepalive:
        control_value_handle_ = pending_value_handle_;
        if (ble_gattc_write_flat(
                handle,
                control_value_handle_,
                dashkit::profile::keepalive_command.data(),
                dashkit::profile::keepalive_command.size(),
                keepalive_write,
                this) != 0) {
            fail_characteristic_operation();
        }
        break;
    case CharacteristicOperation::None:
        break;
    }
}

void EspNimbleCentral::handle_descriptor_discovery(
    const ble_gatt_error& error,
    const ble_gatt_dsc* descriptor) {
    if (error.status == 0 && descriptor != nullptr) {
        if (ble_uuid_u16(&descriptor->uuid.u) ==
            BLE_GATT_DSC_CLT_CFG_UUID16) {
            cccd_handle_ = descriptor->handle;
            descriptor_found_ = true;
        }
        return;
    }

    if (error.status != BLE_HS_EDONE || !descriptor_found_) {
        fail_characteristic_operation();
        return;
    }

    constexpr std::array<std::uint8_t, 2> enable_notifications{1, 0};
    const auto handle = connection_handle_.load(std::memory_order_acquire);
    if (ble_gattc_write_flat(
            handle,
            cccd_handle_,
            enable_notifications.data(),
            enable_notifications.size(),
            subscription_write,
            this) != 0) {
        fail_characteristic_operation();
    }
}

void EspNimbleCentral::handle_subscription_write(
    const ble_gatt_error& error) {
    characteristic_operation_ = CharacteristicOperation::None;
    Event event{EventType::SubscriptionChanged, now()};
    event.flag = error.status == 0;
    enqueue(event);
}

void EspNimbleCentral::handle_firmware_version_read(
    const ble_gatt_error& error,
    const ble_gatt_attr* attribute) {
    characteristic_operation_ = CharacteristicOperation::None;
    Event event{EventType::FirmwareVersionRead, now()};
    if (error.status != 0 || attribute == nullptr || attribute->om == nullptr) {
        enqueue(event);
        return;
    }

    const auto length = OS_MBUF_PKTLEN(attribute->om);
    if (length == 0 || length > event.payload.size() ||
        os_mbuf_copydata(attribute->om, 0, length, event.payload.data()) != 0) {
        enqueue(event);
        return;
    }

    event.flag = true;
    event.payload_size = static_cast<std::uint16_t>(length);
    enqueue(event);
}

void EspNimbleCentral::handle_keepalive_write(
    const ble_gatt_error& error) {
    characteristic_operation_ = CharacteristicOperation::None;
    Event event{EventType::KeepaliveWritten, now()};
    event.flag = error.status == 0;
    enqueue(event);
}

void EspNimbleCentral::fail_characteristic_operation() {
    Event event;
    event.timestamp = now();
    switch (characteristic_operation_) {
    case CharacteristicOperation::Subscribe:
        event.type = EventType::SubscriptionChanged;
        break;
    case CharacteristicOperation::ReadFirmwareVersion:
        event.type = EventType::FirmwareVersionRead;
        break;
    case CharacteristicOperation::WriteKeepalive:
        event.type = EventType::KeepaliveWritten;
        break;
    case CharacteristicOperation::None:
        return;
    }
    characteristic_operation_ = CharacteristicOperation::None;
    enqueue(event);
}

void EspNimbleCentral::enqueue(Event event) {
    if (event_queue_ == nullptr ||
        xQueueSend(event_queue_, &event, 0) != pdTRUE) {
        ESP_LOGE(tag, "BLE event queue full; dropping event %u",
                 static_cast<unsigned>(event.type));
    }
}

dashkit::MonotonicTime EspNimbleCentral::now() {
    return dashkit::MonotonicTime{esp_timer_get_time() / 1'000};
}

}  // namespace tesla_headup::platform
