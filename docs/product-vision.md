# Product vision

## Vision

Tesla Head-Up Display gives a Tesla driver a small, glanceable instrument display in the
driver's natural line of sight. It complements the vehicle's center display by
showing a deliberately limited set of high-value driving information.

## Primary user

A Tesla owner who has installed DashKit and wants a dedicated, low-latency
display without depending on a phone during every drive.

## Product principles

1. **Glanceable:** the important state must be understandable without reading
   dense text or navigating menus.
2. **Fail quiet:** stale, missing, or invalid data must never be presented as
   current vehicle state.
3. **Read-only first:** the initial product observes vehicle data and does not
   transmit vehicle-control commands.
4. **Fast startup:** useful information should appear shortly after power-up.
5. **Offline:** normal operation must not require internet or cloud services.
6. **Testable:** business rules and protocol interpretation must be verifiable
   without physical hardware.

## Implemented scope

The current firmware implements:

- automatic reconnection to one configured DashKit device;
- vehicle speed;
- selected drive state/gear;
- turn indicators;
- blind-spot and Autopilot-active indication;
- battery power use and recuperation in the road-scene layout;
- speed limit, where available;
- connection and stale-data indication;
- configurable brightness, projection behavior, dashboard features, and
  metric or imperial speed presentation.

Before the first usable release, the project still needs live validation of
the provisional vehicle mappings, pairing/reconnect endurance, measured device
resource budgets, and final optical installation. Adaptive-cruise state and set
speed remain a possible addition where the target vehicle and DashKit expose a
reliable signal.

Availability depends on the Tesla model, model year, CAN bus, and signals
exposed by the installed DashKit firmware.

## Out of scope for the first release

- sending control commands to the vehicle;
- navigation and map rendering;
- cloud accounts or remote telemetry;
- support for non-Tesla vehicles;
- a general-purpose application platform;
- reproducing the complete Tesla center display.

## Safety requirements

- A value older than its defined freshness threshold is visibly marked stale
  or removed.
- A BLE disconnect cannot leave the last speed looking live.
- Rendering and logging must not block telemetry ingestion.
- No initial feature may write to the vehicle CAN buses.
- The UI should avoid animations or interactions that demand attention while
  driving.

## Success criteria for the first release

- The device starts, connects, and shows current speed reliably during a drive.
- Reconnection recovers without a reboot after a temporary radio interruption.
- Recorded routes can reproduce display behavior deterministically on a host.
- Domain and application behavior is covered by automated tests.
- Device resource use remains within measured CPU, memory, and frame-time
  budgets established during hardware validation.
