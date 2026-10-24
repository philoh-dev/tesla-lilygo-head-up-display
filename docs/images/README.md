# Dashboard screenshot catalog

## Installed hardware photo

[`lilygo-hud-installed-park.png`](lilygo-hud-installed-park.png) is a
user-supplied photograph of the installed HUD in Park, viewed through its
reflective combiner at night. It is separate from the synthetic simulator
captures below and is not overwritten by the screenshot regeneration helper.

## Simulator captures

These are deterministic captures of the production LVGL canvas through the SDL
simulator. They show synthetic telemetry in direct-view mode: no projection
mirroring, display bezel, simulated phone UI, or image retouching. They are not
vehicle captures or evidence of validated live CAN mappings.

## Current dashboard images

| Image | Resolution | State |
| --- | --- | --- |
| [LILYGO true HUD](dashboard-true-hud.png) | 536 x 240 | Both blind spots occupied; left signal makes its icon red, right icon white |
| [LILYGO driving](dashboard-road-scene-simulator.png) | 536 x 240 | Both blind spots occupied, left signal active: left car and divider red |
| [Signals off](dashboard-road-scene-no-signal.png) | 536 x 240 | Both cars gray, both ego-lane dividers Autopilot blue |
| [Right signal](dashboard-road-scene-right-signal.png) | 536 x 240 | Mirrored right car and right divider red; left car gray |
| [Recuperation](dashboard-road-scene-recuperation.png) | 536 x 240 | Signals off, green lower power fill |
| [LILYGO Park](dashboard-road-scene-lilygo-park.png) | 536 x 240 | Speed zero, enlarged parked ego, roads and adjacent cars hidden |
| [Native RGB driving](newhaven-rgb-road-scene.png) | 800 x 480 | Native road_scene geometry, both blind spots, left signal |

Road-scene driving captures use 130 km/h, a 130 km/h speed limit, healthy synthetic
connectivity, 51% vehicle charge, and active Autopilot. The footer is 15% of the
display. Red divider warnings require occupancy plus the matching signal;
occupancy alone retains blue. Foreground dashes keep gaps. Park selects speed
zero and disables turn signals and Autopilot.

![Current LILYGO driving image](dashboard-road-scene-simulator.png)

## Regenerate

First build the `road_scene` simulator, as described in the
[project README](../../README.md). Close the interactive instance before
rebuilding its executable on Windows. Then run from the repository root:

```sh
python3 -m pip install -r scripts/requirements-assets.txt
python3 scripts/update_readme_images.py --simulator build-simulator-windows/simulator/tesla_headup_simulator.exe
```

For Linux/macOS, pass `build-simulator/simulator/tesla_headup_simulator`. The helper
selects SDL's dummy video driver, invokes the existing deterministic preview
flags, checks native dimensions, and converts BMP captures to lossless PNG.
The simulator must already be compiled with `TESLA_HEADUP_DASHBOARD_LAYOUT=road_scene`.

To capture one state manually:

```sh
build-simulator/simulator/tesla_headup_simulator --direct-view --preview-connected --preview-speed 130 --preview-speed-limit 130 --preview-both-blind-spots --preview-right-turn-signal --screenshot right-signal.bmp
```

## Capture true_hud

Build the simulator with `TESLA_HEADUP_DASHBOARD_LAYOUT=true_hud` (the default),
then capture its own state separately from the road-scene helper:

```powershell
$env:TESLA_HEADUP_DASHBOARD_LAYOUT = "true_hud"
.\scripts\simulate.ps1 --direct-view --preview-connected --preview-speed 130 --preview-speed-limit 130 --preview-both-blind-spots --preview-left-turn-signal --screenshot .cache/true-hud.bmp
```

Convert the BMP to PNG with Pillow to refresh the checked-in image:

```sh
python3 -c "from PIL import Image; Image.open('.cache/true-hud.bmp').convert('RGB').save('docs/images/dashboard-true-hud.png', optimize=True)"
```

This canvas has a 35 px clear border and corner battery/gear labels rather than
an 85/15 road/footer split. Its blind-spot warnings use icons instead of cars.

## Other images

`dashboard-true-hud.png` documents the default compact HUD layout.
`newhaven-rgb-diagnostic-corners.png` documents the raw RGB diagnostic pattern.
They are retained independently of road_scene changes; neither is produced by
the helper above. See the [RGB diagnostic guide](../rgb-diagnostics.md) for its
capture commands.
