# Synthetic DashKit packet fixtures

These fixtures are hand-authored byte streams for deterministic host replay.
They use the DashKit notification framing verified at firmware revision
`c8efb1318069e6b6f60a621e30b3088a9066a884` and the provisional Tesla mappings
documented in the [DashKit integration guide](../../docs/dashkit-integration.md).

| Fixture | Purpose |
| --- | --- |
| `active_drive` | Exercises speed, gear, turns and driver assistance in one five-frame notification. |
| `calm_drive` | Replaces the active state with 42 km/h and clears all warnings and indicators. |
| `late_old_speed` | Verifies that a later-delivered packet with an older local receipt time cannot regress telemetry. |
| `truncated_active_drive()` | Verifies atomic rejection of a multi-frame notification with a truncated final frame. |

The data is entirely synthetic. It contains no VIN, BLE address, location,
timestamp from a real journey, or other user data. These fixtures must not be
relabelled as vehicle captures; real captures require their own provenance and
privacy review.

Speed frames use `DI_uiSpeed` at bits 24–31 in the metric vehicle profile,
including unavailable `255`. Physical speed at bits 12–23 is retained for
realism but is not used as the dashboard source. The UI-unit flag is deliberately
ignored by this metric source profile. The 30/33 pipeline counterexample is
synthetic and must not be described as a captured vehicle frame.

The separate `tests/unit/power_pipeline_test.cpp` builds synthetic bus-1 BMS
packets for power use and recuperation, including complete parser-to-presenter
replay, malformed-batch rejection, and freshness expiry.

Road-scene color and geometry checks live in
[`lvgl_dashboard_canvas_test.cpp`](../integration/lvgl_dashboard_canvas_test.cpp).
They cover both display sizes, left/right occupancy and signals, Autopilot
priority, dim blink phases, and non-overlapping perspective dashes through
Drive/Reverse animation cycles. The [README screenshots](../../docs/images/README.md)
use the simulator's deterministic preview inputs; they are not packet captures.

The default `true_hud` canvas has separate checks in
[`lvgl_true_hud_dashboard_canvas_test.cpp`](../integration/lvgl_true_hud_dashboard_canvas_test.cpp),
including centered speed/unit placement, stable vertical alignment, sign
padding, footer sizing, and independent blind-spot warnings through blink phases.
Exterior-light telemetry is not part of these fixtures or either canvas;
supported turn-signal decoding remains in the shared pipeline.

Run tooling checks from the repository root with:

```sh
python3 -m unittest discover -s scripts/tests -p 'test_*.py'
```

Run C++ replay and integration checks through CTest as described in the
[testing guide](../../docs/testing.md). CMake must be available on PATH for
the tooling suite's board-configuration checks.
