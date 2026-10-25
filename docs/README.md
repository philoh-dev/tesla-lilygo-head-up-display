# Tesla Head-Up Display documentation

This directory contains the product, engineering, and decision documentation
for Tesla Head-Up Display.

The official source is [tesla-lilygo-head-up-display](https://github.com/philoh-dev/tesla-lilygo-head-up-display),
with the release baseline on `main`. Start with the [project README](../README.md)
for cloning, simulation, and flashing.

![Default LILYGO true_hud preview](images/dashboard-true-hud.png)

`true_hud` is the default canvas: centered speed and unit, speed limit above
the unit, corner turn arrows, 40×40 px blind-spot warnings, and battery and
gear/connection text along the bottom. A horizontal gauge between the battery
and gear fills left in gray for power use and right in green for recuperation.
Inter tabular digits and a 35 px clear
border remain stable during pixel orbit.

On Windows, `scripts/flash.ps1 COM3` builds, size-checks, flashes and monitors
the H619 using the installed ESP-IDF environment. Use `-Layout road_scene` for
the other canvas or `-BuildOnly` to validate without uploading. Each layout/mode
has an isolated build and SDKCONFIG; see the first-flash guide for all options.

![LILYGO road-scene preview](images/dashboard-road-scene-simulator.png)

The current `road_scene` has Inter tabular typography, an 85/15 driving/footer
split, speed-proportional perspective dashes, and mirrored adjacent vehicles.
An occupied blind spot colors its ego-lane divider red only with the matching
turn signal. Otherwise the divider retains its normal white/Autopilot-blue color.
Outer gray boundaries follow occupancy independently on each side.

## Guides

| Document | Purpose |
| --- | --- |
| [Product vision](product-vision.md) | Goals, scope, users, and constraints |
| [Architecture](architecture.md) | Domains, ports, adapters, and runtime flow |
| [Dashboard](dashboard.md) | Modular features and rendering contract |
| [Screenshot catalog](images/README.md) | Both layouts, synthetic states and image regeneration |
| [Fonts](../assets/fonts/README.md) | Inter weights, tabular figures, licenses and generation |
| [Dashboard indicators](../assets/icons/project/README.md) | Project-created turn, blind-spot and Autopilot artwork |
| [Vehicles](../assets/vehicles/README.md) | Source artwork, mirrored color states and fixed presentation sizes |
| [Settings](settings.md) | Persistent preferences, ports, and adapters |
| [First flash and recovery](first-flash-and-recovery.md) | H619 build, flash, diagnostics, pairing, and reset procedures |
| [Display adapters](display-adapter.md) | Shared LVGL, RM67162 QSPI and Newhaven RGB |
| [DevKitC / Newhaven](devkitc-n32r16v.md) | N32R16V memory bring-up, RGB builds, placement and pins |
| [Newhaven wiring](newhaven-wiring.md) | Exact GPIO/header/FPC map, PWM level shifting, power and first-power checks |
| [RGB diagnostics](rgb-diagnostics.md) | Solid colors, checkerboard, labeled corners and viewport calibration |
| [BLE connection](ble-connection.md) | Connection state machine and platform seam |
| [Testing](testing.md) | Test levels, fixtures, and quality gates |
| [Hardware](hardware.md) | Target device, confirmed bring-up, and remaining hardware questions |
| [DashKit integration](dashkit-integration.md) | BLE/CAN boundary, mappings, and live-validation status |
| [Roadmap](roadmap.md) | Incremental delivery milestones |
| [ADRs](adr/README.md) | Technical decisions and their consequences |

## Documentation conventions

- **Confirmed** facts are supported by the selected hardware or upstream
  implementation.
- **Assumptions** require validation but allow work to proceed safely.
- **Open questions** must not silently become implementation decisions.
- Architecture decisions with meaningful trade-offs are recorded as ADRs.

Documentation should be updated in the same change as the behavior or decision
it describes.

The current hardware baseline is a verified non-touch H619 display transport
and an exercised encrypted DashKit notification link. The guides explicitly
separate those completed smoke tests from the remaining live-signal,
performance, endurance, and optical-installation validation.
