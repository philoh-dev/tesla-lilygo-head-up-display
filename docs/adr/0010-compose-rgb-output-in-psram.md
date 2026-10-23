# 0010: Compose RGB output in PSRAM

- Status: Accepted
- Date: 2026-10-15
- Logical geometry updated by [0011](0011-render-native-road-scene-with-lossless-assets.md);
  buffer ownership and final composition remain in effect.

## Context

The Newhaven panel requires continuous 800×480 RGB scanout, while the existing
dashboards are designed for 536×240. Rendering directly into a scanned buffer
would risk tearing and couple dashboard geometry to the replacement panel.

## Decision

Keep the selected dashboard at 536×240 with shared LVGL objects/assets. On the
DevKitC profile, render native RGB565 into a retained logical image, then copy
it without scaling into the inactive one of two ESP-IDF PSRAM framebuffers.
Black-fill unused pixels. Apply configurable placement and one final horizontal
mirror in this composition step. Keep the LilyGO QSPI transport separate.

Use internal bounce buffers and the ESP-IDF 5.4.4 frame-completion callback to
establish buffer ownership. Wait for two callbacks after submission returns
before recycling the old buffer, covering an ISR already in flight on the other
core. This conservatively limits new image submissions to about 30 FPS at the
default scanout rate. Enable PSRAM instruction/rodata fetching so flash/NVS
operations do not interrupt bounce refills. Coalesce application updates rather
than queueing frames or allocating memory at runtime.

## Consequences

Both layouts retain their existing logical size and telemetry semantics. The
RGB adapter uses about 1.84 MB PSRAM for pixel storage plus 32 KB internal bounce
buffers. Composition costs a full output clear and a viewport copy per presented
frame; actual throughput and optical performance require hardware measurement.
A failed/overdue handoff turns off the backlight rather than risking reuse of
an active buffer. Driver callback semantics must be rechecked when updating IDF.

## Validation

Host composition tests and real LVGL captures cover native color format,
placement, mirroring, partial updates and ownership-counter rollover. Both
layouts/modes are cross-compiled and size-checked in CI. Physical panel timing,
backlight control, sustained performance and BLE/NVS concurrency still require
on-device validation.
