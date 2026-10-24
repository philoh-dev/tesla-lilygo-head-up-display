# Project-created dashboard indicators

Both supported canvases use the turn arrows. `true_hud` adds compact blind-spot
icons; `road_scene` uses adjacent-vehicle sprites and an Autopilot badge. Neither
canvas includes exterior-light indicators.

These arrows are independently authored geometric vector assets for
Tesla Head-Up Display. They use a conventional filled arrowhead and short
rectangular shaft with softly rounded joins. No Tesla bitmap is read or embedded by their preparation code.

This project-created vector artwork is covered by the repository's
[MIT License](../../../LICENSE), with attribution to philoh-dev.

| Left | Right |
| --- | --- |
| ![Left arrow](turn-left.svg) | ![Right arrow](turn-right.svg) |

[`turn-left.svg`](turn-left.svg) is the canonical polygon. The right SVG is
its horizontal reflection. `project_turn_arrow()` in
[`prepare_indicator_assets.py`](../../../scripts/prepare_indicator_assets.py)
reads that polygon and rasterizes it at 16x resolution with area downsampling.
The right PNG is mirrored from the finished left PNG for exact pixel symmetry.

Both final images are 32 x 30 RGBA with the established green `#35D774`,
transparent surroundings and antialiased edges. The existing LVGL image symbols,
positioning and bright/dim blink behavior remain compatible. Both layouts use
these final-size descriptors without runtime SVG rendering or image scaling.

From the repository root:

```sh
python3 -m pip install -r scripts/requirements-assets.txt
python3 scripts/prepare_indicator_assets.py
python3 scripts/generate_indicator_images.py
python3 scripts/generate_indicator_images.py --check
```

Prepared PNGs live in `prepared/`; the shared RGB565A8 descriptors live in
`components/adapters/lvgl_rm67162/assets/indicator_images.cpp`. The original
manual-sourced turn-arrow PNGs and legacy simulator bitmap have been removed.

## Autopilot artwork

The compact 38 x 36 and native 77 x 72 road-scene badges are generated from
project geometry by `native_autopilot()`. They are project-created artwork
covered by the root license and attribution notice.

## Blind-spot artwork

`blind-spot-left.svg` and `blind-spot-right.svg` are 32 x 32 rear-view car
outlines with two radar arcs on the occupied side. The right icon is an exact
horizontal reflection of the left. White indicates occupancy; the matching
`-warning.svg` variants use red `#FF453A` for occupancy with that turn signal
active. Clear blind spots should hide the icon.

Transparent antialiased 32 x 32 PNG exports are in `prepared/`. The `true_hud`
canvas renders each icon between its turn arrow and battery/gear corner.
The white images are stored in `true_hud_indicator_images.cpp`; LVGL recolors
only the occupied side with active turn intent to warning red. Dark blink
phases retain that warning. Clear or stale occupancy hides the icon. `generate_indicator_images.py` regenerates and
checks the descriptors, and no runtime scaling is used.
