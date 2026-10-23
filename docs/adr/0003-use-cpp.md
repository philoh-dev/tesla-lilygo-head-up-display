# 0003: Use C++ for application firmware

- Status: Accepted
- Date: 2026-09-23

## Context

The firmware needs low overhead and close control of embedded resources while
benefiting from strong types, deterministic ownership, and expressive domain
modeling.

## Decision

Write Tesla Head-Up Display application firmware in C++20. C libraries and ESP-IDF APIs may be
used through narrow adapter boundaries. Both host and ESP-IDF targets compile
the shared production components with the same language standard.

Code should favor value semantics, explicit ownership, bounded containers in
steady-state paths, and compile-time checks. Features are chosen by measured
cost rather than blanket assumptions.

## Consequences

- Domain types can encode units, validity, and invariants.
- Host and target can share most production code.
- ABI and lifetime handling at C boundaries must be explicit.
- Exceptions, RTTI, the standard-library subset, and allocation policy still
  require measurement and documented decisions.

## Validation

The shared domain and application components compile as C++20 with both the host
toolchain and ESP-IDF 5.4.4 without conditional business logic.
