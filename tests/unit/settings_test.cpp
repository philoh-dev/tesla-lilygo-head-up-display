#include "test.hpp"

#include "tesla_headup/adapters/in_memory_settings/in_memory_settings_store.hpp"
#include "tesla_headup/adapters/esp_nvs_settings/settings_record_codec.hpp"
#include "tesla_headup/application/settings/settings_service.hpp"
#include "tesla_headup/domain/settings.hpp"

namespace {

using tesla_headup::adapters::InMemorySettingsStore;
using tesla_headup::application::LoadSettingsStatus;
using tesla_headup::application::SaveSettingsStatus;
using tesla_headup::application::SettingsService;
using tesla_headup::domain::BrightnessPreference;
using tesla_headup::domain::Settings;
using tesla_headup::domain::UnitSystem;
using tesla_headup::platform::SettingsRecord;
using tesla_headup::platform::decode_settings;
using tesla_headup::platform::encode_settings;

TEST_CASE("settings defaults are safe and enable the driving features") {
    const Settings settings;

    CHECK_EQ(settings.units, UnitSystem::Metric);
    CHECK(settings.projection_mirrored);
    CHECK_EQ(settings.brightness.percent(), 100);
    CHECK(settings.enabled_features.vehicle_speed);
    CHECK(settings.enabled_features.turn_signals);
    CHECK(settings.enabled_features.exterior_lights);
    CHECK(settings.enabled_features.autopilot);
    CHECK(settings.enabled_features.blind_spot_warnings);
    CHECK(settings.enabled_features.selected_gear);
}

TEST_CASE("brightness preference accepts only the supported percentage") {
    CHECK(!BrightnessPreference::from_percent(0).has_value());
    CHECK_EQ(BrightnessPreference::from_percent(1)->percent(), 1);
    CHECK_EQ(BrightnessPreference::from_percent(50)->percent(), 50);
    CHECK_EQ(BrightnessPreference::from_percent(100)->percent(), 100);
    CHECK(!BrightnessPreference::from_percent(101).has_value());
    CHECK(!BrightnessPreference::from_percent(255).has_value());
}

TEST_CASE("settings service returns defaults when storage is empty") {
    InMemorySettingsStore store;
    SettingsService service{store};

    const auto loaded = service.load();

    CHECK_EQ(loaded.status, LoadSettingsStatus::NotFound);
    CHECK_EQ(loaded.settings, Settings{});
}

TEST_CASE("in-memory settings adapter round trips every setting") {
    InMemorySettingsStore store;
    SettingsService service{store};
    Settings expected;
    expected.units = UnitSystem::Imperial;
    expected.projection_mirrored = false;
    expected.brightness = *BrightnessPreference::from_percent(37);
    expected.enabled_features.vehicle_speed = false;
    expected.enabled_features.turn_signals = false;
    expected.enabled_features.exterior_lights = false;
    expected.enabled_features.autopilot = false;
    expected.enabled_features.blind_spot_warnings = false;
    expected.enabled_features.selected_gear = false;

    CHECK_EQ(service.save(expected), SaveSettingsStatus::Saved);
    const auto loaded = service.load();

    CHECK(store.has_value());
    CHECK_EQ(loaded.status, LoadSettingsStatus::Loaded);
    CHECK_EQ(loaded.settings, expected);
}

TEST_CASE("settings service preserves storage failure information") {
    InMemorySettingsStore store;
    SettingsService service{store};
    store.fail_load(true);
    store.fail_save(true);

    const auto loaded = service.load();

    CHECK_EQ(loaded.status, LoadSettingsStatus::StorageError);
    CHECK_EQ(loaded.settings, Settings{});
    CHECK_EQ(service.save(Settings{}), SaveSettingsStatus::StorageError);
}

TEST_CASE("NVS record codec preserves every settings field") {
    Settings expected;
    expected.units = UnitSystem::Imperial;
    expected.projection_mirrored = false;
    expected.brightness = *BrightnessPreference::from_percent(23);
    expected.enabled_features.vehicle_speed = false;
    expected.enabled_features.turn_signals = true;
    expected.enabled_features.exterior_lights = false;
    expected.enabled_features.autopilot = true;
    expected.enabled_features.blind_spot_warnings = false;
    expected.enabled_features.selected_gear = true;

    const auto decoded = decode_settings(encode_settings(expected));

    CHECK(decoded.has_value());
    CHECK_EQ(*decoded, expected);
}

TEST_CASE("NVS record codec rejects unknown or corrupt values") {
    SettingsRecord record;

    record.version = SettingsRecord::current_version + 1;
    CHECK(!decode_settings(record).has_value());
    record = SettingsRecord{};
    record.units = 2;
    CHECK(!decode_settings(record).has_value());
    record = SettingsRecord{};
    record.projection_mirrored = 2;
    CHECK(!decode_settings(record).has_value());
    record = SettingsRecord{};
    record.brightness_percent = 0;
    CHECK(!decode_settings(record).has_value());
    record = SettingsRecord{};
    record.brightness_percent = 101;
    CHECK(!decode_settings(record).has_value());
    record = SettingsRecord{};
    record.enabled_feature_bits = 0x80;
    CHECK(!decode_settings(record).has_value());
}

}  // namespace
