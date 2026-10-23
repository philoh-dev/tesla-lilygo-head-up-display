# Printable display cradle prototype

Generate two slicer-ready STLs with Python 3.9+ and no additional packages:

```sh
python3 scripts/generate_display_cradle.py
```

On Windows use `python` or `py` instead of `python3`. Default outputs:

- `build-hardware/display-cradle.stl`: base tray, open at the USB end.
- `build-hardware/display-cradle-frame.stl`: separate removable top frame.

STL coordinates are millimetres (select mm if the slicer asks). The output
folder is intentionally ignored by Git. Regenerate both files to replace the
earlier one-piece design.

## Measured envelope and assumptions

The design uses the user's maximum outside dimensions, including the acrylic:
**55 × 27 × 9.5 mm**. The reported 54 mm width is superseded by the 55 mm
maximum. Confirm that these measurements include all protrusions before
printing. The reported asymmetric acrylic borders (2 mm top/bottom,
1.8 mm non-USB, 7 mm USB side) are not treated as active-screen dimensions.

This is a display cradle, not the full mirror/combiner housing. The user
clarified that **1.8 mm non-USB and 7 mm USB are total roof widths**, including
wall plus overlap, measured from the outer edges. No extra wall thickness is
added to either dimension. The non-USB wall is now **1 mm**: a 2 mm wall could
not fit beneath the specified 1.8 mm roof.

The frame is separate because permanent lips at both ends would block the
previous slide-in assembly. Load the module from above or through the USB end,
then place the frame on top. **There are no snap fits, locating pins, or screw
attachments.** Align and secure the parts externally with removable tape for
bench testing, away from the screen/connectors. Positive vehicle retention
still needs a mounting design; do not use the unsecured frame in a car.

Coordinate convention:

- X: module length; USB and insertion are at **+X**.
- Y: module height across the screen.
- Z: thickness; screen faces **+Z**, flat base faces the print bed.

Defaults: 0.5 mm clearance on each side, 2 mm side walls/base, a 1 mm non-USB
wall, and 1.2 mm side rails projecting inward from each cavity side. The
top/bottom rail convention is unchanged (3.2 mm total width); only the end
roof parameters use total widths. Cavity: **56 × 28 × 10.5 mm**. Tray:
**57 × 32 × 12.5 mm**. Assembled outside: **57 × 32 × 13.7 mm**.

With a centred module, side rails overlap approximately 0.7 mm of each border.
The 1.8 mm non-USB roof projects 0.8 mm into the cavity and overlaps roughly
0.3 mm of the acrylic after clearance; verify actual retention. The 7 mm USB
roof is at X=50…57 mm; the non-USB roof is at X=0…1.8 mm. The entire frame is
1.2 mm thick. All lips must bear on acrylic only, not the panel or illuminated
image; verify this against the actual assembly.
There are no unmeasured screw holes, connector cutouts, or assumed button
positions. Confirm RESET/BOOT access and USB plug clearance in the slicer/mock-up.

## Tune the fit

```sh
python3 scripts/generate_display_cradle.py --clearance 0.3 --output build-hardware/tighter.stl
python3 scripts/generate_display_cradle.py --end-lip 1.8 --usb-lip 7 --force
```

`--clearance` is **per side** on all three dimensions. `--end-lip` and
`--usb-lip` are total roof widths. `--end-wall` changes the non-USB wall
independently of the side-wall `--wall`. `--lip` remains the inward side-rail
projection only. Other parameters: `--length`, `--height`, `--depth`, `--base`,
`--lip-thickness`. All dimensions are mm. Invalid roof/wall combinations are
rejected. Existing outputs require `--force`; both destinations are checked
before writing either file.

For a low-material fit coupon use `--length 12` and a different output filename:
it tests the rail cross-section, not the full-length fit or connector access.
Print the tray base-down and the frame flat as separate pieces. Both STLs
start at Z=0. For an assembled preview, translate the frame up **12.5 mm**,
without rotating it; the wider roof belongs at +X (USB). Inspect both parts
in the slicer. Do not force the
module into a tight fit or let the base press against fragile components.
Allow for any protective pads in the dimensions. Do not install powered
electronics while printing or finishing the part.

This prototype has not been printed or thermally/vibration tested. An STL
passing manifold checks is not proof of dimensional fit, heat resistance,
electrical clearance, or safe automotive retention. Test on the bench first.

## Validation

```sh
python3 -m unittest scripts.tests.test_generate_display_cradle -v
```

Tests check nominal dimensions, closed mesh edges, opposing face winding,
positive/expected volumes for both parts, exact 1.8/7 mm total roof extents,
frame placement, open USB access, invalid parameters, STL generation,
and overwrite protection for both outputs. The script-tool tests also run in the existing CI
Python test discovery step.
