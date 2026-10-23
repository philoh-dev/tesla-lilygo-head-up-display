# 0011: Render the native road scene with initialization-only lossless assets

- Status: Accepted
- Date: 2026-10-16
- Updates: [0010](0010-compose-rgb-output-in-psram.md), logical geometry only

## Context

The Newhaven UI now uses the entire 800×480 surface, divided into transparent
800×384 driving and 800×96 status regions. A large front three-quarter Park
vehicle cannot fit the existing binary budget when every native RGB565A8 image
is embedded uncompressed. Raising budgets or quantizing colors is not required.

## Decision

Keep native road-scene geometry in the shared compile-time-selected canvas.
Minimal and compact/LilyGO road scene retain 536×240 rendering. Keep ADR 0010's
two-framebuffer ownership, bounce buffers and final-output mirroring unchanged.

Store the native image set in deterministic lossless LZ4 blocks generated
offline. Expand it once at canvas initialization into ordinary pre-sized
RGB565A8 buffers allocated explicitly in ESP PSRAM (host heap in the simulator).
The LVGL owner thread reference-counts these buffers across active canvases;
delete widgets before releasing the final owner. A bounded decoder rejects
malformed blocks and rolls back initialization on allocation/decode failure.
The compact road scene uses the same initialization-only storage path for its
enlarged 256×160 Park image (122,880 expanded PSRAM bytes); its other assets stay
directly in flash. Minimal firmware does not link this path.

## Consequences

The current native image set occupies 200,273 packed flash bytes and 632,094
expanded PSRAM bytes. Native pixel buffers plus assets occupy 3,000,094 PSRAM
bytes, excluding LVGL objects and internal bounce buffers. This keeps the
1,179,648-byte firmware budget unchanged. Startup performs allocations and
decompression, while rendering performs neither image scaling nor decompression.
Colors/alpha remain exact; physical startup time and PSRAM bandwidth still
require target-hardware validation.

## Validation

Independent offline round trips, C++ malformed-block/overlap/bounds tests,
ownership/cleanup/failure rollback, normal and UBSan host suites, byte-identical
simulator screenshots, and per-layout firmware compilation/size gates cover
the software contract. No hardware flashing or FPS guarantee is implied.
