# H619 board profile

This is the default firmware profile for the non-touch LILYGO T-Display-S3
AMOLED 1.91-inch H619 board used by Tesla Head-Up Display.

It selects the ESP32-S3 at 240 MHz, 16 MB QIO flash, octal PSRAM, a large
single-application partition, and the board's RM67162 QSPI pin map. Touch is
not initialized or included in the application. The RM67162 reset on GPIO 17
is active-low; display power on GPIO 38 is active-high.

The profile is selected by `TESLA_HEADUP_BOARD_PROFILE=h619` in the firmware CMake
project. The listed pin map, reset polarity, power control, PSRAM mode, and
production QSPI rendering path have been verified on the selected physical
H619 board. This does not declare other LILYGO revisions compatible; each new
revision still requires its own profile and hardware validation.

## Current road-scene layout

![LILYGO H619 direct-view simulator capture](../../../docs/images/dashboard-road-scene-simulator.png)

The renderer uses the native 536 x 240 resolution, with a 204 px driving region
and a 36 px footer. Inter speed typography, a left power/recuperation rail,
perspective dashes, and mirrored blind-spot sprites share the same canvas in
firmware and the simulator. A red divider requires both occupancy and the
matching active turn signal. This image is synthetic simulator output, not a
hardware photograph.

## Build and flash

Activate ESP-IDF 5.4.4 first. From the repository root, the Windows scripts
select H619 and `road_scene` explicitly:

```bat
flash-firmware.bat COM3
flash-self-test.bat COM3
```

For a manual isolated build, run from `firmware/`:

```sh
idf.py -B build-h619-road-scene -DTESLA_HEADUP_BOARD_PROFILE=h619 -DTESLA_HEADUP_DASHBOARD_LAYOUT=road_scene -DSDKCONFIG=sdkconfig.road_scene build
idf.py -B build-h619-road-scene -p COM3 flash monitor
```

Replace `COM3` with the connected port. Projection mirroring is enabled by
default for the reflective HUD; use `--direct-view` in the simulator to see the
image as viewed after reflection. The generic CMake layout default remains
`minimal`, so specify `road_scene` when configuring manually.

Generated SDKCONFIG and build directories are local artifacts. Checked-in
board/layout defaults are the reproducible source. See [first flash and recovery](../../../docs/first-flash-and-recovery.md)
for pairing, diagnostics, and monitor controls, and the [main README](../../../README.md)
for the Windows simulator prerequisites. Newhaven/DevKitC uses a different
profile and separate flash scripts.
