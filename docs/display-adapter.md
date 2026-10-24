# LVGL display adapters

## Newhaven RGB display adapter

`components/adapters/lvgl_rgb` drives NHD-4.3-800480CF-ASXP Rev3A through
NHD-TFT40 on the `devkitc_n32r16v` profile using ESP-IDF 5.4.4's LCD/RGB
peripheral. The default H619 retains the RM67162 adapter below. Both reuse
the compile-time-selected canvas, fonts and assets in `lvgl_rm67162`.

LVGL renders **800×480 native-endian RGB565** for the full-panel road scene,
or **536×240** for `true_hud`/compact mode, into a retained logical image
through one 40-line partial buffer. Unlike RM67162, this path must not swap
pixel bytes. The composer clears a full **800×480** framebuffer to black and
copies the unscaled dashboard at `(0,0)` for full-panel mode, or configurable
`(x,y)` for compact mode. Horizontal mirroring is
applied exactly once across the final panel width, including placement. See
the [board guide](devkitc-n32r16v.md) for build commands and configuration.

The native road scene has persistent transparent 800×384 driving and 800×96
status containers. The footer owns gear/connection labels and a dedicated
Autopilot image; rendering never reparents widgets or creates these objects.
Driving graphics are clipped above the footer. Pixel orbit and final output
mirroring apply to both regions together. The compact road scene has the same
structure with 536×204 driving and 536×36 footer containers. `true_hud` uses
its own compact speed-and-status composition.

Both road-scene sizes use a battery widget with persistent LVGL rectangles and a label in the
left footer: grey outline/terminal, dark empty area, green percentage fill and
grey percentage text. `true_hud` uses the same widget style with a taller body. No bitmap, image
generation or runtime scaling is needed for this widget.
Connection/data status text occupies the same left slot and takes precedence
unless the link is Ready with fresh telemetry; selected gear is on the right.

The interactive SDL preview accepts `--rgb` (native road scene),
`--rgb-viewport` (compact composition), `--origin-x` and `--origin-y`.
It runs the same native RGB565 LVGL canvas and shared update/composition
functions; I/J/K/L adjust placement and M toggles final-output mirroring.
`--screenshot` exports the composed 800×480 image, not a scaled window capture.
The preview does not emulate the RGB peripheral, PSRAM/ISR behavior, backlight
or hardware frame rate. The default simulator mode still previews LilyGO QSPI.

ESP-IDF owns two 768,000-byte PSRAM scanout buffers. The retained dashboard
uses 768,000 bytes in full-panel mode and the partial buffer 64,000 bytes:
2,368,000 bytes of PSRAM pixel storage. Compact mode uses 257,280 and 42,880
bytes respectively: 1,836,160 bytes of PSRAM
pixel storage, excluding LVGL objects/assets. Two internal 10-line bounce
buffers add 32,000 bytes. PSRAM instruction/rodata fetching keeps bounce
refills running during flash/NVS operations.

Native road-scene assets are losslessly packed in flash and expanded once at
canvas initialization into 632,094 additional PSRAM bytes. Together with native
pixel buffers this is 3,000,094 bytes, excluding LVGL objects and the two internal
bounce buffers. The final canvas owner releases expanded assets after deleting
its widgets. LVGL still sees ordinary pre-sized RGB565A8; there is no per-frame
decompression or image scaling. The compact road scene similarly expands only
its 256×160 Park image once into 122,880 PSRAM bytes; its remaining images stay
directly in flash. Minimal does not link image expansion. The format and safety tests are described in the
[vehicle asset README](../assets/vehicles/README.md).

Only an inactive scanout buffer is composed. A submission stays pending until
two subsequent `on_frame_buf_complete` callbacks confirm that the bounce reader
has selected the new buffer and finished reading the old PSRAM buffer. A callback
already in flight on the other core may have latched the old index before
submission; waiting for the second boundary covers that race. The
completion counter is sampled after `draw_bitmap` returns and protected with
an ISR-safe FreeRTOS critical section. LVGL can update the separate logical
image meanwhile; updates coalesce until the other framebuffer is available.
There is no steady-state heap allocation or ISR-side LVGL work. A failed
submission, malformed flush or one-second handoff timeout switches off the
backlight and stops rendering. Shutdown stops the panel before releasing
buffers/callback state. Recovery requires shutdown/reinitialization or reboot.

Rev3A timing defaults: 29.2 MHz pixel clock, horizontal pulse/back/front
48/40/40 and vertical pulse/back/front 1/31/13. The 928×525 totals give about
59.9 Hz scanout. Data changes on the falling clock edge; syncs are active low.
`CONFIG_TESLA_HEADUP_RGB_PCLK_HZ` permits 28.2–40 MHz. Scanout Hz is not a guarantee of
application FPS. GPIO1 controls DISP; GPIO2 controls NHD-TFT40 backlight PWM
through a non-inverting level shifter at 1 kHz (constant high at 100%). The
breakout's PWM pull-up is to 5 V, so GPIO2 must not connect directly to it.
After PWM initialization, firmware holds the backlight off until the first
composed frame is handed off; startup/reset darkness is not guaranteed by the
software. See the [exact wiring guide](newhaven-wiring.md).

This conservative handoff limits new image submissions to roughly half the
scanout rate (about 30 FPS at the default clock); application work can lower it.

The adapter is cross-compiled, **not hardware validated**. Review wiring and
power against the manufacturer datasheets before connecting it. Unused panel
RGB inputs and BIST must not float; GPIOs must not drive LED power connections.

### Raw Newhaven diagnostics

The separate `rgb_diagnostics` firmware mode uses the same RGB panel/PSRAM
transport and ownership handoff but bypasses LVGL and the telemetry application.
It composes native 800x480 test images directly into the available framebuffer,
using the same generator as the simulator's `--rgb-diagnostics` mode. Its code
is enabled only by the dedicated diagnostic config and is not executed in
normal/self-test builds. See [RGB diagnostics](rgb-diagnostics.md).

## LilyGO RM67162 adapter

The firmware contains an ESP-IDF-only display adapter for the LILYGO
T-Display-S3 AMOLED 1.91-inch board. It uses:

- LVGL 9.3 with a logical 536 x 240 landscape display;
- ESP-IDF's SPI master driver with LILYGO-compatible polling QSPI transfers;
- QSPI at 75 MHz; and
- two partial RGB565 buffers of 40 lines each (42,880 bytes per buffer).

The firmware explicitly enables LVGL's `RGB565_SWAPPED` software-rendering
backend. LVGL 9.3 contains that backend but does not expose its enable switch
through the ESP-IDF Kconfig. Without the explicit compile definition, LVGL
calculates valid dirty rectangles but leaves their pixel buffers unrendered.

The RM67162 memory starts in a 240 x 536 portrait layout. The adapter programs
the controller's `MADCTL` register for 536 x 240 landscape and gives LVGL those
physical landscape coordinates directly. LVGL does not apply an additional
software rotation to its partial draw buffers. The adapter also owns color
format, transfer completion, brightness-command translation, and the final
windshield-projection mirror. The dashboard continues to emit normal
driver-relative presentation elements.

The H619 transport follows LILYGO's RM67162 framing directly on the ESP32-S3
SPI2 peripheral: software-managed chip select, an explicit `RAMWR` command
before pixel data, QSPI chunks of at
most 16,384 pixels, and the repeated startup sequence recommended by the board
vendor. The hardware reset explicitly ends high after its active-low pulse.

The desktop simulator compiles this same LVGL canvas. Its substitute transport
captures the post-rotation `RGB565_SWAPPED` flush areas, reconstructs the panel
frame, and presents it through SDL. This makes layout, fonts, colors, clipping,
partial rendering, and the outgoing pixel representation common between host
and hardware.

The common symbol pool creates ten persistent `lv_image` widgets during
initialization and reuses them for every frame. Dashboard symbols select
pre-sized `RGB565A8` descriptors, preserving their source colors and smooth
alpha without runtime decoding, scaling, or widget allocation.

The adapter has separate true-HUD and road-scene canvas sources. ESP-IDF's
dashboard-layout Kconfig choice compiles exactly one of them. The road-scene
variant additionally creates persistent lane lines, a power/recuperation rail
with one retained fill object, one ego
vehicle image, and two blind-spot vehicle images during initialization; it does
not add per-frame allocation or a 3D renderer. All vehicle images are embedded
at their final presentation size; their source and regeneration flow are
documented in the [vehicle asset README](../assets/vehicles/README.md).

BLE connection startup is dispatched before the blocking display bring-up, so
adding the display does not move the connection request behind UI setup.

When `CONFIG_TESLA_HEADUP_DISPLAY_SELF_TEST` is enabled, firmware intentionally skips
BLE instead. A deterministic source drives the normal telemetry, presenter,
render-scheduler, and canvas path while LVGL display events measure completed
refresh and synchronous flush duration. This mode is compiled through
`TESLA_HEADUP_FIRMWARE_MODE=display_self_test` and is not present in normal binaries.
The self-test always mirrors the completed frame for validation through the C1
combiner; normal firmware applies the persisted projection-mirroring
preference.

## AMOLED pixel orbit

Both canvases place their retained objects in a shared `LvglPixelOrbit`
container. It starts at `(0, 0)` at boot and moves every 240,000 ms (four
minutes) through `(1, 1)`, `(0, 2)`, `(-1, 1)`, `(-2, 0)`, `(-1, -1)`,
`(0, -2)`, `(1, -1)`, and `(2, 0)`, then repeats. Each step moves no more than
one pixel per axis, and the total displacement is always within ±2 pixels.

The display-layer LVGL timer runs even when telemetry and the render scheduler
are idle, so a parked or disconnected dashboard still moves. New telemetry
frames do not reset the offset or its timer. All widgets share the offset,
preserving relative alignment and driver-relative sides. Animated road objects
travel with the same container to keep the composition together.

The panel dimensions and QSPI address space stay fixed. LVGL repaints the old
and new bounds against the black screen; decorative geometry already extending
to an edge is clipped to the physical panel. Status, gear, speed, units, and
the speed-limit sign retain sufficient margin through the complete orbit.
Mirroring still occurs once at the display boundary, after this translation.
The simulator, normal firmware, and display-self-test all use this same code.
Initialization allocates one container and one timer, reused for every shift
and released at canvas shutdown. The timer uses LVGL's wrapping tick clock.

This distributes static pixel wear; brightness, thermal behavior, and
long-duration panel wear still require hardware validation.

## H619 board profile

The default firmware profile targets the non-touch H619 T-Display-S3 AMOLED:

| Signal | GPIO |
| --- | ---: |
| QSPI clock | 47 |
| QSPI data 0 | 18 |
| QSPI data 1 | 7 |
| QSPI data 2 | 48 |
| QSPI data 3 | 5 |
| Chip select | 6 |
| Reset, active-low | 17 |
| Display power | 38 |

The profile also selects 16 MB QIO flash, octal PSRAM, and the large
single-application partition. Every pin, the QSPI clock, and buffer height can
still be changed through ESP-IDF configuration. Persisted settings select
projection mirroring and a 1–100% brightness preference at boot. Touch is not
initialized or included.

The adapter performs an active-low hardware reset matching the LILYGO H619
reference sequence and leaves `TFT_RES` high. Display power is enabled by
driving GPIO 38 high.

## Validation status and limitations

- The H619 has rendered the raw solid-color transport test and the complete
  production LVGL dashboard. QSPI framing, active-low reset, active-high power,
  536 x 240 landscape orientation, RGB order, and production pixel submission
  are therefore confirmed on the selected board.
- The self-test has exercised 100% brightness. The complete 1–100% command
  range, persistence across power cycles, tearing-effect synchronization,
  sleep, long-term pixel-orbit effectiveness, and sustained performance remain
  hardware-validation work.

The official board reference is
[LILYGO Display ESP-IDF](https://github.com/Xinyuan-LilyGO/LilyGo-Display-IDF).

## Remaining hardware validation

1. Show an asymmetric `LEFT / RIGHT / TOP` calibration frame through the final
   combiner or windshield and confirm the selected mirror axis.
2. Verify the full 1–100% brightness range, including behavior after reboot.
3. Measure full-frame and partial-update time while BLE notifications run.
4. Measure internal RAM, PSRAM, task stacks, and long-running stability.
5. Repeat cold-boot, manual-reset, and extended high-brightness tests in the
   intended enclosure and power arrangement.
