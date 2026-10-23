# 0002: Use DDD and hexagonal architecture

- Status: Accepted
- Date: 2026-09-23

## Context

Vehicle meaning, DashKit transport details, Tesla CAN decoding, rendering, and
ESP32 infrastructure evolve for different reasons. Directly mixing those
concerns would make behavior difficult to test and would couple the product to
specific upstream and hardware APIs.

## Decision

Model driver information as a domain and isolate external technology through
ports and adapters. Dependencies point toward the domain. Use an
anti-corruption layer wherever DashKit or Tesla protocol concepts enter Tesla Head-Up Display.

Apply the architecture pragmatically: interfaces are introduced at meaningful
boundaries, not for every class or function.

## Consequences

- Core behavior can be compiled and tested on a host.
- BLE, decoding, and display implementations can evolve independently.
- Domain concepts and freshness rules become explicit.
- More mapping code and boundary contracts are required.
- Reviews must enforce dependency direction and prevent framework types from
  leaking inward.

## Validation

The speedometer and subsequent telemetry slices run the same domain/application
code through host fakes, the SDL display transport, and compiled ESP32 hardware
adapters. The production display and BLE adapters have completed initial H619
and DashKit smoke tests; endurance, live-signal, and optical behavior remain
part of hardware acceptance.
