# Architecture

## Style

Tesla Head-Up Display uses domain-driven design to model vehicle telemetry and hexagonal
architecture to isolate that model from BLE, CAN protocols, the display stack,
and ESP32 infrastructure.

The architecture is intentionally pragmatic for embedded software: boundaries
must improve testability and change isolation without requiring dynamic
allocation, reflection, or a large dependency-injection framework.

## Dependency rule

Dependencies point inward:

```text
drivers/frameworks -> adapters -> application -> domain
```

The domain depends only on the C++ standard library subset selected for the
target. Application code depends on the domain and port interfaces. Adapters
implement ports and may depend on ESP-IDF or third-party libraries.

The `devkitc_n32r16v` profile selects `adapters/lvgl_rgb`: ESP-IDF LCD/RGB,
double PSRAM scanout buffers, native 800×480 road-scene rendering or an
adjustable 536×240 viewport for compact rendering.
It reuses the selected LVGL canvas/assets in `lvgl_rm67162` without compiling
the RM67162 transport. H619 keeps its existing QSPI path. Both transports apply
projection mirroring only at the final output boundary; no RGB/RTOS details
enter application or domain code.

## Bounded context

The initial bounded context is **Driver Information**. It translates incoming
vehicle observations into a safe, presentation-ready dashboard state.

Implemented domain concepts include:

- `VehicleSpeed`
- `SelectedGear`
- `TurnSignalState`
- `ExteriorLightState`
- `AutopilotState`
- `BlindSpotOccupancy`
- `SpeedLimit`
- `BatteryElectricalState`, `BatteryPowerLimits`, and signed `PowerFlow`
- `BatteryEnergy` and buffered, bounded `BatteryCharge`
- validated dashboard settings and brightness preferences.

Units, valid ranges, unknown values, and timestamps belong in domain types
rather than being represented by unrelated primitives throughout the code.

## Layers

### Domain

Owns vehicle-information meaning and invariants. It has no knowledge of CAN
message IDs, byte offsets, GATT characteristics, widgets, or RTOS tasks.

### Application

Coordinates use cases such as accepting a telemetry update, detecting stale
data, presenting connection health, applying settings, and publishing
dashboard state. Speed remains canonical in km/h and is converted only by the
selected presentation implementation. The application defines the ports
required from the outside world.

The dashboard is composed from bounded, caller-owned feature slots. Features
can be enabled, disabled, or replaced and emit presentation primitives only
through the `DashboardCanvas` output port.

`TelemetryStore` is the first application-side telemetry aggregate. It retains
vehicle speed, independent left/right turn-lamp and blind-spot observations,
the fused speed limit, battery electrical state and power limits, diagnostic
exterior-light feedback, and the detailed
Tesla Autopilot state with their
DashKit source timestamps and local monotonic receipt times. Callers receive
values only while they are fresh; never-received, stale, and explicitly
unavailable states remain distinct. Updates older than the latest local receipt
time are rejected per signal so delayed work cannot overwrite newer data.

The physical turn lamps blink between ON and OFF CAN states. The store retains
turn intent for one second after the most recent ON observation while exposing
the current lamp phase separately. This lets presentation reproduce the car's
blink timing without treating each dark phase as cancellation. Missing or
invalid telemetry still fails dark after its shorter freshness timeout.
When the opposite lamp turns on, an older dark side's retained intent is
cleared immediately so a direction change cannot briefly appear as hazards.
Two illuminated lamps still produce the hazard state.

Blind-spot levels collapse to occupied/clear only at the application boundary,
because the current presentation uses the same warning for Tesla levels 1 and
2. Autopilot retains its meaningful disabled, available, active, transitional,
and fault states. The HUD icon illuminates only for nominal, restricted,
navigation, or Full Self-Driving active states.

Autopilot uses a 1.5-second freshness window for its 500 ms CAN message cycle,
as does the speed limit. This prevents jitter-driven hide/show transitions
without delaying explicit inactive, fault, or unavailable observations.
Blind-spot freshness remains independently configured at 500 ms.

Exterior-light decoding and storage remain isolated for protocol diagnostics,
but no exterior-light feature or presenter is registered in the active HUD.
This keeps the provisional mappings available for captured-data validation
without presenting unverified lamp state to the driver.

Power utilization is derived only while both battery electrical state and
available limits are fresh. Fixed-point domain arithmetic selects the
directional limit, clamps utilization, and rejects unusable inputs. A power
presenter updates the road-scene gauge through a semantic canvas primitive;
the minimal layout omits it.
Measured voltage/current expires after 500 ms; the independent limit input has
a bounded 1,500 ms delivery-tolerance deadline. Fresh current cannot revive an
expired limit, and a retained limit cannot revive stale current.

Battery charge similarly requires independently fresh full/remaining energy
and buffer observations. Its integer, clamped percentage uses DashPilot's
usable-energy formula with floor rounding (rather than Vanilla's nearest
rounding), with a five-second deadline per multiplexed input.
The battery presenter updates an optional feature value without changing its
enabled permission. The dashboard now has nine bounded feature slots; the
native and compact road scenes render the battery, while minimal omits the visual.

### Ports

Input and output ports include:

- telemetry ingestion;
- dashboard rendering;
- connection status;
- settings storage;
- diagnostics.

Ports should describe Tesla Head-Up Display's needs, not mirror an external library API.

### Adapters

Implemented adapters include:

- DashKit BLE scanning, connection, and notifications;
- DashKit framing and CAN-signal decoding;
- LVGL-based rendering on the RM67162 display;
- ESP-IDF/NimBLE, NVS settings, bounded diagnostic logging, and RM67162 output;
- deterministic host fakes used by tests and simulation.

## Anti-corruption layer

DashKit and DashPilot terminology must not define the core domain. The DashKit
adapter converts transport frames into explicit observations and the decoder
maps those observations into Tesla Head-Up Display domain values. Unsupported and malformed
messages are rejected at this boundary.

This allows BLE framing, DBC definitions, or the upstream protocol to evolve
without forcing changes through the dashboard model.

## Runtime model

The firmware runtime flow is:

1. NimBLE's host task handles the stack while `DashKitConnection` maintains
   discovery, setup, keepalive, and recovery policy on the firmware event loop.
2. NimBLE callbacks copy notifications into a bounded event queue with a local
   monotonic reception timestamp.
3. The firmware event loop drains that queue, atomically validates DashKit
   batches, decodes supported Tesla signals, and updates `TelemetryStore`.
4. Presenters expose only fresh values and mark the shared render scheduler
   dirty when visible state changes.
5. The scheduler coalesces updates to at most one frame every 40 ms; LVGL is
   serviced from the same 10 ms firmware loop rather than from BLE callbacks.

The queue and all steady-state telemetry containers are bounded. Queue overflow
is logged and the incoming event is dropped; stack-specific operation failures
are translated into the adapter's small failure vocabulary. BLE callbacks never
render directly.

## Source layout

```text
components/
  domain/
    include/tesla_headup/domain/
  application/
    dashboard/
    diagnostics/
    hud_runtime/
    settings/
    telemetry/
  adapters/
    dashkit_ble/
    dashkit_protocol/
    dashkit_tesla/
    tesla_decoder/
    esp_nimble/
    esp_nvs_settings/
    esp_diagnostics/
    lvgl_rm67162/
    lvgl_rgb/
firmware/
  boards/h619/
  boards/devkitc_n32r16v/
  layouts/
  main/
simulator/
tests/
  unit/
  integration/
  fixtures/
```

ESP-IDF component boundaries may refine this layout, but must preserve the
dependency rule.

## Error handling

- Expected absence is modeled explicitly, not as a magic numeric value.
- Malformed external input returns a typed decoding error.
- Resource exhaustion is observable through diagnostics and uses defined
  degradation behavior.
- Production component boundaries are `noexcept` where failure can be returned
  explicitly; exceptions must not cross embedded adapter boundaries.
- Assertions protect programmer invariants and do not replace input validation.

## Performance principles

- Prefer bounded memory and explicit ownership.
- Avoid allocation in steady-state telemetry and rendering paths.
- Coalesce display updates instead of rendering once per CAN frame.
- Keep the current 40 ms render-coalescing interval configurable and replace it
  only with measurements from physical hardware.
- Preserve timestamps from receipt through presentation so freshness is
  testable.
