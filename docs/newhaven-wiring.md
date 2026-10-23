# Newhaven display wiring guide

Connect **ESP32-S3-DevKitC-1-N32R16V**, **NHD-TFT40**, and
**NHD-4.3-800480CF-ASXP Rev3A** using the `devkitc_n32r16v` firmware profile.
This is a bench-prototype guide, not a finished vehicle power harness.

**Power-on is blocked pending regulator and FPC verification.** Memory-only
MCU testing is safe to prepare with the breakout unpowered/disconnected.
Do not proceed to the display-power steps until the two checks below are closed.

**Do not connect GPIO2 directly to the breakout PWM pin.** Its schematic
contains a 10 kOhm pull-up to 5 V. Use the non-inverting level shifter below.
The generic breakout header labeled `NC` also needs special treatment: it
reaches this panel's BIST input and must be held low.

The electrical map follows the manufacturer schematics and current firmware.
Before applying power, confirm your board revision, FPC pin 1 and connector
contact side. The connector's physical insertion orientation still needs a
clear photo of your actual connector and both sides of the FPC tip; do not
guess "contacts up/down" from the cable's color. Stop if the actual board
differs from the drawings or the continuity checks below fail.
Newhaven's [2026-09-04 change notice](https://support.newhavendisplay.com/hc/en-us/articles/43251406680215-09-04-26-NHD-4-3-800480CF-ASXP)
also reports an FPC redesign; check the actual revision rather than relying
only on the shared base part number.

## Resolve the regulator before power

The NHD-TFT40 compatibility table includes this panel, but its published
schematic names **U1 MIC5225-3.3V**. Microchip rates the
[MIC5225 at 150 mA](https://www.microchip.com/en-us/product/MIC5225).
The Rev3A panel datasheet lists **160 mA typical and 240 mA maximum** VDD
current, excluding the separately powered backlight. Those published ratings
do not establish an adequate logic-supply margin for this pairing.

Identify the actual U1 marking and board revision from clear photos, and obtain
Newhaven confirmation of the regulator/current capability for **Rev3A**, not
only connector compatibility. If the fitted part is MIC5225, do not assume its
current-limit threshold makes operation above its 150 mA rating acceptable.
Any replacement supply needs to cover the specified panel load plus other
loads and thermal headroom; a regulator swap is not automatically pin-compatible.

Ask Newhaven: "Does this NHD-TFT40 revision supply the Rev3A panel's 240 mA
maximum logic load, given the MIC5225 shown in the schematic? What approved
board revision or modification should be used?" Do not independently feed H6
or parallel a second 3.3 V regulator with U1. If modification is necessary,
have a qualified technician isolate/rework the supply using a reviewed circuit.
The power arrangement below is conditional on resolving this mismatch.

## Parts and tools

- The three exact boards/panel above, plus soldered 2.54 mm header pins where
  necessary. The NHD-TFT40 holes are not screw terminals. Have a repair shop or
  experienced maker solder the headers if yours are bare; do not hold wires in
  the holes with tape.
- A **BSS138 bidirectional 3.3 V / 5 V logic-level converter**, with 10 kOhm
  pull-ups, for **PWM only**. One documented example is
  [Adafruit 757](https://www.adafruit.com/product/757). Its low/high sides are
  labeled `LV/A` and `HV/B`; equivalent modules often use `LV1/HV1`.
- One 10 kOhm resistor from DISP to GND to keep display enable low during reset.
- A regulated, isolated 5.0 V bench supply for the NHD-TFT40, with current
  limiting and adequate headroom. A 1 A output capability is a starting design
  allowance, not a measured consumption figure or a required current setting.
- A PC USB data cable for the DevKitC USB-to-UART port, a multimeter, insulated
  power distribution and short signal connections.

Keep the RGB/clock wiring to a few centimeters where practical, with short
ground returns beside the clock and signal bundle. At 29.2 MHz, long Dupont
wires or a solderless breadboard can cause noise even with the correct pinout.
The BSS138 module is for the 1 kHz backlight signal, **not RGB or pixel clock**.

## Identify the three numbering systems

- `GPIO4` means the ESP32 signal, not the fourth header position.
- `J1.4` means DevKitC header J1, position 4, per Espressif's front-view drawing.
  Check the printed signal label as well. J1 and J3 are DevKitC names only.
- `H32` below means **NHD-TFT40 external header pin 32**. `H` is this guide's
  shorthand, not a connector name printed on the PCB.
- `F24` means **panel/FPC contact 24**, carried through the breakout socket.
  You do not solder individual wires to the panel FPC.

For the breakout, use the connector-side mechanical drawing, with the
`NHD-TFT40` text upright below the socket. Its upper row is H1 to H20 from
left to right (`5VIN` to `NC`); the lower row is H21 to H40 from left to right
(`R0` to `YU`). This is **not** odd/even numbering of a two-row IDC connector.
The manufacturer's [pinout clarification](https://support.newhavendisplay.com/hc/en-us/community/posts/43856803941015-Breakout-Board-pinout-mismatch-NHD-TFT40)
confirms the distinction between these header numbers and the FPC numbers.

## Power and ground

For initial bench testing, use two separate positive supplies and a shared
ground. Do not join the DevKitC and breakout 5 V or 3.3 V rails:

```text
PC USB data/power ----> DevKitC USB-to-UART
                         GPIOs ------> RGB/control inputs on NHD-TFT40
                         3V3 --------> level shifter LV only
                         GND --------+--------------------------+
                                    |                          |
Isolated regulated 5 V supply        |                          |
  +5 V ---------------------------> H1 5VIN ----> shifter HV   |
  0 V ----------------------------> H2 GND <--------------------+
                                    |
                          onboard 3.3 V regulator + LED driver
                                    |
                              40-contact FPC socket ---> panel
```

| Connection | Destination | Meaning |
| --- | --- | --- |
| External regulated +5.0 V | H1 `5VIN` | Powers the breakout regulator and backlight converter |
| Supply 0 V | H2 `GND` | Common ground |
| DevKitC GND, e.g. J1.22 | H2/common ground | Logic reference; also connect the level shifter GND |
| DevKitC J1.1 `3V3` | Level shifter `LV` only | Low-side logic reference |
| H1's +5 V rail | Level shifter `HV` | High-side logic reference |
| H6 `3.3V` | No external connection | **Output** of the breakout regulator; internally powers F4 VDD |
| H3 `LEDK`, H4 `LEDA` | No external connection | Internally connected to F1/F2 and the current-regulated LED driver |
| DevKitC J1.21 `5V` | No connection to the display supply | Avoid joining an external 5 V supply to USB power |

FPC grounds F3, F29 and F36 are connected by the breakout. Use H2 as the
external ground reference. **Do not ground LEDK**: it is the LED current-sense
return, not an interchangeable ground terminal. Do not feed 24 V into any
header. The backlight driver generates its higher voltage internally.

Use the external breakout supply only with the panel plugged in. Newhaven
warns that powering the NHD-TFT40 **without a display connected can damage
the LED driver**. During MCU memory bring-up, leave the breakout unpowered.

This arrangement deliberately does not rely on undocumented current available
through the DevKitC USB path. Do not power the DevKitC through its 5V/3V3
headers while USB supplies it. A future single-supply vehicle installation
needs a reviewed distribution circuit; this guide does not select a USB-C PD
trigger or authorize connection to raw vehicle power.

## GPIO connections

All RGB, clock, sync, DE and DISP signals below are direct **3.3 V logic**.
Only PWM passes through the level shifter. Keep the order within each color;
RGB565 uses the panel's most significant color inputs.

| Signal | ESP32 GPIO | DevKitC header | TFT40 header H | Panel FPC F |
| --- | --- | --- | --- | --- |
| B3 | 4 | J1.4 | 32 | 24 |
| B4 | 5 | J1.5 | 33 | 25 |
| B5 | 6 | J1.6 | 34 | 26 |
| B6 | 7 | J1.7 | 35 | 27 |
| B7 | 8 | J1.12 | 36 | 28 |
| G2 | 9 | J1.15 | 9 | 15 |
| G3 | 10 | J1.16 | 10 | 16 |
| G4 | 11 | J1.17 | 11 | 17 |
| G5 | 12 | J1.18 | 12 | 18 |
| G6 | 13 | J1.19 | 13 | 19 |
| G7 | 14 | J1.20 | 14 | 20 |
| R3 | 15 | J1.8 | 24 | 8 |
| R4 | 16 | J1.9 | 25 | 9 |
| R5 | 17 | J1.10 | 26 | 10 |
| R6 | 18 | J1.11 | 27 | 11 |
| R7 | 21 | J3.18 | 28 | 12 |
| PCLK | 39 | J3.9 | 15 | 30 |
| HSYNC | 40 | J3.8 | 17 | 32 |
| VSYNC | 41 | J3.7 | 18 | 33 |
| DE | 42 | J3.6 | 19 | 34 |
| DISP | 1 | J3.4 | 16 | 31 |
| PWM | 2 | J3.5 | 5 | driver |

The PWM row means **GPIO2 -> low-side converter channel -> high-side channel
-> H5**, not a direct jumper. Add the 10 kOhm DISP pull-down between H16 and
H2. Neither connection changes the firmware's active-high polarity.

GPIO35/36/37 are reserved for this module's octal memory. GPIO19/20 remain
native USB, GPIO43/44 remain UART0, and boot-strapping pins are not used here.
GPIO39-42 cannot simultaneously serve an external JTAG probe.

## PWM level shifter

The breakout schematic shows R1 = 10 kOhm from VIN to PWM. Consequently,
an un-driven H5 can rise to **5 V**. The firmware's 3.3 V PWM output does not
make this connection safe while the ESP32 resets or is unpowered.

Wire one channel of the specified BSS138 module as follows:

| Module terminal | Connect to |
| --- | --- |
| `LV` | DevKitC J1.1 `3V3` |
| `HV` | Breakout +5 V supply rail at H1 |
| `GND` terminal(s) | Common ground/H2/DevKitC GND |
| `A1` or `LV1` | GPIO2, DevKitC J3.5 |
| `B1` or `HV1` | Breakout H5 `PWM` |
| Remaining signal channels | No external connections |

Use the matching low/high channel, not channels from different pairs. This
is a **non-inverting** connection: GPIO low requests backlight off; GPIO high
requests full backlight. It does not need an inverted firmware duty cycle.
The choice is for slow PWM only; see the module manufacturer's speed limits.

Pull-ups can turn the backlight on while the MCU is absent or starting. The
firmware holds it off after initializing PWM, then enables it after frame
handoff; it does **not** guarantee darkness throughout reset or uneven supply
startup. Verify that behavior on the bench before vehicle/night use. A future
hardware enable circuit is needed if reset-time darkness is mandatory.

Do not replace this with a lone series resistor or an inverting transistor
without redesigning the interface. An alternative is professional rework of
the breakout's pull-up network, but that is not the wiring described here.

## Unused color inputs and BIST

Tie these unused lower color bits to common GND/H2. Do not leave them floating
and do not connect them to 5 V. Grounding them gives a deterministic RGB565
mapping; full RGB565 white will not drive all 24 panel bits high.

| Signal | TFT40 header H | Panel FPC F | Connection |
| --- | --- | --- | --- |
| R0 | 21 | 5 | GND |
| R1 | 22 | 6 | GND |
| R2 | 23 | 7 | GND |
| G0 | 7 | 13 | GND |
| G1 | 8 | 14 | GND |
| B0 | 29 | 21 | GND |
| B1 | 30 | 22 | GND |
| B2 | 31 | 23 | GND |
| BIST | 20 | 35 | GND after continuity verification |

**H20 is labeled NC on the generic breakout, but F35 is BIST on this panel.**
The breakout schematic connects that header position to F35, not to ground.
With power removed and preferably before inserting the panel, verify
continuity from H20 to socket contact 35 using accessible pads and suitable
probes. Do not bridge the fine socket contacts with a large probe. If the net
cannot be verified safely, have a technician check it. Then connect H20 to
H2/GND for normal operation; do not solder a wire to the panel ribbon.

H37 `XR`, H38 `YD`, H39 `XL`, H40 `YU` reach F37-F40, which are NC on this
non-touch panel. Leave these four header positions unconnected. Do not treat
all generic `NC` or touch-labeled pins as interchangeable.

## Backlight current

Leave solder jumpers **SJ1, SJ2 and SJ3 all open**, the breakout's approximately
40 mA default. This matches the panel's typical LED current. Its maximum is
50 mA; the breakout's 60/90/180 mA settings are not suitable for this panel.
Do not bridge a jumper to gain brightness. Firmware brightness controls PWM
duty, not a higher LED current. Typical LED voltage is 24 V and is not a
separate external supply connection.

## Insert the panel FPC

Do this with **both supplies disconnected**. Never insert, remove or reverse
the FPC while either board is powered.

1. Check 40 contacts at 0.5 mm pitch and the exact panel/revision. The panel
   drawing's front view shows contact 1 at the left of its downward tail and
   contact 40 at the right. Turning the tail over reverses that apparent order.
2. Identify the socket's pin-1 end and spring-contact side. The panel datasheet
   recommends a top-contact Molex 54104-4031, but that recommendation does not
   identify the socket fitted to every breakout revision. Match your actual
   connector, not an assumed blue-side-up convention.
3. Open only the connector's locking bar, evenly at both ends, using the latch
   mechanism fitted to that connector. Do not pry the fixed housing off the PCB
   or force a sliding latch to hinge. If uncertain, stop for the photo check.
4. Insert the FPC straight to the seating line/stop, with exposed metal pads
   facing the socket's spring contacts and **FPC pin 1 aligned with socket
   pin 1**. Do not force it, twist it or sharply crease it near the glass.
5. Close the latch evenly. Check that the FPC is square and seated equally on
   both edges. Support the panel so its weight cannot pull on the ribbon.

The following net identities are the final electrical orientation check:
socket/FPC 1 -> H3 LEDK; 2 -> H4 LEDA; 3 -> ground; 4 -> H6 3.3 V;
35 -> H20; 36 -> ground. Verify using accessible PCB pads, not by puncturing
the cable. **Header H1 is 5VIN; it is not FPC pin 1.**

## First power checklist

1. Run `flash-newhaven-memory.bat COM3` on the DevKitC alone, with the
   breakout unpowered and signal wiring disconnected. Confirm 32 MiB flash,
   16 MiB PSRAM and the memory-success log. Exit monitor with `Ctrl+]`, then
   remove USB power.
2. With all power off, inspect soldering, FPC seating/orientation, SJ1-SJ3,
   grounded unused bits/BIST, DISP pull-down and the PWM converter. Check
   for solder bridges and persistent low-resistance shorts between supply
   rails and ground; capacitors may cause a brief continuity beep.
3. **Stop unless the regulator-capacity and FPC-orientation checks above are
   resolved.** For the first breakout voltage check, leave all MCU signal wires and the
   level shifter disconnected. The panel must already be inserted. A temporary
   H5-to-H2 jumper can hold PWM low for this isolated check. Apply regulated
   5.0 V to H1/H2 with current limiting; measure H6 relative to H2 near 3.3 V
   (the panel specifies 2.8-3.5 V). Stop for excessive current, heat, smell or
   an incorrect rail. Do not probe the boosted LED contacts casually.
4. Turn off the breakout supply and wait for rails to discharge. **Remove the
   temporary PWM grounding jumper before connecting GPIO2/the converter**;
   leaving it in place can short a driven signal through the converter.
   Finish the unpowered GPIO/converter wiring and connect common ground.
5. Power the breakout first so its logic rail is present before ESP32 signals
   can drive the panel; then connect DevKitC USB-to-UART power/data. Run
   `flash-newhaven-self-test.bat COM3`. This bench order avoids intentionally
   driving an unpowered panel; it is not a validation of the panel's complete
   standby/power sequencing or uneven-supply behavior.
6. Verify legible layout, colors, orientation, stable scanout and PWM dimming.
   A multimeter cannot establish 29.2 MHz clock quality: persistent noise with
   correct wiring needs shorter interconnects and oscilloscope/logic checks.
7. Once display testing passes, use `flash-newhaven-firmware.bat COM3` for BLE.
   Exit the monitor before changing modes. For bench power removal, disconnect
   MCU USB before removing the display supply, avoiding active ESP32 outputs
   into an unpowered panel. Do not unplug only the panel while live.

Do not connect the USB-powered DevKitC's 5 V header to the external display
supply. Never attach either 3.3 V rail to the other regulator's output.
Final single-supply power sequencing, startup glare, current draw, temperature
and vehicle installation remain separate hardware-validation tasks.

## Sources and verification

Checked on 2026-10-16 against:

- [NHD-TFT40 datasheet](https://newhavendisplay.com/content/specs/NHD-TFT40.pdf),
  PDF pages 3-5: physical header layout, schematic (R1/PWM, LCD1-35, R6 clock
  series resistor), external pin table, default LED current and no-load warning.
- [NHD-4.3-800480CF-ASXP Rev3A datasheet](https://newhavendisplay.com/content/specs/NHD-4.3-800480CF-ASXP.pdf),
  PDF pages 4-6 and 9: FPC/contact drawing, signal table, BIST polarity,
  supply/current limits and power-sequence diagrams.
- [Espressif DevKitC v1.1 guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html),
  J1/J3 tables, mutually exclusive power inputs and WROOM-2 memory pins.
- [Adafruit BSS138 converter 757](https://www.adafruit.com/product/757):
  topology, pull-ups and low-speed digital-interface use.
- [Microchip MIC5225](https://www.microchip.com/en-us/product/MIC5225):
  rated 150 mA output; compare against the exact panel's VDD current rating.
- Firmware mapping: `firmware/boards/devkitc_n32r16v/board.hpp`.

The header-to-firmware map is checked by repository tests. Those checks do not
verify your actual PCB revision, soldering, FPC insertion or electrical safety.
