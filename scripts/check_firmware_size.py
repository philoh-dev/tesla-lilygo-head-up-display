#!/usr/bin/env python3
"""Fail when a firmware binary exceeds its explicit release budget."""

from __future__ import annotations

import argparse
from pathlib import Path


def check_firmware_size(binary: Path, maximum_bytes: int) -> int:
    if maximum_bytes <= 0:
        raise ValueError("maximum_bytes must be positive")
    if not binary.is_file():
        raise FileNotFoundError(f"firmware binary not found: {binary}")

    actual_bytes = binary.stat().st_size
    percentage = actual_bytes * 100.0 / maximum_bytes
    print(
        f"Firmware size: {actual_bytes} / {maximum_bytes} bytes "
        f"({percentage:.1f}% of budget)"
    )
    if actual_bytes > maximum_bytes:
        raise ValueError(
            f"firmware exceeds size budget by {actual_bytes - maximum_bytes} bytes"
        )
    return actual_bytes


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("--max-bytes", type=int, required=True)
    arguments = parser.parse_args()

    try:
        check_firmware_size(arguments.binary, arguments.max_bytes)
    except (FileNotFoundError, ValueError) as error:
        parser.exit(1, f"error: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
