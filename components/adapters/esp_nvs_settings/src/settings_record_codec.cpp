#include "tesla_headup/adapters/esp_nvs_settings/settings_record_codec.hpp"

namespace tesla_headup::platform {
namespace {

constexpr std::uint8_t vehicle_speed_bit = 1U << 0U;
constexpr std::uint8_t turn_signals_bit = 1U << 1U;
constexpr std::uint8_t exterior_lights_bit = 1U << 2U;
constexpr std::uint8_t autopilot_bit = 1U << 3U;
constexpr std::uint8_t blind_spot_bit = 1U << 4U;
constexpr std::uint8_t selected_gear_bit = 1U << 5U;
constexpr std::uint8_t known_feature_bits = vehicle_speed_bit |
                                             turn_signals_bit |
                                             exterior_lights_bit |
                                             autopilot_bit |
                                             blind_spot_bit |
                                             selected_gear_bit;

}  // namespace

SettingsRecord encode_settings(const domain::Settings& settings) noexcept {
    std::uint8_t features = 0;
    features |= settings.enabled_features.vehicle_speed ? vehicle_speed_bit : 0;
    features |= settings.enabled_features.turn_signals ? turn_signals_bit : 0;
    features |=
        settings.enabled_features.exterior_lights ? exterior_lights_bit : 0;
    features |= settings.enabled_features.autopilot ? autopilot_bit : 0;
    features |=
        settings.enabled_features.blind_spot_warnings ? blind_spot_bit : 0;
    features |= settings.enabled_features.selected_gear ? selected_gear_bit : 0;

    return SettingsRecord{
        .version = SettingsRecord::current_version,
        .units = static_cast<std::uint8_t>(settings.units),
        .projection_mirrored = static_cast<std::uint8_t>(
            settings.projection_mirrored ? 1 : 0),
        .brightness_percent = settings.brightness.percent(),
        .enabled_feature_bits = features,
    };
}

std::optional<domain::Settings> decode_settings(
    SettingsRecord record) noexcept {
    const auto brightness = domain::BrightnessPreference::from_percent(
        record.brightness_percent);
    if (record.version != SettingsRecord::current_version ||
        record.units > static_cast<std::uint8_t>(domain::UnitSystem::Imperial) ||
        record.projection_mirrored > 1 || !brightness.has_value() ||
        (record.enabled_feature_bits &
         static_cast<std::uint8_t>(~known_feature_bits)) != 0) {
        return std::nullopt;
    }

    return domain::Settings{
        .units = static_cast<domain::UnitSystem>(record.units),
        .projection_mirrored = record.projection_mirrored != 0,
        .brightness = *brightness,
        .enabled_features = {
            .vehicle_speed =
                (record.enabled_feature_bits & vehicle_speed_bit) != 0,
            .turn_signals =
                (record.enabled_feature_bits & turn_signals_bit) != 0,
            .exterior_lights =
                (record.enabled_feature_bits & exterior_lights_bit) != 0,
            .autopilot =
                (record.enabled_feature_bits & autopilot_bit) != 0,
            .blind_spot_warnings =
                (record.enabled_feature_bits & blind_spot_bit) != 0,
            .selected_gear =
                (record.enabled_feature_bits & selected_gear_bit) != 0,
        },
    };
}

}  // namespace tesla_headup::platform
