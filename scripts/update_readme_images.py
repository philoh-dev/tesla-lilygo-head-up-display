#!/usr/bin/env python3
"""Capture README road-scene images from a built SDL simulator."""

import argparse
import os
from pathlib import Path
import subprocess
import tempfile

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SCENES = {
    "dashboard-road-scene-simulator.png": ["--preview-left-turn-signal"],
    "dashboard-road-scene-no-signal.png": ["--preview-no-turn-signal"],
    "dashboard-road-scene-right-signal.png": ["--preview-right-turn-signal"],
    "dashboard-road-scene-recuperation.png": [
        "--preview-no-turn-signal", "--preview-regeneration"],
    "dashboard-road-scene-lilygo-park.png": ["--preview-park"],
    "newhaven-rgb-road-scene.png": ["--rgb", "--preview-left-turn-signal"],
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--simulator", type=Path, required=True,
                        help="Built road_scene simulator executable")
    args = parser.parse_args()
    simulator = args.simulator.resolve(strict=True)
    output = ROOT / "docs/images"
    environment = dict(os.environ, SDL_VIDEODRIVER="dummy")
    with tempfile.TemporaryDirectory(prefix="tesla_headup-readme-") as directory:
        for name, options in SCENES.items():
            bitmap = Path(directory) / "capture.bmp"
            subprocess.run([
                str(simulator), "--direct-view", "--preview-connected",
                "--preview-speed", "130", "--preview-speed-limit", "130",
                "--preview-both-blind-spots", *options,
                "--screenshot", str(bitmap),
            ], cwd=ROOT, env=environment, check=True)
            expected = (800, 480) if "--rgb" in options else (536, 240)
            with Image.open(bitmap) as image:
                if image.size != expected:
                    raise ValueError(f"{name}: expected {expected}, got {image.size}")
                image.convert("RGB").save(output / name, optimize=True)
            print(f"Updated docs/images/{name} ({expected[0]}x{expected[1]})")


if __name__ == "__main__":
    main()
