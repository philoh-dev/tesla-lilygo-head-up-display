# Road-scene vehicle assets

These PNG sources and prepared sprites belong to the compile-time `road_scene`
layout. The ego driving view is user-supplied; the adjacent vehicles are
perspective edits of supplied artwork. Park uses separate generated artwork.

![Current LILYGO road scene](../../docs/images/dashboard-road-scene-simulator.png)

## States and orientation

| Side | Occupied, no matching signal | Occupied, matching signal |
| --- | --- | --- |
| Left | ![Left gray car](prepared/blind-spot-left-grey.png) | ![Left red car](prepared/blind-spot-left.png) |
| Right | ![Mirrored right gray car](prepared/blind-spot-right-grey.png) | ![Mirrored right red car](prepared/blind-spot-right.png) |

The right sprites are exact horizontal mirrors of the prepared left sprites.
Both color states retain the approved minimalist shape, inner-flank view,
simple gradients and sparse linework. The red perspective source targets a
shallow rear chase view and level rear-tire baseline. It is an illustrated
sprite rather than a calibrated 3D mesh.

Occupancy alone shows a gray car and that side's outer gray lane boundary.
A matching active turn signal changes the car to red and the adjacent ego-lane
divider to pure red (`#FF0000`), overriding Autopilot blue. The warning persists
through the signal's dim blink phase. No matching signal leaves the divider's
normal white/blue color. Empty blind spots hide their car and outer boundary.

## Sources and fixed presentation boxes

| Source | Prepared output | LILYGO | Native RGB |
| --- | --- | ---: | ---: |
| [Supplied rear ego view](supplied/drive-chase.png) | `road-scene-vehicle*.png` | 96 x 92 | 128 x 121 |
| [Park view](road-scene-front-three-quarter.png) | `road-scene-park*.png` | 256 x 160 | 410 x 307 |
| [Red adjacent view](blind-spot-left-red-perspective.png) | `blind-spot-left*.png` and right mirror | 128 x 112 | 160 x 140 |
| [Gray adjacent view](blind-spot-left-grey-perspective.png) | `blind-spot-*-grey*.png` | 128 x 112 | 160 x 140 |

Native files use the `-wide.png` suffix. Driving sprites are fitted to 75% of
their presentation boxes with aspect ratio preserved. Park keeps its full fit
and is bottom-aligned above the footer. The original supplied adjacent sprites
remain under `supplied/` as provenance rather than active preparation inputs.

The built-in imagegen edit prompts and reference roles are saved alongside
the perspective sources:

- [Red pose edit](blind-spot-left-red-perspective-prompt.txt).
- [Gray color edit](blind-spot-left-grey-perspective-prompt.txt).

The Park source was generated with the graphite ego as its identity reference
and the supplied red car as a camera-angle reference. Its prompt retained the
graphite crossover, panoramic roof and slim front light bar, with a transparent
background and no floor, shadow, text or logos.

## Rendering and storage

Both display sizes reserve 85% for driving and 15% for the status footer.
Only fresh Park gear selects the enlarged front three-quarter ego view and
hides the road. Driving uses the rear ego view. All four road boundaries share
one vanishing point; dashes grow with depth and keep gaps in the foreground.

Prepared images are final-size RGB565A8 assets: a color plane followed by an
8-bit alpha plane. H619 links compact descriptors; native RGB links dedicated
wide descriptors. The simulator links both sizes. Native assets and the large
compact Park image use lossless LZ4 storage, expanded once at initialization
into shared buffers and released after the last canvas shuts down. Rendering
does not resize or decompress sprites per frame. The packed native asset set
is checked against its 225,000-byte budget.

## Regeneration

From the repository root:

```sh
python3 -m pip install -r scripts/requirements-assets.txt
python3 scripts/prepare_road_scene_vehicle.py
python3 scripts/generate_indicator_images.py
python3 scripts/generate_indicator_images.py --check
```

The preparation step crops transparent source padding, preserves aspect ratio,
fits fixed boxes, and mirrors the right views. It does not regenerate the AI
source artwork. See the [screenshot catalog](../../docs/images/README.md) to
refresh the rendered dashboard images after changing assets.
