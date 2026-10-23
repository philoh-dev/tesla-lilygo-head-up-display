# 0009: Persist validated settings behind a storage port

- Status: Accepted
- Date: 2026-09-25

## Context

Unit presentation, HUD mirroring, AMOLED brightness, and optional dashboard
features must survive power cycles. The domain and application layers must
remain independent of ESP-IDF, while corrupt or outdated persistent data must
not produce unsafe or undefined presentation behavior.

## Decision

Represent settings as validated domain values. Access persistence through
reader and writer ports, with an in-memory adapter for host tests and an
ESP-NVS adapter for firmware.

Store a versioned set of scalar NVS keys rather than a raw C++ structure. The
adapter validates the complete record before publishing it and rejects unknown
versions, partial data, invalid enum values, invalid brightness, and unknown
feature bits. Missing or invalid data falls back to current defaults. Storage
errors also fall back for the current boot but are not overwritten
automatically.

Connection and stale-data indication remains permanently enabled because it is
safety feedback, not a cosmetic dashboard preference.

## Consequences

- Domain and application tests do not require NVS or ESP-IDF.
- Persisted data is stable across compiler padding and internal class changes.
- Schema changes require a new record version and an explicit migration or
  defaulting policy.
- Settings can be applied at boot now even though no on-device settings UI
  exists yet.
- RM67162 brightness behavior remains hardware-dependent; 100% output is
  exercised, while the complete range still needs measurement.

## Validation

Host tests cover defaults, value bounds, storage errors, complete round trips,
record corruption, imperial speed presentation, feature mapping, and freshness
gating. CI compiles the NVS adapter and display command against ESP-IDF 5.4.4.
Physical hardware testing has confirmed usable 100% brightness. The full
brightness response and NVS persistence across repeated power cycles remain to
be measured.
