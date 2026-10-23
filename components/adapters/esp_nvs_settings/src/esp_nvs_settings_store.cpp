#include "tesla_headup/adapters/esp_nvs_settings/esp_nvs_settings_store.hpp"
#include "tesla_headup/adapters/esp_nvs_settings/settings_record_codec.hpp"

#include "nvs.h"

#include <cstdint>
#include <initializer_list>

namespace tesla_headup::platform {
namespace {

constexpr char settings_namespace[] = "tesla_headup";
static_assert(sizeof(settings_namespace) <= 16, "NVS namespaces allow at most 15 characters");
constexpr char version_key[] = "version";
constexpr char units_key[] = "units";
constexpr char mirror_key[] = "mirror";
constexpr char brightness_key[] = "brightness";
constexpr char features_key[] = "features";

class NvsHandle final {
public:
    ~NvsHandle() {
        if (handle != 0) {
            nvs_close(handle);
        }
    }

    NvsHandle(const NvsHandle&) = delete;
    NvsHandle& operator=(const NvsHandle&) = delete;
    NvsHandle() = default;

    nvs_handle_t handle{0};
};

application::LoadSettingsStatus read_u8(
    nvs_handle_t handle,
    const char* key,
    std::uint8_t& destination) noexcept {
    const esp_err_t result = nvs_get_u8(handle, key, &destination);
    if (result == ESP_OK) {
        return application::LoadSettingsStatus::Loaded;
    }
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        return application::LoadSettingsStatus::InvalidData;
    }
    return application::LoadSettingsStatus::StorageError;
}

}  // namespace

application::LoadSettingsStatus EspNvsSettingsStore::load(
    domain::Settings& destination) noexcept {
    NvsHandle storage;
    const esp_err_t open_result = nvs_open(
        settings_namespace,
        NVS_READONLY,
        &storage.handle);
    if (open_result == ESP_ERR_NVS_NOT_FOUND) {
        return application::LoadSettingsStatus::NotFound;
    }
    if (open_result != ESP_OK) {
        return application::LoadSettingsStatus::StorageError;
    }

    SettingsRecord record;
    const esp_err_t version_result =
        nvs_get_u8(storage.handle, version_key, &record.version);
    if (version_result == ESP_ERR_NVS_NOT_FOUND) {
        return application::LoadSettingsStatus::NotFound;
    }
    if (version_result != ESP_OK) {
        return application::LoadSettingsStatus::StorageError;
    }
    for (const auto status : {
             read_u8(storage.handle, units_key, record.units),
             read_u8(
                 storage.handle,
                 mirror_key,
                 record.projection_mirrored),
             read_u8(
                 storage.handle,
                 brightness_key,
                 record.brightness_percent),
             read_u8(
                 storage.handle,
                 features_key,
                 record.enabled_feature_bits)}) {
        if (status != application::LoadSettingsStatus::Loaded) {
            return status;
        }
    }

    const auto decoded = decode_settings(record);
    if (!decoded.has_value()) {
        return application::LoadSettingsStatus::InvalidData;
    }
    destination = *decoded;
    return application::LoadSettingsStatus::Loaded;
}

application::SaveSettingsStatus EspNvsSettingsStore::save(
    const domain::Settings& settings) noexcept {
    NvsHandle storage;
    if (nvs_open(settings_namespace, NVS_READWRITE, &storage.handle) != ESP_OK) {
        return application::SaveSettingsStatus::StorageError;
    }

    const auto record = encode_settings(settings);
    if (nvs_set_u8(storage.handle, version_key, record.version) != ESP_OK ||
        nvs_set_u8(storage.handle, units_key, record.units) != ESP_OK ||
        nvs_set_u8(
            storage.handle,
            mirror_key,
            record.projection_mirrored) != ESP_OK ||
        nvs_set_u8(
            storage.handle,
            brightness_key,
            record.brightness_percent) != ESP_OK ||
        nvs_set_u8(
            storage.handle,
            features_key,
            record.enabled_feature_bits) != ESP_OK ||
        nvs_commit(storage.handle) != ESP_OK) {
        return application::SaveSettingsStatus::StorageError;
    }
    return application::SaveSettingsStatus::Saved;
}

}  // namespace tesla_headup::platform
