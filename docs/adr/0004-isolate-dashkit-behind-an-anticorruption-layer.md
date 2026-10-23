# 0004: Isolate DashKit behind an anti-corruption layer

- Status: Accepted
- Date: 2026-09-23

## Context

DashKit owns the BLE and CAN-facing protocol consumed by Tesla Head-Up Display. Its framing,
names, and decoded signals may change independently, and different Tesla
models or firmware revisions may expose different data.

## Decision

Place BLE transport, DashKit framing, and Tesla signal decoding in a driving
(input) adapter and an anti-corruption layer. Convert external messages into
typed Tesla Head-Up Display telemetry observations before invoking application behavior.

Preserve reception timestamps and provenance. Do not expose raw GATT objects,
CAN frames, message IDs, or DBC library types to the domain.

## Consequences

- Upstream protocol changes remain localized.
- Captured payloads can drive contract tests.
- Unsupported or malformed data has one validation boundary.
- Mapping and version-compatibility code must be maintained explicitly.

## Validation

Synthetic packets currently replay through the complete adapter and
presentation path on the host. Versioned vehicle captures and comparison with
live observations remain the acceptance test for each provisional mapping.
