# Settings

## Model

Settings are domain values and contain no ESP-IDF or NVS types:

- unit system: metric (`km/h`) or imperial (`mph`);
- windshield-projection mirroring;
- display brightness from 1 through 100 percent; and
- enabled state for speed, selected gear, turn signals, Autopilot, and
  blind-spot warnings.

The serialized exterior-light preference is retained only for compatibility
with existing NVS records. No exterior-light feature is registered by the
current firmware.

Connection and stale-data status is safety-critical and is therefore always
enabled rather than exposed as a configurable dashboard feature. Vehicle speed
remains canonical in km/h throughout decoding and telemetry storage. Imperial
selection converts only the rounded presentation value.

Defaults select metric units, projection mirroring, 100 percent brightness,
and all configurable features enabled.

Both display adapters use these preferences. LilyGO uses the RM67162 brightness
command; Newhaven uses the NHD-TFT40 PWM input through a non-inverting level
shifter (its pull-up is 5 V; see [wiring](newhaven-wiring.md)). Mirroring is applied once at
final output, not per symbol. Newhaven placement is separate build configuration
(`CONFIG_TESLA_HEADUP_RGB_ORIGIN_X/Y`) or an adapter API call, not persisted settings.
Full-panel RGB road scene fixes origin to `(0,0)`; compact-origin preferences
apply only when `CONFIG_TESLA_HEADUP_RGB_FULL_ROAD_SCENE` is disabled or minimal is selected.

## Ports and application behavior

`SettingsReader` and `SettingsWriter` form the persistence boundary;
`SettingsStore` combines them for adapters that support both operations.
`SettingsService` returns stored settings when available and otherwise returns
safe defaults alongside the original load status. This keeps missing, invalid,
and inaccessible storage distinguishable.

The dashboard settings applier maps feature preferences to registered feature
slots. Speed has an additional permission in `VehicleSpeedPresenter` so fresh
telemetry cannot re-enable a feature disabled by configuration. Selected gear
uses the same permission gate. Enabling either feature still does not display
it until fresh telemetry is present.

## Adapters

`InMemorySettingsStore` supports deterministic success and failure tests
without external storage.

`EspNvsSettingsStore` persists version, units, mirroring, brightness, and a
feature bit mask as scalar values in the `tesla_headup` NVS namespace. A
record is accepted only when every key exists and every value is valid. Unknown
versions, partial records, invalid percentages, and unknown feature bits are
rejected. A missing or invalid record is replaced with the current defaults at
boot; a storage I/O error uses defaults for that boot without attempting a
write.

The firmware loads settings before display initialization. It preconfigures
mirroring and brightness before the panel is switched on, then applies feature
preferences before the first dashboard frame. Brightness percentage is
converted to the RM67162's 8-bit `0x51` display-brightness command. The
display-self-test has exercised 100% brightness on the H619; the complete
1–100% range and persistence across repeated power cycles still require
measurement.

## Changing settings

There is currently no on-device settings UI or supported serial command for
editing the stored record. Firmware persists the domain defaults on first boot
and loads that record on subsequent boots. During development, change defaults
in `components/domain/include/tesla_headup/domain/settings.hpp`, rebuild, erase NVS,
and flash again. Erasing NVS also removes local BLE bonds; see
[First flash and recovery](first-flash-and-recovery.md).

`menuconfig` controls compile-time display and layout options, not the persisted
unit, brightness, mirroring, or feature record. A future settings input adapter
can use `SettingsWriter` without coupling the domain to its transport.

The storage namespace is `tesla_headup` (11 characters), within ESP32 NVS's
15-character limit. Preferences from pre-rename firmware are not migrated;
first boot with this namespace uses defaults until settings are saved again.
