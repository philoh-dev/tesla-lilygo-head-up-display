# DashKit integration

## Purpose

DashKit is Tesla Head-Up Display's source of vehicle telemetry. It reads Tesla CAN buses and
streams data over Bluetooth Low Energy. Tesla Head-Up Display consumes this interface in a
read-only role for the first release.

Relevant upstream repositories:

- [dashkit-firmware](https://github.com/softwiredtech/dashkit-firmware)
- [dashpilot](https://github.com/softwiredtech/dashpilot)

## Boundary design

The integration is split into three responsibilities:

1. **BLE transport:** device discovery, connection lifecycle, GATT discovery,
   subscription, MTU/framing, and raw byte delivery.
2. **DashKit protocol:** message framing, validation, version compatibility,
   and conversion into raw CAN observations or upstream signal messages.
3. **Vehicle decoding:** conversion from CAN/protocol observations into Tesla Head-Up Display
   domain values with units, validity, and timestamps.

Each responsibility must be testable independently with captured fixtures.

## Compatibility policy

- Pin a known compatible DashKit firmware revision for each Tesla Head-Up Display release.
- Detect incompatible service or protocol versions where the upstream protocol
  provides enough information.
- Reject malformed frames and expose diagnostic counters.
- Never guess a vehicle value when a frame is incomplete or unsupported.
- Keep decoding tables versioned and traceable to their source.

## Confirmed BLE profile

The following details were verified against DashKit firmware revision
`c8efb1318069e6b6f60a621e30b3088a9066a884` and DashPilot revision
`6af1011ddf4c10b569e36632ee5bc1c83e2721e3` on 2026-09-23:

| Property | Value |
| --- | --- |
| Advertised local name | `DashKit` |
| CAN service | `CADA0000-CA00-B1E0-B0D6-C000AA0100A1` |
| CAN notification characteristic | `CADA0001-CA00-B1E0-B0D6-C000AA0100A1` |
| CAN filter write characteristic | `CADA0002-CA00-B1E0-B0D6-C000AA0100A1` |
| Vehicle-control write characteristic | `CADA0004-CA00-B1E0-B0D6-C000AA0100A1` |
| Firmware-version read characteristic | `CADA0005-CA00-B1E0-B0D6-C000AA0100A1` |
| Security | LE Secure Connections, “Just Works,” bonded |
| Preferred ATT MTU | 512 |
| Preferred PHY | 1M |

DashKit does not advertise the CAN service UUID, so discovery must scan by the
advertised local name and then confirm the service during GATT discovery. The
CAN characteristic is readable and notifiable, but access and notification
subscription require an encrypted connection.

When DashKit has no bonds, it permits first-time pairing. Once it has a bond,
new devices are rejected unless a paired client opens the two-minute pairing
window or the DashKit is factory-reset. Tesla Head-Up Display removes a stale local peer bond
once and retries. This recovers the case where DashKit has no bonds but Tesla Head-Up Display
retained old keys; it cannot bypass DashKit's locked pairing policy.

Firmware `0.1.2` introduced application keepalive opcode `0x45`. Compatible
clients send `[0x45, 0x00]` about every 15 seconds; after the first ping,
DashKit drops the link after 60 seconds of silence. Tesla Head-Up Display reads `CADA0005` and
enables this behavior only for semantic versions at or above `0.1.2`.

DashKit batches classic CAN frames in each notification:

```text
[frame_count]
  repeated frame_count times:
  [timestamp_us_le32][bus_u8][address_le32][length_u8][data_0..data_n]
```

The current firmware caps a batch at 16 frames and a CAN payload at eight
bytes. Packet decoding is deliberately outside the connection state machine.

## Remaining protocol validation

The firmware has completed encrypted setup and received notifications from a
physical DashKit. The following still require recorded hardware runs before the
integration is considered stable:

- observed effective MTU and notification rate on real hardware;
- whether notifications ever require application-level fragmentation;
- message timestamps and ordering guarantees;
- CAN bus identifier and Tesla model compatibility.

Answers should be recorded here or in a versioned protocol document with links
to the exact upstream revision.

## Security and safety

The initial adapter subscribes to telemetry only. It does not expose or invoke
DashKit vehicle-control functionality. Any future command path requires a
separate threat model, safety analysis, user confirmation design, and ADR.

Logs and fixtures must not contain secrets, stable device identifiers, or
location history unless explicitly required and safely handled.

## Current implementation

`DashKitConnection` implements scanning, connection, security, GATT discovery,
subscription, timeouts, bounded initial retries, and automatic reconnection
after a healthy link is lost. It depends on the `BleCentral` platform seam and
therefore runs in host tests without ESP-IDF or radio hardware.

`parse_can_packet` implements the DashKit notification framing as a bounded,
allocation-free parser. It validates the complete batch before exposing any
frames, rejects frame counts above 16 and classic-CAN payloads above eight
bytes, and returns typed errors for truncation or trailing data. Frame payloads
are transient views into the BLE notification and must be consumed or copied
during the visitor callback.

Synthetic replay fixtures exercise the integrated path from a ready BLE
connection through parsing, all current Tesla decoders, telemetry freshness,
presentation, and dashboard output. They cover active and calm multi-signal
states, truncated-batch atomicity, late delivery, and replay silence. These are
contract fixtures rather than evidence that the provisional mappings have been
validated on a physical vehicle.

The compact speed decoder recognizes `DI_speed` (address 599 / `0x257`, eight
bytes) on DashKit bus 0 and emits `VehicleSpeed`. It reads `DI_uiSpeed`, bits
24–31, unsigned little-endian, scale 1 and offset 0; `255` is unavailable.
This matches [DashPilot's TeslaDashKitMapper](https://github.com/softwiredtech/dashpilot/blob/3db1c41cbb0e3b030175078b48145448e81fd462/bridge/car/cars/tesla.h).
Source timestamps are preserved. The adapter explicitly supports a vehicle
configured for km/h. HUD imperial presentation converts this canonical metric
value; it does not select an imperial vehicle source profile.

Previously, interpreting `DI_uiSpeedUnits` (bit 32) as an instruction to convert
produced 26 at a reported 16 km/h on the metric target car. Switching to
`DI_vehicleSpeed` (bits 12–23, scale 0.08, offset -40 km/h, SNA `4095`) removed
that large error but the user subsequently reported 33 versus Tesla's 30 km/h.
The decoder now uses displayed speed directly, without unit-bit conversion or
an invented calibration offset. The reported pairs are not raw CAN captures;
the unit-bit discrepancy and the final displayed-speed match still require
target-car capture/comparison. An mph-configured Tesla requires an explicit
source-profile implementation rather than silently reusing this metric profile.
Both fields are documented in the pinned
[DashPilot vehicle DBC](https://github.com/softwiredtech/dashpilot/blob/3db1c41cbb0e3b030175078b48145448e81fd462/dashpilot-android/app/src/main/assets/vehicles/tesla/bus_1_tesla_vehicle.dbc#L3014-L3019).

Selected gear comes from `DI_gear` in `DI_systemStatus` (address 280 / `0x118`)
on DashKit bus 0. The three-bit little-endian field starts at bit 21. Values
1-4 map to Park, Reverse, Neutral, and Drive; invalid/SNA values hide the
indicator, while undefined values are rejected.

The Tesla decoder also recognizes `VCLEFT_lightStatus` (address 994, seven-byte
payload) and `VCRIGHT_lightStatus` (address 995, two-byte payload) on DashKit
bus 1. Bits 4-5 of the first byte distinguish OFF, ON, fault, and unavailable
states. It also accepts trailing bytes when a CAN adapter pads either message
to a larger classic-CAN storage frame, while rejecting truncated payloads.
Left and right observations remain independent and combine into the domain's
left, right, or hazard state.

`DAS_status` (address 921, eight-byte payload) on DashKit bus 0 supplies three
more signals in its first byte: the four-bit `DAS_autopilotState`, followed by
the two-bit left and right rear blind-spot warning levels. Warning levels 1 and
2 both mean occupied for the current HUD, while value 3 is unavailable. The
Autopilot decoder retains every documented state, treats value 15 as
unavailable, and rejects reserved values. Only active states 3-6 illuminate
the Autopilot indicator; available, aborting, aborted, fault, unavailable, and
stale states remain dark.

Autopilot freshness is 1,500 ms, or three nominal `DAS_status` cycles, matching
the speed-limit window below. The previous 500 ms deadline coincided with the
message period and could briefly hide an active icon (and reset blue road
lines) before a slightly delayed BLE update. This is a freshness tolerance,
not an activation/deactivation debounce: received disabled, available,
aborting, aborted, fault, or SNA states still clear the active visual
immediately. Malformed/reserved states do not extend validity, and silence
beyond 1,500 ms clears the icon. Other signal deadlines remain independent.

The same frame supplies `DAS_fusedSpeedLimit` in bits 8-12. The compact decoder
multiplies the five-bit raw value by five, matching DashPilot's Tesla mapper and
DBC. Raw values `0` (`UNKNOWN_SNA`) and `31` (`NONE`) hide the sign. Valid data
is presented as a white circle with a thick red border and black numerals, and
is removed after 1,500 ms without an updated limit. The
[DBC specifies a 500 ms cycle for DAS_status](https://github.com/softwiredtech/dashpilot/blob/3db1c41cbb0e3b030175078b48145448e81fd462/dashpilot-android/app/src/main/assets/vehicles/tesla/bus_1_tesla_vehicle.dbc#L9293-L9294),
so expiring the sign at exactly 500 ms caused hide/show flicker during normal
BLE delivery jitter. The three-cycle window tolerates jitter and missed
updates; explicit `UNKNOWN_SNA`/`NONE` still hides the sign immediately. Speed
and blind-spot freshness deadlines are unchanged.

`DashKitTelemetryIngestor` connects each valid notification batch to the Tesla
decoders and `TelemetryStore`. `VehicleSpeedPresenter` exposes only fresh speed
to the dashboard, and `SelectedGearPresenter` applies the same freshness gate
to selected gear. `TurnSignalPresenter` applies persistent direction plus the
current physical blink phase; a one-second hold distinguishes a normal dark
phase from cancellation. `BlindSpotPresenter` and `AutopilotPresenter` apply
fresh driver-assistance state. A newly illuminated opposite turn lamp clears
the previous dark side's retained intent, preventing a false hazard blink
during direction changes. `SpeedLimitPresenter` applies the same freshness
gate to the fused speed limit. Unsupported exterior-light decoding and storage
are omitted; turn-lamp feedback remains supported. All active presenters feed a shared
dirty-frame scheduler, which
coalesces changes and limits dashboard rendering to one frame per 40 ms by
default. The firmware entry point composes this complete path; BLE callbacks
still perform no rendering directly.

The implemented NimBLE adapter serializes port callbacks through a bounded
FreeRTOS event queue drained by the firmware event loop. It initiates security
before service discovery, requests the 1M PHY, answers DashKit's peer-initiated
ATT MTU exchange, persists bonds in NVS, and translates NimBLE errors into the
small failure vocabulary exposed by the port. NimBLE's peripheral role remains
enabled even though Tesla Head-Up Display does not advertise: it provides the ATT server needed
to return the MTU response expected by DashKit.

An optional diagnostic capture receives frames only after the complete DashKit
packet has passed atomic validation. It copies at most the eight classic-CAN
payload bytes and logs a configurable maximum number of frames per one-second
window. Suppressed frames are summarized when the next window begins. Capture
is disabled by default because logs can contain vehicle-specific information.

## Battery power and recuperation

The road-scene rail follows DashPilot Vanilla's battery-power calculation:
`power_kW = voltage_V * current_A / 1000`. Positive power is normalized against
available discharge power; negative power against available recuperation power.
Tesla Head-Up Display requires the applicable directional limit to be positive, so a zero
recuperation allowance does not hide valid discharge output.

| DashKit bus | CAN address | Signal | Little-endian field | Scaling |
| --- | --- | --- | --- | --- |
| 1 | 306 (`0x132`, `BMS_hvBusStatus`) | `BMS_packVoltage` | bits 0–15, unsigned | 0.01 V |
| 1 | 306 (`0x132`, `BMS_hvBusStatus`) | `BMS_packCurrent` | bits 16–30, signed | -0.1 A |
| 1 | 594 (`0x252`, `BMS_powerAvailable`) | `BMS_maxRegenPower` | bits 0–15, unsigned | 0.01 kW |
| 1 | 594 (`0x252`, `BMS_powerAvailable`) | `BMS_maxDischargePower` | bits 16–31, unsigned | 0.01 kW |

Source references are the
[Tesla mapper](https://github.com/softwiredtech/dashpilot/blob/3db1c41cbb0e3b030175078b48145448e81fd462/bridge/car/cars/tesla.h),
[vehicle DBC](https://github.com/softwiredtech/dashpilot/blob/3db1c41cbb0e3b030175078b48145448e81fd462/dashpilot-android/app/src/main/assets/vehicles/tesla/bus_1_tesla_vehicle.dbc),
and [Vanilla power bar](https://github.com/softwiredtech/dashpilot/blob/6af1011ddf4c10b569e36632ee5bc1c83e2721e3/dash-apps/web-vanilla/index.html#L746-L768),
inspected on 2026-10-14.

The DBC declares eight-byte messages, but all four consumed signals are in
bytes 0–3. The decoder accepts signal-complete 4–8 byte classic-CAN payloads,
ignores unrelated trailing fields, and rejects fewer than four or more than
eight bytes. This matches the signal-oriented reading in DashPilot's pinned
[CAN parser](https://github.com/progphil1337/dashpilot/blob/3db1c41cbb0e3b030175078b48145448e81fd462/bridge/car/can_parsers.h)
and [signal reader](https://github.com/progphil1337/dashpilot/blob/3db1c41cbb0e3b030175078b48145448e81fd462/bridge/dbc/dbc.cc#L133-L139),
without accepting incomplete signal bits. Rejecting every DLC other than eight
previously left the gauge empty for these otherwise decodable payloads. No
target capture is available to confirm whether that was the reported car's DLC.

Signed raw current `-16384` is SNA;
zero pack voltage also clears electrical telemetry. Voltage/current and power
limits retain independent timestamps and ordering. Measured voltage/current
expires after 500 ms; limits have a separate bounded 1,500 ms deadline, allowing
BLE delivery gaps in the 100 ms limit message without extending current's
lifetime. This is a delivery-tolerance policy, not a claim that the CAN message
itself has a 1,500 ms cycle. Unlike Vanilla's last-frame cache, neither input is
retained indefinitely. Explicit unavailable current clears the gauge immediately;
zero directional limits are never replaced with guessed defaults. Signed
utilization is calculated in integer permille and clamped to -1000…1000.
Malformed batches cannot partially update either input. No new CAN writes or
BLE commands are introduced. The installed DashKit must forward both message
IDs on bus 1; live validation on the target car remains required.
The bus-1 hardware allowlist in DashKit revision
[`91492e2`](https://github.com/softwiredtech/dashkit-firmware/blob/91492e2911c9f83611d62a3fd488066bc1ed8a1f/main/can/dbc/dbc_generated.c#L280-L282)
already includes both IDs, so that revision requires no filter change for
these inputs.

## Battery state of charge

The road scene's battery percentage uses DashPilot Vanilla's usable-energy
inputs, not the phone battery and not an invented direct SOC field:
`floor(100 * (remaining - buffer) / (full - buffer))`, clamped to 0–100.
Unlike the pinned Vanilla implementation's `Math.round`, Tesla Head-Up Display deliberately
rounds down at the user's request: 60.5% and 60.99% both display as 60%; the
value reaches 61% only at 61.0%. This presentation choice is not a claim of
verified equivalence with Tesla's own calculation or rounding.
Zero remaining energy is a valid 0%; full energy must exceed the buffer.
Both mux inputs must be received and fresh independently. Each expires after
5,000 ms of local silence; other muxes and malformed packets do not refresh it.
Explicit unavailable energy/buffer clears its input immediately. Out-of-order
local receipt times are rejected, and source timestamps are retained per mux.

Evidence is pinned to DashPilot `3db1c41cbb0e3b030175078b48145448e81fd462`:
[TeslaDashKitMapper](https://github.com/softwiredtech/dashpilot/blob/3db1c41cbb0e3b030175078b48145448e81fd462/bridge/car/cars/tesla.h),
[Vanilla calculation](https://github.com/softwiredtech/dashpilot/blob/3db1c41cbb0e3b030175078b48145448e81fd462/dash-apps/web-vanilla/index.html),
and [vehicle DBC](https://github.com/softwiredtech/dashpilot/blob/3db1c41cbb0e3b030175078b48145448e81fd462/dashpilot-android/app/src/main/assets/vehicles/tesla/bus_1_tesla_vehicle.dbc).

| Bus | ID / DLC | Mux (bits 0–1) | Signal | Unsigned little-endian bits | Scale / SNA raw |
| --- | --- | --- | --- | --- | --- |
| 1 | 850 / `0x352`, 8 bytes | 0 | `BMS_nominalFullPackEnergy` | 16–31 | 0.02 kWh / 2047 |
| 1 | same | 0 | `BMS_nominalEnergyRemaining` | 32–47 | 0.02 kWh / 2047 |
| 1 | same | 1 | `BMS_energyBuffer` | 16–31 | 0.01 kWh / 255 |

The DBC names the message `BMS_energyStatus`, with a 1,000 ms cycle; the
5-second timeout permits alternating mux delivery and BLE jitter. These are
the exact documented SNA codes, not assumed all-ones field values.
The checked DashKit
[bus-1 allowlist](https://github.com/softwiredtech/dashkit-firmware/blob/91492e2911c9f83611d62a3fd488066bc1ed8a1f/main/can/dbc/dbc_generated.c)
contains `0x352`, but not the alternative `BMS_socStatus` (`0x292`). No new
BLE command or DashKit flash is needed for that revision's energy stream.
Target-car comparison is still required: this is an energy-derived estimate,
not a claim of verified equality with Tesla's displayed percentage.

The bounded decoder feeds independent telemetry slots, `BatteryChargePresenter`,
and `BatteryChargeIndicator` through the normal scheduler. It cannot enable a
disabled feature. No new persisted preference/schema is introduced. Both
road-scene sizes draw it in the left footer when connection/data status is healthy;
otherwise status text takes precedence. Minimal deliberately omits it.

## Licensing

Both upstream repositories currently advertise open-source licenses, but every
reused file or generated artifact must be checked at the selected revision.
Inspiration and clean-room reimplementation should be distinguished from copied
source. Preserve required notices and document provenance.
