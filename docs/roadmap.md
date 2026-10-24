# Roadmap

The roadmap is organized as vertical slices. Each milestone should leave the
repository buildable, tested, and documented.

## Milestone 0: foundation

- [x] Confirm and configure the non-touch H619 board variant.
- Confirm Tesla model/year.
- [x] Select and pin ESP-IDF and display dependencies.
- [x] Scaffold the first C++ component boundary and host build.
- [x] Implement and test the hardware-independent BLE connection state machine.
- Configure formatting and static analysis.
- [x] Configure tests, firmware size enforcement, and CI.
- [x] Validate the production RM67162 transport and LVGL dashboard on H619
  hardware.

Exit criterion: a tested domain library and a reproducible firmware build that
renders a hardware test screen.

## Milestone 1: live speed

- [x] Document the connection-relevant DashKit BLE profile from upstream source.
- [x] Implement the ESP-IDF/NimBLE central adapter and boot entry point.
- [x] Validate discovery, encrypted pairing, GATT setup, and notification
  delivery against DashKit hardware.
- Validate repeated pairing recovery, keepalive, and reconnect behavior during
  long-running hardware tests.
- [x] Decode vehicle speed from synthetic DashKit packets using the provisional
  Tesla mapping.
- Validate vehicle speed and the remaining provisional signals against
  recorded and live messages from the target Tesla.
- [x] Add a modular, host-tested centered vehicle-speed dashboard feature.
- [x] Render live speed plus connection/staleness state through the display adapter.
- [x] Test reconnects, malformed frames, and freshness behavior.

Exit criterion: the display reliably shows live speed and never presents stale
speed as current.

## Milestone 2: essential driver state

- [x] Add selected P/R/N/D gear decoding and presentation.
- [x] Add modular left, right, and hazard-warning indicator visuals.
- [x] Remove unused exterior-light telemetry, decoders and indicators pending
  DashKit support.
- [x] Add compile-time selectable true-HUD and road-scene layouts.
- Add adaptive-cruise status and set speed where available.
- [x] Add freshness-aware fused speed-limit decoding and presentation.
- [x] Decode battery electrical state and power limits and drive the road-scene
  power/recuperation rail through freshness-aware presentation.
- [x] Add persistent unit, projection, brightness, and feature settings.

Exit criterion: the initial information set works against recorded routes and
the supported test vehicle.

## Milestone 3: product hardening

- Tune startup, reconnection, memory use, and rendering latency.
- Add automatic day/night brightness behavior if supported.
- [x] Add a shared ±2-pixel AMOLED orbit every four minutes.
- Validate long-term panel wear and add thermal/power handling.
- Run long-duration and fault-injection tests.
- [x] Document first-flash and recovery procedures.

Exit criterion: a release candidate survives extended bench and vehicle tests
with measured resource budgets.

## Later possibilities

- additional Tesla model/year decoding profiles;
- richer driver-assistance visualization;
- over-the-air updates;
- diagnostic capture export.

Vehicle-control commands are intentionally not on this roadmap.
