# Testing strategy

Tesla Head-Up Display treats testability as an architectural constraint. Most behavior must run
on a development host without an ESP32 or vehicle.

## Test levels

### Unit tests

Fast host tests cover:

- value-object validation and unit conversion;
- telemetry aggregation;
- stale-data behavior;
- connection-state transitions;
- dashboard-state selection;
- malformed and boundary-value inputs.

Domain tests use no mocks of frameworks because the domain has no framework
dependencies.

### Protocol and contract tests

Adapters are tested against stable contracts and byte fixtures:

- BLE notification delivery and bounded queuing;
- DashKit frame validation;
- CAN message and signal decoding;
- endianness, sign extension, scaling, and range handling;
- compatibility with recorded frames from known firmware versions;
- display-port output for known telemetry sequences.

Fixtures must record provenance, upstream version, vehicle model/year when
known, bus, capture time, and any redaction. Raw captures must be reviewed for
identifiers or sensitive location data before committing.

### Component tests

Host fakes connect real decoders, application services, and a recording display
adapter. These tests replay representative journeys and verify the resulting
dashboard timeline.

The first complete replay suite uses the code-native fixtures in
`tests/fixtures/synthetic_dashkit_packets.hpp`. It drives raw BLE notification
bytes through `DashKitConnection`, the packet parser, every implemented Tesla
decoder, the freshness-aware telemetry store, all presenters, the render
scheduler, and a recording dashboard canvas. Its timeline covers:

- one multi-frame notification containing every currently displayed signal;
- a subsequent calm-state notification that replaces the complete view;
- atomic rejection of a truncated multi-frame notification;
- rejection of a late packet with an older local receipt time;
- removal of every displayed value after replay silence exceeds freshness.

The companion `tests/fixtures/README.md` records provenance and privacy status.
Synthetic fixtures remain distinct from future vehicle captures.

The power suite additionally covers every signed 15-bit BMS current encoding,
fixed-point power scaling and saturation, independent electrical/limit
freshness, packet-to-rail replay, and the display-self-test power phases. LVGL
tests verify the retained rail fill's direction, extent, color, zero/stale
clearing, and unchanged object count.
Power regressions also cover every accepted 4–8 byte signal-complete payload
length, rejection of incomplete fields, independent 500 ms current / 1,500 ms
limit deadlines, and malformed short-frame batches. A raw-packet → decoder →
telemetry → presenter → real LVGL RGB565 capture verifies grey consumption and
green recuperation above the empty track on both road-scene sizes, then checks
that current expiry actually restores the track pixels. These are synthetic
software regressions, not a captured-car validation.

Speed-decoder tests exhaust all 256 UI-speed encodings, including SNA, with
both unit-bit values and unrelated-bit noise for the metric vehicle profile.
Packet-pipeline regressions cover 16 versus 26 and synthetic UI 30 versus
physical 33 km/h. Both layouts verify retained arrows at full/50% opacity and
capture rendered RGB565 pixels to check that the dark phase stays green at
roughly half the bright-phase intensity. Multi-cycle turn-signal replay covers
both directions at the documented 100 ms lamp-message cadence.
Speed-limit replay tests include jitter around the 500 ms CAN cycle,
a 1.5-second expiry, and immediate clearing of explicit unavailable values.
Autopilot pipeline tests service presentation between jittered 500 ms updates
and across a missed cycle. They verify no redundant renders or disappearing
icons, exact 1.5-second expiry, immediate clearing of explicit inactive/fault/SNA
states, and that malformed/reserved data cannot prolong an active indication.

### Pixel-orbit rendering tests

Battery-charge test cases cover hand-authored mux-0/mux-1 packets, independent
input ordering/expiry, missing and explicit unavailable inputs, zero/full and
floor rounding/clamping, wrong bus/ID/DLC, every mux byte and malformed-batch atomicity.
Floor regressions cover 60.49%, 60.5%, 60.99% and 61.0%, buffered ratios,
0/100% boundaries, and exact quotient bounds over small denominators. Packet
replay verifies that fractional changes within one whole percent do not request
another frame, while crossing the integer boundary updates the rendered value.
They also exercise packet → telemetry → presenter → canvas, no redundant
presentation updates and disabled-feature preservation. Native canvas tests
cover footer warning precedence in both feature orders, missing/invalid/0–100%
charge, stable placement with Autopilot on/off, and retained object counts.

Native lossless asset tests compare expanded bytes to source-derived RGB565A8,
exercise literal/overlapping matches, malformed/truncated blocks, 5,000 bounded
fuzz inputs, shared ownership, cleanup/reinitialization and failure rollback.
Python tests independently round-trip the offline compressor and enforce a
packed-asset storage bound. These checks run alongside the normal and UBSan
host suites; firmware builds enforce the existing binary-size budget.

Both layout canvas suites advance LVGL time without waiting in real time to
verify the four-minute pixel orbit, repeated bounded cycles, tick rollover,
timer/container cleanup, and static-label bounds and alignment across shifts.
An RGB565 render capture also checks that newly exposed edges are black after
every orbit position, without retaining pixels from the previous position.

### On-device tests

Raw RGB diagnostic tests cover exact solid/checkerboard pixels, labeled panel
edges, viewport borders, invalid inputs, final mirroring, buffer bounds and
three-second sequence timing/rollover. SDL tests compare all eight patterns
against the shared generator; CLI smoke tests run every pattern plus rejection
of an invalid name. CI builds diagnostic firmware for both layout selectors.

Repository tooling tests check the Newhaven `.bat` entry-point contracts:
explicit board/target/mode/layout, distinct config/build pairs, optional quoted
port, script-relative firmware directory, error propagation and no erase/eFuse
commands. These are cross-platform source-contract checks, not execution of
Windows `cmd.exe` or hardware flashing. They run in the existing Python CI suite.

Wiring-guide tests compare the GPIO/color order with `board.hpp`, check the
separate DevKitC/header/FPC mappings, account for all 40 breakout positions,
and retain the unresolved regulator/FPC safety gates. These are documentation
consistency checks; they do not clear electrical compatibility or physical wiring.

The separate `devkitc_n32r16v` CI configuration compiles a memory-only bring-up
binary with 32 MB octal flash and octal PSRAM at 80 MHz. On hardware it must
detect 16 MiB PSRAM and allocate two RGB565 framebuffer equivalents before
reporting success. It drives no display or BLE; see the
[DevKitC guide](devkitc-n32r16v.md). Host tests verify the reserved RGB pin map,
profile isolation, memory defaults and rejection of invalid board combinations.

The RGB CI matrix also builds both layouts in normal and self-test modes, with
the same binary-size limit. Composition tests exercise every valid placement,
black margins, native RGB565 partial updates, malformed rectangles, mirroring,
relocation and two-framebuffer ownership, including counter rollover.
Full-panel road-scene tests additionally cover 800×480 partial flushes,
dimension validation, exact mirroring, twelve straight lane segments, enlarged
vehicle descriptors, grey/red turn intent, edge-safe text and reverse/park/neutral
motion. The compact viewport and LilyGO tests remain independent regressions.
Both road-scene sizes verify the 80/20 driving/footer structure, Park image
switching and bottom alignment, fresh battery display and connection-warning
precedence independent of draw order. Compact Park storage is tested for
shared ownership, bounded expansion and exact RGB565A8 round trips.
`tesla_headup_rgb_dashboard_tests` renders the real LVGL layout in native and swapped
RGB565, compares colors pixel-for-pixel and verifies direct/mirrored 800×480
output. Set `TESLA_HEADUP_RGB_PREVIEW_DIR` to an absolute directory when running it to
export deterministic PPM previews. These tests do not emulate electrical timing,
PSRAM bandwidth or ISR latency.

`tesla_headup_sdl_display_tests` runs headlessly with SDL's dummy video driver under
both layouts. It compares actual SDL RGB pixels against the firmware composer
and the legacy renderer, exercises edge/center/asymmetric origins, mirror
toggles, clamped movement and partial redraws, and verifies native-size BMP
export. CLI tests cover defaults and invalid/missing/overflowing origins;
`tesla_headup_rgb_simulator_smoke` also runs the actual executable. CI renders direct
and mirrored RGB screenshots with asymmetric placement for each layout.

The Newhaven adapter is not physically validated yet. Check color order, timing,
black margins, mirrored text/direction, PWM dimming and sustained output while
BLE/NVS are active. Self-test logs report presented frames, scanout callbacks,
measurement interval and free PSRAM. Scanout Hz is not application refresh FPS.

Hardware tests cover behavior that host tests cannot establish:

- display initialization, rotation, brightness, and pixel format;
- BLE discovery, subscription, disconnect, and reconnection;
- queue behavior under realistic notification rates;
- task scheduling and watchdog behavior;
- heap stability and long-running operation;
- startup and render latency.

Initial H619 bring-up has confirmed display initialization, landscape RGB
output, PSRAM startup, and the complete LVGL pixel path. A physical DashKit has
also completed encrypted setup and delivered notifications. These successful
smoke tests do not replace the remaining automated endurance, reconnect,
resource, brightness-range, and live-signal validation.

### Manual vehicle validation

Road tests remain necessary, but supplement rather than replace automated
tests. Validation must be performed safely, ideally with one person driving and
another observing/logging.

## Test doubles

Ports should have small deterministic fakes:

- `FakeClock` for freshness and reconnect timing;
- `ReplayTelemetrySource` for captured sessions;
- `RecordingDashboard` for presentation assertions;
- in-memory settings and diagnostics adapters.

Avoid tests that reproduce implementation details through excessive mocking.

## Quality gates

A change is complete when applicable gates pass:

1. Formatting and static analysis.
2. Host unit and contract tests.
3. Firmware compilation with warnings treated as errors for project code.
4. Relevant on-device smoke or integration tests.
5. Documentation and ADR updates for changed behavior or decisions.

Coverage is useful for finding untested code, but a percentage alone is not the
goal. Safety-critical state transitions, parsers, and boundary conditions need
explicit tests even when aggregate coverage is already high.

## Continuous integration

GitHub Actions runs the host suite for every push and pull request, and can also
be started manually. The workflow executes both a regular debug build and a
build instrumented with UndefinedBehaviorSanitizer. Project code and tests are
compiled with the strict warning policy defined by their CMake targets.

A separate matrix job builds the normal `true_hud` and `road_scene` ESP32-S3
firmware plus the road-scene display-self-test binary through Espressif's
official CI action using the pinned ESP-IDF 5.4.4 container. This compile-checks
the microcontroller entry point, NimBLE adapter, every selectable display
implementation, and the compile-time BLE bypass without connected hardware.
Each binary is independently rejected when it is larger than 1,179,648 bytes.
The H619 profile uses a 1500 KiB application partition, so this budget preserves
348 KiB for growth and linker variation.

The LVGL simulator smoke test uses the same layout matrix. Both layouts run
their canvas tests and produce a deterministic RGB565 screenshot in CI.

The workflow is defined in `.github/workflows/test.yml` and uses only read
access to repository contents.

## Remaining tooling decisions

The project already uses its lightweight host test harness, CTest, strict
compiler warnings, UndefinedBehaviorSanitizer, GitHub Actions, ESP-IDF firmware
builds, and SDL smoke tests. Remaining decisions are:

- coverage tooling;
- static analyzers;
- hardware-test harness and result format;
- supported host compiler/version policy.
