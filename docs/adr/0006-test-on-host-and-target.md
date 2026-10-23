# 0006: Test on host and target

- Status: Accepted
- Date: 2026-09-23

## Context

Fast feedback requires tests that do not need a car or microcontroller, while
BLE, scheduling, display, and memory behavior cannot be proven by host tests
alone.

## Decision

Use a layered test strategy:

- host unit tests for domain and application behavior;
- host contract and replay tests for protocols and adapters;
- host component tests for complete telemetry-to-dashboard flows;
- on-device integration and endurance tests for hardware-dependent behavior;
- controlled manual vehicle validation for the final system.

All production behavior must be structured so it can be exercised at the
lowest applicable level.

## Consequences

- Most regressions can be caught quickly without hardware.
- Protocol fixtures become maintained project assets.
- Hardware still needs dedicated test procedures and access in release flows.
- Build configuration must keep host and target interpretations consistent.

## Validation

Unit, protocol, replay, canvas, and firmware-build tests run in CI. The
display-only self-test provides documented on-device telemetry and performance
output, and initial H619/DashKit smoke tests have succeeded. Automated
endurance and controlled vehicle validation remain release gates.

