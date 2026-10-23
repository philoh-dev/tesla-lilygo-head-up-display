# 0005: Use LVGL for the display adapter

- Status: Accepted
- Date: 2026-09-23

## Context

Tesla Head-Up Display needs efficient 2D rendering, fonts, layout, and controlled display
updates on the RM67162 AMOLED. LILYGO's reference projects support LVGL, but
the version and integration quality must be verified with the exact board.

## Decision

Use LVGL inside the display adapter. The application publishes
presentation-ready dashboard state through a display port and has no
dependency on LVGL objects or callbacks.

Pin LVGL 9.3 for the initial ESP-IDF integration. Do not mix LVGL
major-version APIs in production code.

## Consequences

- The project gains a mature embedded UI and rendering ecosystem.
- UI rendering remains replaceable and host-testable through the display port.
- LVGL memory, task, buffer, and thread-safety rules become adapter concerns.
- Version mismatches between examples and dependencies are a known risk.

## Validation

The selected H619 board successfully renders the production LVGL dashboard
through the RM67162 QSPI adapter with correct landscape orientation and color
order. Full and partial redraw time, memory use under BLE load, tearing,
brightness range, and long-running stability remain release measurements.
