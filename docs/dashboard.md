# Dashboard

On the Newhaven/DevKitC profile, `road_scene` fills the native 800×480 output
by default. It has six markings per lane, 128×121 ego and 160×140 blind-spot
vehicle boxes, and driving identifiers moved toward the outer edges. It uses
110 px Inter Regular speed, a 96 px speed-limit sign with 40 px Inter SemiBold numerals,
and a 77×72 Autopilot box. Two transparent, full-width regions divide the output:
the upper 85% (800×408) contains the driving graphics, while the bottom 15%
(800×72) contains connection status on the left, Autopilot in the center and gear
on the right. A healthy connection replaces the left status text with fresh
battery charge. Roads are clipped to the upper region, including during animation.
The speed-limit sign sits at the top-right with a 24 px inset; blind-spot cars
are centered at y=354.
The Autopilot icon keeps its existing active/hidden behavior; absent or stale
telemetry is not presented as a confirmed inactive state.
In the left footer, a grey-outline battery with green proportional fill and a
bright-grey percentage shows fresh usable battery charge. Its outline stays
visible at a valid 0%; missing, unavailable or stale inputs hide the whole
widget. Connection/waiting/stale/failure messages take precedence over the
battery widget in that same slot. Its position does not depend on Autopilot.
Preview/self-test show synthetic 51%, not a live vehicle reading.
Live charge is rounded down to whole percentages: for example, 60.99% displays
as 60%. The underlying usable-energy calculation and freshness rules are unchanged.
The native Autopilot badge is clean geometric artwork with a flat blue disk,
a centered white wheel with curved bridge/swept lower spokes and smooth edges, prepared offline at its
final size. The compact road scene uses the same artwork prepared at 38×36.
The current speed starts beside the top of the power/recuperation rail
at y=24; the unit follows the speed. The native rail has no horizontal marker.
The road boundaries share a common vanishing point and equal lane widths
at each depth; their stroke tapers continuously toward the horizon.
Artwork is pre-sized, not scaled at runtime. LilyGO remains 536×240 with four
markings per lane. Its road scene follows the same structure: a 536×204 driving
region and 536×36 footer. Speed starts at (24,12), beside the top of the rail;
the 62 px limit sign sits top-right with a 12 px inset. The rail has no horizontal
marker. Connection/battery is left, Autopilot centered and gear right in the footer.
`true_hud` and the optional compact
RGB road scene retain an unscaled 536×240 viewport. Mirroring belongs to the RGB adapter; see the
[board guide](devkitc-n32r16v.md). The SDL simulator supports both transports:
its default LilyGO mode and the Newhaven mode selected with `--rgb`.

## Current implementation

The vehicle-speed feature presents speed in the configured unit. Telemetry
remains canonical in km/h; imperial presentation uses deterministic integer
conversion. The feature remains hidden until fresh decoded telemetry is
provided. Its final placement is controlled by the selected canvas layout.
Live speed uses `DI_uiSpeed`, the displayed-speed source also used by DashPilot.
The supported CAN source profile is a Tesla configured for km/h; the unit bit
does not trigger conversion. An mph-configured vehicle is not supported by this
source profile, even though the HUD can convert metric telemetry for presentation.
No fixed calibration offset is applied. Target-car comparison is still required.

The speed-limit sign has a 1.5-second freshness window because its CAN message
updates every 500 ms. Normal BLE jitter must not blink the sign off between
updates. Explicit unavailable/no-limit values still hide it immediately.

The Autopilot icon and blue road lines use a 1.5-second freshness window too:
their `DAS_status` frame arrives every 500 ms, so a 500 ms expiry could cause
flicker during normal delivery jitter. A received inactive, fault, or unavailable
state still clears the active presentation immediately; this window only
tolerates gaps between valid observations. Silence beyond 1.5 seconds clears it.

A system-status feature occupies the left footer in both road-scene sizes,
without moving the driving indicators.
In `true_hud`, status appears bottom-right and takes precedence over gear.
It distinguishes connecting and waiting-for-data information,
amber reconnecting or stale-data warnings, and red disconnected, pairing, or
terminal connection failures. Once the BLE link is ready, the message remains
`WAITING FOR DATA` until the first valid DashKit packet arrives. It changes to
`NO DATA` when valid telemetry has not arrived for more than 500 ms. Malformed
packets never refresh that timer.

The selected-gear feature shows `P`, `R`, `N`, or `D` in the right footer
of either layout. It is
visible only while the decoded gear observation is fresh.

The turn-signal feature presents green left and right arrows around the speed.
Hazard warning activates both arrows together. Blink timing is supplied to the
feature as an explicit illuminated/dark phase, keeping clock behavior outside
the domain and deterministic in tests. Active arrows alternate between bright
green (100% opacity) and dark green (50% opacity). This follows
[DashPilot Vanilla's blink presentation](https://github.com/softwiredtech/dashpilot/blob/main/dash-apps/web-vanilla/index.html#L470-L478).
The HUD uses 50% rather than Vanilla's 30% for the dark phase because the dim
arrow was reported invisible through the C1 glass. Optical visibility needs
on-device validation; the active dark phase is not an OFF/hidden state.
Inactive, unavailable, and stale directions are hidden. The dim phase retains
turn intent, including the red vehicle warning for a blocked lane change.

In the `road_scene` layout, the blind-spot feature presents a larger, simplified
grey visualization vehicle in the occupied lane beside and slightly behind the
ego vehicle. The vehicle changes to red while the turn signal toward its lane
is active, including the lamp's dark blink phase, to highlight a blocked lane
change. Left and right occupancy are independent, so both lanes can warn at the
same time. The feature emits a semantic side warning; its concrete vehicle asset
and lane-relative placement remain display-adapter concerns. `true_hud` uses
small radar/car icons instead of full vehicle sprites.

The dashboard core does not depend on LVGL, ESP-IDF, or the RM67162 driver. It
emits presentation-ready text elements through `DashboardCanvas`, an output
port that a display adapter implements.

## Compile-time layouts

The road-scene left rail is a power/recuperation gauge. A grey fill grows upward
from the center for positive battery power; a green (`#5CBD68`) fill grows
downward for negative power. Each half spans 0–100% of its corresponding
available BMS limit. Values are clamped, and zero, stale, missing, or unavailable
power produces no fill. The neutral track remains visible without a center marker.
The `true_hud` layout uses a 6 px-high horizontal gauge between the battery
percentage and gear. Grey power use fills left from its midpoint; green
recuperation fills right. It uses the same BMS limit scaling and freshness
rules as the road-scene rail. The track stays visible with no fill at zero or
without fresh data, and shortens to clear longer connection-status text.

`PowerIndicator` receives a typed, signed utilization through `PowerPresenter`.
Electrical state and power limits must both be fresh within 500 ms. Either
input expiring clears the gauge and requests a frame through the normal render
scheduler. The calculation uses fixed-point arithmetic without allocation.

Two LVGL layouts implement the same `DashboardCanvas` contract:

- `true_hud` provides 100 px Inter speed with a light-gray unit centered as one
  group at the top on a fixed digit baseline, top-corner turn signals and a speed-limit sign
  centered above the unit (18 px digits with at least 3 px white radial padding),
  battery icon and
  percentage at bottom-left and gear/connection status at bottom-right. Footer
  text is 18 px and vertically centered on the 18 px-high battery icon. A
  35 px clear border is maintained throughout pixel orbit. Connection status
  takes precedence over gear; absent or stale values are cleared each frame.
  A 40 px blind-spot radar/car icon sits between each turn arrow and its footer
  corner. Occupied sides are white; signaling toward an occupied side turns
  that icon red, including during the dark blink phase. Clear or stale sides
  hide independently. A horizontal power/recuperation gauge shares the footer;
  vehicle, road and Autopilot graphics are omitted;
- `road_scene` places speed beside a fixed rail and adds perspective lane
  segments plus a centered vehicle viewed from behind at an elevated angle.
  Turn signals flank the road. Both sizes put the speed limit at the top-right and gear, Autopilot and
  connection status into a separate bottom status bar.

The road geometry, ego vehicle, blind-spot vehicles, labels, and image widgets
are created once during initialization and reused. Unsupported
data such as traffic signals and navigation warnings is deliberately not
fabricated.

A fresh selected gear `P` hides all lane markings and switches the same ego
image widget to a front three-quarter parked-car asset. The camera angle is
inspired by the supplied Tesla parked visualization, with the nose pointing
lower-left and the side and roof visible.
The parked car is horizontally centered and bottom-aligned to the driving
region, directly above the 15%-height status bar. Its prepared box is 410×307
on Newhaven and 256×160 on LilyGO; the corresponding bottom edges are y=408 and y=204.
Other gears restore the rear view and lane markings; `N` keeps them stationary.
Missing, stale or disabled gear
does not retain the previous Park presentation. Both road-scene sizes use
pre-sized assets with no runtime image scaling or widget creation.

When road-scene speed is above zero, an allocation-free 16 ms LVGL timer
advances the lane markings toward the vehicle. Motion is distance-based rather
than a decorative fixed-speed loop: a 6 m marking and its 6 m gap form a 12 m
cycle, so cycle time is derived directly from vehicle speed. Perspective
interpolation moves and widens nearby markings faster than distant ones. At
zero speed—or whenever the speed feature is hidden because telemetry is
stale—the road freezes immediately. Drive moves markings toward the bottom;
Reverse moves them toward the top, while Park and Neutral stop them. Imperial
values are converted back to km/h before calculating physical motion. While
Autopilot is active, every lane segment uses the same blue as its indicator;
the normal lane color returns on the first frame after disengagement.

The layout is a build choice, not a persisted runtime setting. ESP-IDF selects
one source through **Tesla Head-Up Display RM67162 display → Dashboard layout** in `menuconfig`.
Both the firmware and simulator also accept
`-DTESLA_HEADUP_DASHBOARD_LAYOUT=road_scene`, or
`-DTESLA_HEADUP_DASHBOARD_LAYOUT=true_hud` during configuration. The unselected
canvas source and its layout-specific assets are not compiled or linked.

## Modular composition

`Dashboard` owns a fixed-capacity set of non-owning feature slots. Each slot:

- is identified by a stable `FeatureId`;
- can be enabled or disabled at runtime;
- contains a replaceable `DashboardFeature` implementation; and
- renders only through `DashboardCanvas`.

The initial `CenteredVehicleSpeed` implementation can therefore be exchanged
without changing the dashboard coordinator. A future alternative could render
a speed arc or a compact value by implementing the same feature contract. A
canvas adapter controls concrete fonts, colors, and display-library objects.

No dynamic allocation occurs in the production dashboard render path. The
caller owns each registered feature and must keep it alive for as long as the
dashboard uses it.

## Rendering contract

Every render is a complete frame:

1. `begin_frame()` lets the adapter clear or prepare its retained objects.
2. Each enabled feature emits its visual elements.
3. `end_frame()` commits or flushes the frame.

The dashboard features emit these logical primitives. The adapter selects the
layout-specific placement:

| Element | Anchor | Offset | Role |
| --- | --- | --- | --- |
| Numeric speed | Center | `(0, -24)` | Primary value |
| `km/h` / `mph` | Center | `(0, +51)` | Unit |
| Left signal | Center | `(-175, -58)` | Turn signal |
| Right signal | Center | `(+175, -58)` | Turn signal |
| Blind-spot warning (`road_scene` only) | Adjacent left/right lane | `128 x 112 px` | Grey occupancy vehicle; red with matching turn signal |
| Connection / data status | Center | `(0, -104)` | Severity text |
| Selected gear | Top right | `15 px` inset | Gear text |
| Speed limit | Center | `(+220, +45)` | White/red circular road sign |

Offsets are logical display pixels interpreted by the adapter. The LVGL
adapter maps the roles to pinned fonts, colors, and persistent objects. Direct
panel rendering is verified on the H619; final sizing and mirroring still need
validation through the installed combiner or windshield.

## Remaining integration

- Validate the provisional Tesla signal mappings against real vehicle traffic.
- Measure sustained refresh, flush, heap, and task behavior on the H619 while
  BLE traffic is active.
- Calibrate sizing, brightness, and mirror direction in the final optical
  installation.
- Tune the 500 ms freshness threshold from captured driving data if necessary.

## Desktop simulator

The desktop simulator uses the same LVGL 9.3 `LvglDashboardCanvas`, built-in
fonts, object tree, 536 x 240 layout, rotation, partial-buffer size, and
`RGB565_SWAPPED` output format as the firmware. Its SDL transport receives the
same address windows and byte-swapped RGB565 pixels that the RM67162 QSPI
adapter receives, then converts those bytes back for desktop presentation.
Launch it with `./scripts/simulate.sh`. The first build fetches the pinned LVGL
revision. That default mode remains the LilyGO transport preview.

For Newhaven, run:

```sh
TESLA_HEADUP_DASHBOARD_LAYOUT=road_scene ./scripts/simulate.sh \
  --rgb --direct-view
```

`--rgb` uses native 800×480 LVGL for `road_scene`; placement is fixed to `(0,0)`.
Add `--rgb-viewport` for the original movable 536×240 composition. `true_hud`
always uses the compact viewport, default `(132,0)`.
Both use native RGB565, one 40-line partial buffer and the firmware's shared
`RgbFrame::update` / `RgbFrame::compose` functions. No bitmap stretching occurs.
In viewport mode X accepts 0–264 and Y accepts 0–240; invalid values or placement flags without
`--rgb` fail before SDL starts. I/J/K/L move the viewport up/left/down/right
by one pixel, or ten with Shift. Values clamp at the panel edges and appear
in the window title. Coordinates are unmirrored; when mirrored, horizontal
movement appears reversed on the physical preview. M reflects the entire
800-pixel output, including asymmetric placement, without changing telemetry
side semantics. Screenshots export native output pixels (800×480 for RGB,
536×240 for LilyGO), independent of high-DPI window scaling.

This previews the final pixel composition, not LCD timing, PSRAM bandwidth,
bounce-buffer ownership, brightness or ESP32 frame rate. Desktop presentation
does not impose the firmware's conservative scanout-handoff limit.

Set `TESLA_HEADUP_DASHBOARD_LAYOUT=road_scene` in the environment when invoking the
script to compile and run the alternative layout.

A deterministic preview can be captured without interaction using
`tesla_headup_simulator --screenshot /path/to/preview.bmp`; it renders 135 km/h, a 50
speed limit, and the hazard, left blind-spot, and Autopilot indicators while
showing a reconnect attempt. Windshield-projection mirroring is enabled by
default; pass `--direct-view` for an ordinary, non-mirrored desktop preview.
The preview accepts `--preview-no-turn-signal`,
`--preview-left-turn-signal`, or `--preview-right-turn-signal`; use
`--preview-right-blind-spot` for the opposite occupied lane.
Add `--preview-dim-turn-signal` to capture the active arrow's dark-green phase.
Add `--preview-regeneration` to render -50% recuperation instead of the
default +25% power-use preview.
Add `--preview-park` for a stationary `P` preview with the parked vehicle and
no road lines, turn signal or active Autopilot.
Add `--preview-connected` for a healthy synthetic link, showing battery instead
of the default reconnecting message.
Use `--preview-speed 130 --preview-speed-limit 130` for explicit values;
`--preview-both-blind-spots` shows both adjacent vehicles and
`--preview-no-autopilot` hides the icon and restores white lane lines.

- Arrow keys change speed by 1 km/h.
- Shift plus an arrow key changes speed by 10 km/h.
- A toggles the left turn signal.
- D toggles the right turn signal.
- H toggles the hazard warning signal.
- S turns all turn signals off.
- B cycles blind-spot occupancy through clear, left, right, and both.
- P cycles synthetic power use, recuperation, and zero power. The fill clears
  when simulated telemetry becomes stale.
- C cycles simulated connection and pairing states. Entering `Ready` first
  shows `WAITING FOR DATA`, then accepts periodic simulated telemetry.
- N pauses or resumes simulated telemetry. While paused in `Ready`, the normal
  500 ms freshness policy produces `NO DATA`.
- M toggles windshield-projection mirroring.
- Space enables or disables the vehicle-speed feature.
- R resets speed to zero.
- G cycles the simulated selected gear through P, R, N, and D.
- Escape closes the simulator.

The simulator validates the LVGL composition and pixel stream, but cannot by
itself validate QSPI timing, RM67162 initialization, AMOLED brightness,
tearing, or physical viewing conditions. Separate H619 bring-up has confirmed
initialization and correct production pixels; performance, endurance, and the
final optical path remain hardware work.

Projection mirroring is applied once to the completed frame by the display
adapter. Dashboard features continue to use normal driver-relative positions
and unmodified text; the windshield reflection restores the intended
orientation. The RM67162 adapter performs the corresponding final mirror in the
panel controller.

The simulator and hardware use the same pre-sized `RGB565A8` indicator images.
The shared LVGL canvas keeps a fixed pool of persistent image widgets and
reassigns their descriptor pointers each frame, avoiding runtime scaling and
object churn. Unsupported exterior-light telemetry, presentation types and
decoders are omitted.

### Lower chase-camera road scene

The driving scene uses the supplied rear Model Y and red/grey adjacent-car
artwork. Its vanishing point sits below the upper readouts, with a wider
foreground and lower vehicle viewing angle on both display sizes. The left
power/recuperation rail and vehicle-charge footer remain available. No phone
battery indicator is displayed.

Outer lane markers stay subtle gray during Autopilot and follow the road animation,
with progressively longer dashes and preserved gaps. Adjacent cars use symmetric offsets (±130 px
compact, ±184 px native). Center-lane divergence targets 1.6–1.7 times the
opaque ego-body width at its tire-contact plane. Park hides all four boundaries.

The visible road starts at y=62 on LILYGO and y=124 on native RGB, extending
the visible road markings. The 15% footer centers the vehicle battery icon
and percentage, Autopilot badge, and gear on a shared vertical midpoint.

All four boundaries project from one vanishing point, including animated
dash endpoints. Virtual ray bottom anchors
are (61,204)/(475,204) on LILYGO and (112,408)/(688,408) on native RGB.
Tests verify shared convergence, tire clearance, and adjacent-lane centering.

At each depth a shared lateral offset d places boundaries at VP_X plus
-3d, -d, +d and +3d. All three corridors therefore have exactly the same
2d pixel width, and all four origins coincide at d=0 on the horizon.

The shared apex is above the visible tips at (268,32) on LILYGO and
(400,64) on native RGB. This opens gaps between all four tips while keeping
equal-width corridors and the existing bottom anchors.

All four boundaries preserve separate dash windows into the foreground; no dash
is forced to the driving-region bottom. Distant markings are shorter and grow
with depth, including the compact exit marking below the visible region.
Full-cycle Drive/Reverse tests verify increasing lengths and positive gaps,
preventing near-field dashes from overlapping or merging.

Blue and gray segments share the same Y endpoints, animation phase, depth-based
opacity and perspective width (1–6 px compact, 1–8 px native). The left
vehicle is inset by 1 px compact / 2 px native to balance its opaque tire
footprint against the right vehicle. Gray remains subdued through its color.

### Stable low-speed rasterization and Inter

Moving dash windows reveal fixed, full-length perspective rays rather than
rerasterizing diagonals between moving integer endpoints. Each ray is one
antialiased triangle with continuous perspective taper and a full-depth opacity
gradient. This avoids the visible shoulders produced by integer-width bands
while keeping the edges stationary as dash endpoints move slowly. The LVGL 9.3
draw-task adapter uses its pinned private task header to preserve clipping;
there are no per-frame application allocations or extra persistent widgets.
A low-speed RGB565 regression compares overlapping pixels across 100 animation
frames at both display sizes. A scanline coverage check catches abrupt thickness
steps, alongside the projection and symmetry checks.

The `road_scene` layout uses Inter Regular for speed, Medium for labels/gear, and SemiBold for
speed limits. All numeric roles use tabular figures. See `assets/fonts/README.md`
for source, license and reproducible generation details.

Road-scene speed units use 16 px Inter Medium, a 3 px gap, and painted-glyph
bottom alignment with the speed digits. The power rail reaches the painted
speed top. Outer gray boundaries appear only on sides with a visible blind-spot
vehicle; empty adjacent lanes have no outer marking.

An occupied blind spot turns the adjacent ego-lane divider pure red only while
the matching turn signal is active. Occupancy alone leaves the normal lane
color unchanged. The warning overrides Autopilot blue, persists through the dim
blink phase, and clears next frame when either occupancy or the signal ends.
