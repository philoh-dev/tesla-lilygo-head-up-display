# DevKitC N32R16V board profile

## Status and scope

The `devkitc_n32r16v` profile targets the **official Espressif
ESP32-S3-DevKitC-1-N32R16V**, with an ESP32-S3-WROOM-2-N32R16V module.
It is separate from the existing, default `h619` LilyGO profile.

Before wiring, use the [exact Newhaven wiring guide](newhaven-wiring.md).
In particular, PWM needs a level shifter because the breakout pulls it to 5 V,
and the generic `NC` header pin reaches this panel's BIST input.
Display power-on is not cleared: confirm the actual regulator against the
Rev3A panel's VDD current requirement and verify FPC insertion orientation first.

The profile supports **memory-only board bring-up** and the RGB display adapter
for NHD-4.3-800480CF-ASXP Rev3A through NHD-TFT40. The memory-only
bring-up binary does not initialize BLE, NVS, LVGL, RGB, or
backlight GPIOs. Leave the display disconnected for this test. A dark screen
is expected; results appear on the serial monitor.

## Memory and boot configuration

`firmware/boards/devkitc_n32r16v/sdkconfig.defaults` selects:

- ESP32-S3 CPU at 240 MHz;
- 32 MB octal OPI flash, DTR sampling, 80 MHz;
- octal PSRAM at 80 MHz, auto-detected capacity, boot initialization and test;
- PSRAM in the allocation heap, with 64 KiB internal memory reserved;
- PSRAM instruction/rodata fetching to keep RGB refills running during NVS writes;
- no experimental 120 MHz operation, ignored missing PSRAM, or PSRAM ECC;
- the existing single-app-large partition (1500 KiB app), not a new OTA layout;
- UART0 serial console through the USB-to-UART connector.

The application verifies **32 MiB flash and 16 MiB PSRAM** at runtime and
attempts a 1,536,000-byte PSRAM allocation, equivalent to two 800x480 RGB565
framebuffers. It frees that allocation and returns, leaving the FreeRTOS idle
tasks running. Allocation success is not an RGB bandwidth or rendering test.
No eFuses are programmed by this application. Do not manually change memory
voltage eFuses: the module handles its internal 1.8 V memory supply; external
GPIO logic remains 3.3 V.

## Build separately from the LilyGO

With ESP-IDF **5.4.4** activated, from the repository root:

```sh
idf.py -C firmware -B build-devkitc-n32r16v \
  -DIDF_TARGET=esp32s3 \
  -DTESLA_HEADUP_BOARD_PROFILE=devkitc_n32r16v \
  -DTESLA_HEADUP_FIRMWARE_MODE=board_bring_up \
  -DSDKCONFIG=sdkconfig.devkitc_n32r16v build
```

The board profile also selects `board_bring_up` and the isolated sdkconfig
automatically on a **fresh** configure. Explicit flags make repeated commands
unambiguous. Use the same flags and directory for later build/flash/monitor
commands. For example, in an ESP-IDF Windows command prompt:

```bat
idf.py -C firmware -B build-devkitc-n32r16v -DIDF_TARGET=esp32s3 -DTESLA_HEADUP_BOARD_PROFILE=devkitc_n32r16v -DTESLA_HEADUP_FIRMWARE_MODE=board_bring_up -DSDKCONFIG=sdkconfig.devkitc_n32r16v -p COM3 build flash monitor
```

Replace COM3 with the actual board port. Connect only the DevKitC's
**USB-to-UART** port for the first test, using a data-capable cable. Do not
connect an external 5 V source at the same time.

Expected application logs include:

```text
Profile: devkitc_n32r16v (memory-only bring-up)
Flash: 33554432 bytes; PSRAM: 16777216 bytes
Memory checks passed; two RGB565 framebuffer equivalents fit
BLE and display disabled. No RGB or backlight GPIOs driven.
```

ESP-IDF logs should also report successful PSRAM initialization/testing. A
capacity mismatch or allocation failure aborts the application rather than
pretending the board is ready. A flash configuration error can fail before
`app_main`; retain the complete boot log for diagnosis.

Never reuse a LilyGO build directory or generated sdkconfig for this board.
Defaults do not override existing configuration. If the DevKitC configuration
is stale, move its generated sdkconfig and build directory aside, then run the
command above again. CMake rejects incompatible resolved memory settings and
requires RGB/XIP settings for this profile. The root `flash-firmware.bat` and
`flash-self-test.bat` scripts remain **LilyGO-only** and have not changed.

## Windows flashing scripts

Activate the **ESP-IDF 5.4.4 command prompt**, then run one repository-root
script at a time. Replace `COM3` with the DevKitC USB-to-UART port:

```bat
flash-newhaven-memory.bat COM3
flash-newhaven-self-test.bat COM3
flash-newhaven-firmware.bat COM3
```

| Script | Mode | Generated configuration / build directory under `firmware` |
| --- | --- | --- |
| `flash-newhaven-memory.bat` | Memory only; no BLE/display; true_hud layout selector | `sdkconfig.devkitc_n32r16v.memory` / `build-newhaven-memory` |
| `flash-newhaven-self-test.bat` | Road-scene RGB self-test; no BLE | `sdkconfig.devkitc_n32r16v.self-test` / `build-newhaven-self-test` |
| `flash-newhaven-firmware.bat` | Normal road-scene RGB + DashKit BLE | `sdkconfig.devkitc_n32r16v.normal` / `build-newhaven-normal` |

All three explicitly select `devkitc_n32r16v` and `esp32s3`, run
`build flash monitor`, and return the ESP-IDF exit status. The port is optional;
omit it only if ESP-IDF can select the correct device. Scripts work when called
from another directory and support repository paths containing spaces. They
do not erase NVS/settings or program eFuses. Normal firmware applies stored
brightness/mirroring preferences; self-test is mirrored at 100% brightness.

For the memory test, leave the display disconnected and expect a dark screen.
Confirm the memory-success logs above, then exit the monitor with **Ctrl+]**.
Review display wiring and power before running either RGB script. The panel
still needs physical validation; a successful build does not verify wiring.
Use only the DevKitC USB-to-UART connection for initial memory bring-up, with
no simultaneous external 5 V source.

Each mode has a separate configuration to avoid stale mode flags. Defaults
still do not override an existing generated file: if configuration changes,
move that mode's generated sdkconfig and build directory aside before retrying.
The old `flash-firmware.bat` / `flash-self-test.bat` remain LilyGO-only.

## RGB display build

For raw color/orientation/viewport calibration before dashboard rendering, use
the separate [RGB diagnostic mode](rgb-diagnostics.md). It shares the RGB
transport but does not start LVGL/BLE or load NVS preferences. Its dedicated
configuration defaults to direct view and 25% backlight brightness. The pending
regulator and FPC checks still apply before any display power-on.

After memory bring-up, use separate configuration files for each mode/layout.
For the display-only road-scene sequence, from the repository root:

```sh
idf.py -C firmware -B build-newhaven-self-test \
  -DIDF_TARGET=esp32s3 -DTESLA_HEADUP_BOARD_PROFILE=devkitc_n32r16v \
  -DTESLA_HEADUP_FIRMWARE_MODE=display_self_test -DTESLA_HEADUP_DASHBOARD_LAYOUT=road_scene \
  -DSDKCONFIG=sdkconfig.devkitc_n32r16v.self-test build
```

For normal BLE firmware:

```sh
idf.py -C firmware -B build-newhaven-normal \
  -DIDF_TARGET=esp32s3 -DTESLA_HEADUP_BOARD_PROFILE=devkitc_n32r16v \
  -DTESLA_HEADUP_FIRMWARE_MODE=normal -DTESLA_HEADUP_DASHBOARD_LAYOUT=road_scene \
  -DSDKCONFIG=sdkconfig.devkitc_n32r16v.normal build
```

On Windows, enter each command on one line without backslashes. For `true_hud`,
select `-DTESLA_HEADUP_DASHBOARD_LAYOUT=true_hud` with a fresh build/config pair. Exactly
one layout and transport are selected. Replace `build` with `-p COM3 flash monitor`
only after wiring/power review: the new panel has not been physically verified.

### Placement, mirroring and brightness

Run `menuconfig` with the same profile/build/config flags. **Tesla Head-Up Display Newhaven RGB output**
provides `CONFIG_TESLA_HEADUP_RGB_ORIGIN_X` (0–264, default 132) and
`CONFIG_TESLA_HEADUP_RGB_ORIGIN_Y` (0–240, default 0). For `road_scene`,
`CONFIG_TESLA_HEADUP_RGB_FULL_ROAD_SCENE` defaults to enabled: LVGL renders at 800×480
and origin is fixed to `(0,0)`, regardless of the compact-origin configuration.
Disable it for the original viewport. Minimal remains compact. In compact mode these place the unscaled 536×240
dashboard inside the 800×480 black output. Use `(132,120)` to center both axes.
Coordinates refer to the **unmirrored** panel. Mirroring reflects the entire
800-pixel output, so asymmetric placements change physical sides too.

The adapter exposes `set_dashboard_position(x,y)` and
`set_projection_mirrored(bool)` before or after initialization. Placement is
not stored in NVS. Call adapter APIs from the same firmware task as `process()`,
not from an ISR or concurrent task. Normal firmware uses persisted mirroring/brightness settings;
the self-test retains its mirrored, 100%-brightness setup. GPIO2 drives the
NHD-TFT40 PWM input **through the non-inverting level shifter**, not directly
and not through the panel LED power connections. After PWM initialization,
firmware holds the backlight off until the first composed image is handed off;
reset-time behavior depends on the external circuit. 100% is constant high; lower preferences
use 1 kHz LEDC PWM. See [adapter details](display-adapter.md#newhaven-rgb-display-adapter).

## Desktop RGB preview

Before wiring hardware, run from the repository root:

```sh
TESLA_HEADUP_DASHBOARD_LAYOUT=road_scene ./scripts/simulate.sh --rgb --direct-view
```

The road scene fills the panel by default. Add
`--rgb-viewport --origin-x 132 --origin-y 120` for a centered compact viewport, or move it with
I/J/K/L (up/left/down/right; Shift for ten pixels). The window title reports
the unmirrored origin. M mirrors the entire 800×480 output. All other simulator
telemetry controls remain available. Origins match the firmware ranges and
defaults; adjustments here do not save to NVS or modify the firmware config.
Use `--screenshot /absolute/path/preview.bmp` for a deterministic native-size
capture. Omit `--direct-view` to capture the mirrored image.

## GPIO mapping for the RGB adapter

Actual SDL native 800×480 road-scene preview, unmirrored at `(0,0)`:

![Newhaven RGB output preview](images/newhaven-rgb-road-scene.png)

The source of truth is `firmware/boards/devkitc_n32r16v/board.hpp`. These are
**ESP32 GPIO numbers**, not physical header positions or NHD-TFT40 pin numbers.
The adapter uses this mapping. For physical header positions, FPC pin numbers,
PWM level shifting, power and first-power checks, use [newhaven-wiring.md](newhaven-wiring.md).

| Signal | GPIOs, low-to-high colour bit |
| --- | --- |
| Blue B3–B7 | 4, 5, 6, 7, 8 |
| Green G2–G7 | 9, 10, 11, 12, 13, 14 |
| Red R3–R7 | 15, 16, 17, 18, 21 |
| Pixel clock | 39 |
| HSYNC | 40 |
| VSYNC | 41 |
| DE | 42 |
| DISP enable | 1 |
| Backlight PWM | 2 |

Reserved/excluded from display use:

- GPIO26–37: flash/PSRAM or not available on the module headers; in particular
  GPIO35–37 **must not** be used on the WROOM-2.
- GPIO0/3/45/46: boot-strapping pins.
- GPIO19/20: native USB, kept available for debugging/flashing.
- GPIO43/44: UART0, kept available for the USB-to-UART console.
- GPIO38 and GPIO48: avoid the onboard RGB LED on both documented board revisions.

GPIO39–42 are multiplexed with external JTAG; this reservation means they
cannot simultaneously be used for an external JTAG probe. Native USB JTAG is
left available. Static assertions and host tests check uniqueness and exclusions.

The panel has 24 colour inputs, while the output is RGB565. The wiring guide
grounds the unused lower bits and BIST, separates supply outputs and identifies
the PWM interface. Confirm the actual FPC contact side/pin 1 before insertion;
do not connect header and FPC pins by matching their numbers.

## Verification and remaining work

CI builds memory bring-up and both RGB layouts in normal and self-test modes
with ESP-IDF 5.4.4, alongside existing LilyGO builds. All use the binary-size
budget. Host tests cover isolation, memory/pins, native RGB565 partial updates,
placement, mirroring, black margins and framebuffer ownership handoff.

Still required: physical boot verification, display timing and
pixel-format validation, PWM/electrical verification, reviewed power distribution and
measurement, and confirmation of the actual FPC connector orientation. Successful memory bring-up
does not establish frame rate, USB supply sufficiency, daylight readability, or
car-temperature suitability.

## Manufacturer references

- [Espressif DevKitC-1 guide and pinout](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html)
- [ESP-IDF 5.4.4 flash and PSRAM configuration](https://docs.espressif.com/projects/esp-idf/en/v5.4.4/esp32s3/api-reference/kconfig.html)
- [Newhaven panel datasheet](https://newhavendisplay.com/content/specs/NHD-4.3-800480CF-ASXP.pdf)
- [NHD-TFT40 datasheet](https://newhavendisplay.com/content/specs/NHD-TFT40.pdf)
