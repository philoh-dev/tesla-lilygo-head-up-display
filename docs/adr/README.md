# Architecture Decision Records

Architecture Decision Records (ADRs) capture decisions that shape Tesla Head-Up Display and
would otherwise be difficult to reconstruct later.

These decisions describe the official [Tesla Head-Up Display repository](https://github.com/philoh-dev/tesla-lilygo-head-up-display).
See the [current dashboard guide](../dashboard.md) and [screenshot catalog](../images/README.md)
for the latest visual behavior. Typography, lane-warning colors, dash spacing,
and artwork refinements are maintained there without rewriting accepted ADRs.

The supported canvases are `true_hud` (default) and `road_scene`. Their current
selection, flashing workflow and asset ownership are documented in the
[project README](../../README.md). The legacy canvas is no longer a build option.

## Status values

- **Proposed:** under evaluation; implementation may explore it.
- **Accepted:** the project follows this decision.
- **Superseded:** replaced by a newer ADR.
- **Rejected:** considered but not selected.

## Index

| ADR | Status | Decision |
| --- | --- | --- |
| [0001](0001-use-esp-idf.md) | Accepted | Use ESP-IDF as the target framework |
| [0002](0002-ddd-and-hexagonal-architecture.md) | Accepted | Use DDD and hexagonal architecture |
| [0003](0003-use-cpp.md) | Accepted | Use C++ for application firmware |
| [0004](0004-isolate-dashkit-behind-an-anticorruption-layer.md) | Accepted | Isolate DashKit behind an anti-corruption layer |
| [0005](0005-use-lvgl-for-the-display-adapter.md) | Accepted | Use LVGL for the display adapter |
| [0006](0006-test-on-host-and-target.md) | Accepted | Test on host and target |
| [0007](0007-telemetry-only-first-release.md) | Accepted | Keep the first release telemetry-only |
| [0008](0008-compose-dashboard-from-replaceable-features.md) | Accepted | Compose the dashboard from replaceable features |
| [0009](0009-persist-settings-in-nvs.md) | Accepted | Persist validated settings behind a storage port |
| [0010](0010-compose-rgb-output-in-psram.md) | Accepted | Compose RGB output in PSRAM without changing dashboard geometry |
| [0011](0011-render-native-road-scene-with-lossless-assets.md) | Accepted | Native road-scene geometry and initialization-only lossless assets |

## Creating an ADR

Copy [the template](template.md), assign the next four-digit number, and use a
short imperative title. ADRs are immutable after acceptance except for status,
minor clarification, or links. A changed decision receives a new ADR that
supersedes the old one.
