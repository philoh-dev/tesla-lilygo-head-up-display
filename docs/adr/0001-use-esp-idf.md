# 0001: Use ESP-IDF as the target framework

- Status: Accepted
- Date: 2026-09-23

## Context

Tesla Head-Up Display targets an ESP32-S3 and needs BLE central functionality, predictable
FreeRTOS integration, control over memory, and access to the RM67162 display.
DashKit itself uses ESP-IDF, and LILYGO publishes ESP-IDF examples for the
display family. Arduino offers convenient board libraries but introduces
another abstraction layer around the platform services Tesla Head-Up Display needs to control.

## Decision

Use ESP-IDF 5.4.4 as the initial firmware framework and build system, matching
the version pinned by the inspected DashKit firmware. Board and framework APIs
are confined to adapters.

## Consequences

- Tesla Head-Up Display can use native NimBLE, FreeRTOS, logging, storage, and diagnostics.
- The project can share platform vocabulary with DashKit without coupling to
  its implementation.
- Some Arduino-only examples or libraries may require porting or replacement.
- Developers need the ESP-IDF toolchain in addition to the host test toolchain.

## Validation

CI compiles both dashboard layouts with ESP-IDF 5.4.4. H619 bring-up confirms
that the RM67162 initializes and renders the production dashboard, and the
firmware has separately completed DashKit setup and received notifications.
Extended coexistence, reconnect, and resource-stability tests remain required.
