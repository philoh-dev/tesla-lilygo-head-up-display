# 0008: Compose the dashboard from replaceable features

- Status: Accepted
- Date: 2026-09-23

## Context

Tesla Head-Up Display will gain several independently optional dashboard elements. Layout and
visual treatments must evolve without coupling vehicle-information behavior to
LVGL or a particular display implementation. Embedded rendering must also use
bounded memory and remain host-testable.

## Decision

Compose the dashboard from a fixed-capacity set of `DashboardFeature` slots.
Identify slots with stable feature identifiers and allow each slot to be
enabled, disabled, or supplied with another implementation of the same
feature.

Features render presentation primitives through the `DashboardCanvas` output
port. Concrete display adapters map those primitives to LVGL, a simulator, or
test recordings. Feature and canvas lifetimes remain caller-owned; the render
path performs no dynamic allocation.

## Consequences

- Features can be configured independently.
- Visual implementations can be exchanged without changing the coordinator.
- The application can be tested without the display library or hardware.
- The canvas vocabulary must grow carefully as richer visuals are introduced.
- Replacing a feature requires an object with a lifetime longer than its slot.

## Validation

Host tests verify centered metric and imperial speed output, enabling and
disabling the feature, and replacement with a different visual implementation.
Both compiled LVGL layouts are covered by simulator tests, and the road-scene
layout has rendered on the H619. On-device performance and long-running
stability still require measured validation.
