# Project-created dashboard indicators

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

`autopilot.png` is the original Codex-generated minimal-layout source, as
confirmed by the contributor. Its prepared variant remains 32 x 30. The compact
38 x 36 and native 77 x 72 road-scene badges are produced by `native_autopilot()`
from project geometry. All source and prepared indicators live in this folder;
none are sourced from Tesla owner-manual bitmaps. Unused lighting artwork and
its generated descriptors have been removed. These project-created assets are
covered by the root license and attribution notice.
