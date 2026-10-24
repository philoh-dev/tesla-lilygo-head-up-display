# First flash and recovery

## Supported first-bring-up target

For the **ESP32-S3-DevKitC-1-N32R16V**, use the separate
[DevKitC guide](devkitc-n32r16v.md) for memory and Newhaven RGB bring-up.
Its [Windows scripts](devkitc-n32r16v.md#windows-flashing-scripts) provide
separate memory, display-self-test and normal BLE firmware entry points.
Complete the [Newhaven wiring checks](newhaven-wiring.md#first-power-checklist)
before either display stage, including PWM level shifting and FPC orientation.
The commands and display self-test below target the **LilyGO H619** only.

The default firmware target is the non-touch LILYGO T-Display-S3 AMOLED
1.91-inch H619 board. Its profile selects ESP32-S3, 16 MB QIO flash, octal
PSRAM, the RM67162 QSPI pins, and a 1500 KiB application partition. The
application contains no touch dependency or interaction.

The build requires ESP-IDF 5.4.4. LVGL 9.3.0 is resolved through the checked-in
`firmware/dependencies.lock`; the board-specific RM67162 transport is part of
the display adapter. The first clean build therefore requires internet access
to the Espressif component registry; later builds can use its local component
cache.

## Windows flash launcher

From the repository root, build and flash the LILYGO H619 `true_hud` layout:

```powershell
.\scripts\flash.ps1 COM3
```

Or run `flash-true-hud.bat COM3` from a command prompt. Omit the
port for ESP-IDF auto-detection. The launcher finds an activated ESP-IDF or the
standard Windows ESP-IDF 5.4.4 installation and activates its environment.
Use `-IdfPath C:\path\to\esp-idf` for another installation location.

```powershell
.\scripts\flash.ps1 COM3 -Layout road_scene
.\scripts\flash.ps1 COM3 -Mode display_self_test
.\scripts\flash.ps1 COM3 -NoMonitor -Baud 115200
.\scripts\flash.ps1 -BuildOnly
.\scripts\flash.ps1 COM3 -DryRun
```

Only `true_hud` and `road_scene` are supported. Every layout/mode pair uses its
own `build-flash-h619-<layout>-<mode>/` directory and SDKCONFIG inside it.
The script builds, enforces the 1,179,648-byte firmware budget, then flashes
and opens the serial monitor. Build or size-check failures stop before flashing.
`-NoMonitor` skips the monitor; `-BuildOnly` stops after the size check.
`-DryRun` prints the commands without needing an SDK or accessing the board.
Exit the serial monitor with `Ctrl+]`. Newhaven/DevKitC uses its separate scripts.

## Build

From a clean checkout with ESP-IDF activated:

```sh
cd firmware
idf.py set-target esp32s3
idf.py build
python3 ../scripts/check_firmware_size.py \
  build/tesla_headup_firmware.bin --max-bytes 1179648
```

The `true_hud` dashboard is the default. To compile the road-scene layout into a
separate build directory instead, run:

```sh
cd firmware
idf.py -B build-road-scene \
  -DSDKCONFIG=sdkconfig.road-scene \
  -DTESLA_HEADUP_DASHBOARD_LAYOUT=road_scene build
python3 ../scripts/check_firmware_size.py \
  build-road-scene/tesla_headup_firmware.bin --max-bytes 1179648
```

The firmware CMake project selects `TESLA_HEADUP_BOARD_PROFILE=h619` by default.
Generated `firmware/sdkconfig*` files are ignored by Git; the checked-in
defaults are authoritative. A configuration generated before a defaults
change can retain old values. In that case, move the generated configuration
aside and rerun the corresponding build. For example, after a normal
road-scene firmware change on Windows, delete
`firmware\sdkconfig.road-scene` before rerunning
`flash-firmware.bat COM3`.

## Display-only self-test

Before introducing DashKit, compile the road-scene layout with the dedicated
self-test mode and a separate generated configuration:

```sh
cd firmware
idf.py -B build-display-self-test \
  -DSDKCONFIG=sdkconfig.display-self-test \
  -DTESLA_HEADUP_DASHBOARD_LAYOUT=road_scene \
  -DTESLA_HEADUP_FIRMWARE_MODE=display_self_test build
idf.py -B build-display-self-test flash monitor
```

On Windows, the repository-root scripts perform the complete configure, build,
flash, and monitor operation. Pass the serial port when auto-detection is not
possible:

```bat
flash-self-test.bat COM3
flash-firmware.bat COM3
```

Omit `COM3` to let ESP-IDF select the port. The scripts use separate build
directories and generated `sdkconfig` files, so switching between the
self-test and normal firmware does not contaminate either configuration. Run
them from an ESP-IDF command prompt and exit the monitor with `Ctrl+]`.

This resolves `CONFIG_TESLA_HEADUP_DISPLAY_SELF_TEST=y` from
`modes/display_self_test.sdkconfig.defaults`. The firmware does not initialize
NimBLE or scan for DashKit. It forces metric units and every registered
dashboard feature on, uses windshield-projection mirroring and 100% brightness
by default, and publishes fresh synthetic telemetry every 100 ms through
`TelemetryStore`.

The deterministic sequence starts with two seconds in Park and every other
visual inactive. It then:

1. selects Drive and accelerates from 0 to 130 km/h over seven seconds;
2. holds 130 km/h for five seconds;
3. enables the left turn signal, then adds a left blind-spot warning one
   second later while leaving the signal active;
4. clears the blind-spot warning after three seconds, leaves the signal active
   for another three seconds, then clears it;
5. activates Autopilot for five seconds at 130 km/h, switches it off, and
   decelerates from 130 to 0 km/h over 15 seconds;
6. waits one second, selects Reverse, and accelerates to 15 km/h over three
   seconds;
7. activates speed, selected gear, each turn-signal state, each blind-spot
   side, Autopilot, the speed-limit sign, power use, and recuperation
   individually, clearing each one before continuing; and
8. settles after 73 seconds with all retained compatible visuals active.

Synthetic battery telemetry drives the power rail upward during acceleration
and downward during braking. The final feature sweep tests +50% power use and
-50% recuperation separately; the final steady scene uses +25% power use.

Turn lamps alternate every 500 ms. Reverse exercises opposite road motion,
and active Autopilot exercises blue lane markings. Once per second the serial
monitor reports completed refreshes per second, average/maximum refresh
duration, flush-submission overhead, flush count, and current/minimum internal
RAM and PSRAM. Flush-submission time measures the software queue operation; it
is not the asynchronous QSPI transfer time.

The self-test always mirrors the completed frame for validation through the C1
combiner and drives the AMOLED at 100% brightness. Text therefore appears
reversed when the AMOLED is viewed directly. Normal firmware uses its
persisted projection-mirroring and brightness preferences. Keep this build
directory and `sdkconfig.display-self-test` separate from the normal firmware;
selecting `TESLA_HEADUP_FIRMWARE_MODE=normal` excludes the synthetic source at compile
time.

If an existing build directory reports `--chip esp32` for this ESP32-S3 board,
it was configured for the wrong target. Remove or `fullclean` that build
directory, run `idf.py set-target esp32s3`, and configure the selected mode
again. Do not reuse a build directory created for a different target or mode.

## Flash and monitor

Connect the board directly over a data-capable USB cable, then run:

```sh
idf.py -C firmware flash monitor
```

Pass `-p PORT` to both operations if ESP-IDF cannot select the serial device.
Exit the monitor with `Ctrl+]`.

On the first boot, the expected sequence is:

1. NVS initializes and stores default dashboard settings.
2. BLE scanning for the advertised name `DashKit` starts immediately.
3. The RM67162 initializes and shows the dashboard.
4. The status line progresses through `CONNECTING`, `WAITING FOR DATA`, and
   then disappears after valid telemetry arrives.

No vehicle value should remain visible after its freshness timeout.

## Diagnostic CAN capture

For lag or disconnect investigation, normal H619 firmware already emits
`Display performance` every five seconds, including maximum refresh time,
internal/PSRAM memory, LVGL free space/fragmentation and stack headroom.
`BLE disconnected: reason=...` records the native disconnect reason.
`BLE queue overflow: notifications=... control=...` reports dropped callback
events at most once per second rather than logging every dropped packet in the
NimBLE task. The application drains a bounded queue snapshot on each pass so
incoming telemetry cannot indefinitely postpone connection timers.

Diagnostic capture is disabled by default. Enable it only for controlled
bring-up:

```sh
idf.py -C firmware menuconfig
```

Open **Tesla Head-Up Display diagnostics**, enable **Log a bounded sample of received CAN
frames**, and select the maximum frames logged per second. The default is 32.
Rebuild and flash afterward.

Each accepted record includes local receipt time, DashKit source timestamp,
bus, CAN identifier, payload length, and up to eight payload bytes. Invalid
DashKit packets produce no frame logs. Excess frames are suppressed and a
summary is logged when the next one-second window begins.

CAN captures can reveal vehicle configuration, behavior, or location-derived
activity. Review and redact them before sharing or committing them.

## Pairing recovery

Tesla Head-Up Display uses encrypted, bonded BLE. It automatically removes the active local
DashKit bond and retries once when stored keys are stale.

If the HUD reports `PAIRING REQUIRED`, DashKit may still be bonded to another
client. Use an already-paired DashPilot client to open DashKit's pairing window,
or follow DashKit's official factory-reset procedure, then restart Tesla Head-Up Display. Tesla Head-Up Display
does not send the vehicle-control pairing command automatically.

## Reset local NVS

Settings and BLE bonds share the ESP32's NVS partition. The supported complete
reset is:

```sh
idf.py -C firmware erase-flash
idf.py -C firmware flash monitor
```

This erases the firmware, dashboard settings, and every local BLE bond. It does
not clear bonds stored by DashKit. Default settings are written again on the
next boot.

Do not use a hard-coded flash offset to erase only NVS; offsets depend on the
active partition table.

## Flash-mode recovery

If automatic reset cannot enter the ROM bootloader:

1. Disconnect USB power.
2. Hold the board's `BOOT` control.
3. Reconnect USB or press `RESET` while continuing to hold `BOOT`.
4. Release `BOOT` after the serial device appears.
5. Run the flash command again with the explicit port.

After a successful flash, press `RESET` once if the application does not start
automatically.

## Hardware acceptance checks

Basic H619 QSPI output, landscape RGB order, reset/power polarity, PSRAM
startup, encrypted DashKit setup, and notification delivery have been observed
successfully. Before mounting the HUD in a vehicle, additionally verify:

- windshield/combiner mirror axis and optical alignment;
- active-low panel reset and active-high display power across repeated cold
  boots and manual resets;
- the full brightness range at boot;
- BLE pairing, notification flow, disconnect, and automatic reconnect;
- live speed against the vehicle display at rest and at controlled speeds;
- turn, hazard, blind-spot, Autopilot, and speed-limit mappings;
- heap stability and frame rate during an extended bench run; and
- readable but non-distracting brightness in daylight and darkness.
