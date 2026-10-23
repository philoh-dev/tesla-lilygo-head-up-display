# Hardware

## Display unit

The target device is the LILYGO T-Display-S3 AMOLED 1.91-inch board.

Confirmed family specifications:

| Property | Value |
| --- | --- |
| MCU | ESP32-S3R8 |
| Flash | 16 MB |
| PSRAM | 8 MB OPI |
| Display controller | RM67162 |
| Resolution | 536 x 240 pixels |
| Display interface | QSPI |
| Wireless | Wi-Fi and Bluetooth Low Energy |

The selected unit is the non-touch H619 variant. Its dedicated firmware profile
records the 16 MB flash, octal PSRAM, and confirmed RM67162 QSPI pin map. The
physical board has rendered both raw color frames and the production LVGL
dashboard. GPIO 17 is the active-low panel reset and GPIO 38 enables display
power when driven high.

## Newhaven alternative

The separate `devkitc_n32r16v` profile targets ESP32-S3-WROOM-2-N32R16V
(32 MB octal flash, 16 MB octal PSRAM) with NHD-4.3-800480CF-ASXP Rev3A and
NHD-TFT40. Its LCD/RGB adapter outputs native 800×480 for the road scene, or an
adjustable unscaled 536×240 viewport, with final-output mirroring and PWM backlight control.
This is implemented and cross-compiled, but **not hardware validated**. Follow
the [separate board guide](devkitc-n32r16v.md); H619 pin/power/flash instructions
must not be reused. Wiring, power distribution, optics and brightness still
need verification on the new assembly.

Use the [exact wiring guide](newhaven-wiring.md) for header/FPC numbering,
grounded unused bits/BIST, separate power rails and the required PWM level
shifter. Do not wire GPIO2 directly to the breakout's 5 V-pulled-up PWM input.
The published MIC5225 regulator rating is below the Rev3A panel's stated logic
current. Resolve the actual fitted regulator/current capacity and FPC orientation
before powering the display; the compatibility listing alone is insufficient.

## Reused C1 HUD hardware

A C1 HUD serves as the donor for the housing and reflective combiner. The
planned assembly replaces its original display unit with the LILYGO H619
AMOLED and ESP32-S3 running Tesla Head-Up Display. The C1's original electronics and firmware
are not part of the Tesla Head-Up Display software stack.

Mechanical fit, the visible image area, focus, brightness, and final mirror
direction still need validation with the assembled C1 optics. The display
self-test renders a mirrored image for this setup; normal firmware uses the
persisted projection-mirroring preference.

## Vehicle interface

DashKit is a separate ESP32-S3 device connected to two Tesla CAN buses through
MCP2518FD controllers. Its firmware exposes vehicle data over BLE. Tesla Head-Up Display acts
as a BLE central/client and does not connect directly to the CAN wiring.

## Remaining hardware questions

- orientation and mounting position;
- power source, ignition behavior, and brownout characteristics;
- acceptable boot time and the complete display brightness range;
- thermal behavior in a parked vehicle;
- BLE range and coexistence in the final mounting positions;
- enclosure, reflections, night brightness, and burn-in mitigation.

## Bring-up status

- [x] Confirm the non-touch H619 variant and its QSPI pin map.
- [x] Confirm 536 x 240 landscape output, RGB order, reset polarity, and display
  power control with the project transport.
- [x] Confirm octal PSRAM initialization and its boot-time memory test.
- [x] Establish an encrypted DashKit connection, discover the GATT profile, and
  receive notifications.
- [ ] Measure boot, idle, and full-brightness current.
- [ ] Record negotiated MTU, notification rate, and queue occupancy during a
  representative drive.
- [ ] Exercise repeated disconnect/reconnect, bond recovery, and power-cycle
  behavior.
- [ ] Measure frame rate, latency, heap, and stack headroom during an extended
  BLE-plus-rendering run.
- [ ] Record firmware and hardware versions with every future capture.

## Safety and installation

The device must be mounted securely, avoid obstructing the driver's view, and
remain readable without creating hazardous glare. Power wiring and placement
must account for airbags, vehicle controls, and cabin temperature extremes.
