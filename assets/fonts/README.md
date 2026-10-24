# Dashboard fonts

The `road_scene` and `true_hud` layouts use Inter 4.1 with tabular figures. Fonts are rasterized
at their final sizes and linked only into the selected layout.

![Inter typography in the LILYGO road scene](../../docs/images/dashboard-road-scene-simulator.png)

The table below describes `road_scene`. `true_hud` uses 100 px Regular speed,
18 px Medium units/status/battery/gear, and 18 px SemiBold limit
digits on the LILYGO display.

In `true_hud`, speed and unit form one horizontally centered group with a
4 px label gap. A fixed digit envelope prevents vertical movement as speed
changes. Battery and gear text fit the 18 px-high battery icon; speed-limit
digits retain at least 3 px of white padding inside the sign. The RGB preview
uses the same unscaled viewport.

| Role | Weight | LILYGO 536 x 240 | Native RGB 800 x 480 |
| --- | --- | ---: | ---: |
| Current-speed digits | Regular 400 | 64 px | 110 px |
| Speed unit | Medium 500 | 16 px | 16 px |
| General labels and charge | Medium 500 | 18 px | 18 px |
| Gear | Medium 500 | 18 px | 24 px |
| Speed-limit digits | SemiBold 600 | 24 px | 40 px |

The unit has a 3 px gap after the speed label. Its painted bottom aligns with
the speed digits rather than the font's invisible line padding. The power rail
aligns with the painted top of the speed. SemiBold assets also contain `MAX`,
but a separate cruise-speed/MAX readout is not exposed by the telemetry model.

## Sources and licenses

- [Google Fonts Inter source](https://github.com/google/fonts/tree/main/ofl/inter):
  `Inter-Variable.ttf`, with upstream revision
  `66647c0bbbe41a850d79d9c76fb13add3378940f` recorded in the font metadata.
- [Inter-OFL.txt](Inter-OFL.txt): SIL Open Font License and copyright notice.
- [OFL.txt](OFL.txt): retained license for LVGL's built-in Montserrat fallback;
  dashboard labels use Inter.
- `Inter-Regular.ttf`, `Inter-Medium.ttf`, and `Inter-SemiBold.ttf`: generated
  static instances. Regular uses optical size 32; Medium/SemiBold use 14.

## Regeneration

From the repository root, with Python and Node.js/npm installed:

```sh
python3 -m pip install -r scripts/requirements-assets.txt
python3 scripts/generate_inter_fonts.py
```

The generator pins `lv_font_conv@1.5.3`. It instantiates the three weights,
maps digit codepoints through Inter's OpenType `tnum` substitution, and checks
identical advances before generating the 4 bpp LVGL `tesla_headup_inter_*` sources.
Kerning is disabled. Canvas checks independently verify the embedded digit
advances at every generated size. There is no runtime font scaling.

To reuse an installed converter or select Node explicitly:

```sh
python3 scripts/generate_inter_fonts.py --converter path/to/lv_font_conv.js --node path/to/node
```
