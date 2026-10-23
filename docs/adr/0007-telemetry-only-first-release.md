# 0007: Keep the first release telemetry-only

- Status: Accepted
- Date: 2026-09-23

## Context

DashKit can read and send CAN messages and supports vehicle controls. A
heads-up display needs only observations. Transmitting commands creates a much
larger safety, security, and validation surface.

## Decision

The first Tesla Head-Up Display release consumes telemetry only. It does not transmit vehicle
commands or expose automation and control features.

BLE writes that are technically necessary to establish a subscription or
protocol session are permitted only when they cannot command vehicle behavior
and are documented at the adapter boundary.

## Consequences

- Initial development focuses on reliable and safe information display.
- The command path is absent rather than merely disabled in the UI.
- Some DashKit capabilities remain intentionally unused.
- Any future control feature requires a new ADR, threat model, and safety
  analysis.

## Validation

The adapter exposes only setup, subscription, firmware-version, and documented
keepalive writes; it has no application port for vehicle commands. BLE traces
must continue to be reviewed when the DashKit protocol or keepalive behavior
changes.

