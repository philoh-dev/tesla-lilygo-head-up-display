#!/usr/bin/env python3
"""Generate separate STL files for a display cradle and removable top frame.

Python standard library only. Dimensions are millimetres; +X is the USB end,
+Z is the visible screen side. This is a mechanical prototype, not an optical
housing or a validated automotive mounting system.
"""

import argparse
from dataclasses import dataclass, fields
import math
from pathlib import Path


Point = tuple[float, float, float]
Triangle = tuple[Point, Point, Point]
Box = tuple[float, float, float, float, float, float]


@dataclass(frozen=True)
class Dimensions:
    length: float = 55.0
    height: float = 27.0
    depth: float = 9.5
    clearance: float = 0.5  # Per side, not total clearance.
    wall: float = 2.0
    end_wall: float = 1.0  # Thin non-USB wall beneath the 1.8 mm total roof.
    base: float = 2.0
    lip: float = 1.2  # Measured inward from the cavity edge.
    end_lip: float = 1.8  # TOTAL non-USB roof width, including end_wall.
    usb_lip: float = 7.0  # TOTAL USB roof width, from the outside USB edge.
    lip_thickness: float = 1.2

    def validate(self) -> None:
        for field in fields(self):
            value = getattr(self, field.name)
            if not math.isfinite(value) or value < 0:
                raise ValueError(f"{field.name} must be finite and non-negative")
            if field.name != "clearance" and value == 0:
                raise ValueError(f"{field.name} must be positive")
        if self.lip <= self.clearance:
            raise ValueError("lip must exceed clearance to retain the module")
        if 2 * self.lip >= self.height + 2 * self.clearance:
            raise ValueError("lips must leave an open screen aperture")
        if self.end_lip <= self.end_wall + self.clearance:
            raise ValueError("total end_lip must exceed end_wall + clearance")
        if self.usb_lip <= self.clearance:
            raise ValueError("usb_lip must exceed clearance")
        if self.end_lip + self.usb_lip >= self.outer[0]:
            raise ValueError("end roofs must leave an open screen aperture")

    @property
    def cavity(self) -> Point:
        return (self.length + 2 * self.clearance,
                self.height + 2 * self.clearance,
                self.depth + 2 * self.clearance)

    @property
    def outer(self) -> Point:
        x, y, z = self.cavity
        return (x + self.end_wall, y + 2 * self.wall,
                self.base + z + self.lip_thickness)


def cradle_boxes(d: Dimensions) -> list[Box]:
    d.validate()
    x, y, _ = d.outer
    shoulder = d.base + d.cavity[2]
    boxes = [
        (0, x, 0, y, 0, d.base),  # Flat printing surface.
        (0, d.end_wall, 0, y, d.base, shoulder),  # Non-USB end stop.
        (0, x, 0, d.wall, d.base, shoulder),
        (0, x, y - d.wall, y, d.base, shoulder),
    ]
    return boxes


def frame_boxes(d: Dimensions) -> list[Box]:
    """Separate frame, flat on Z=0 for printing; fit above the cradle walls."""
    d.validate()
    x, y, _ = d.outer
    t = d.lip_thickness
    return [
        (0, x, 0, d.wall + d.lip, 0, t),
        (0, x, y - d.wall - d.lip, y, 0, t),
        (0, d.end_lip, 0, y, 0, t),  # No extra wall thickness added!
        (x - d.usb_lip, x, 0, y, 0, t),
    ]


def union_mesh(boxes: list[Box], cuts: list[Box] = ()) -> list[Triangle]:
    """Surface of a box union minus rectangular cuts, on a shared grid."""
    axes = [sorted({round(b[i], 9) for b in list(boxes) + list(cuts)
                    for i in (2*a, 2*a+1)})
            for a in range(3)]
    occupied = set()
    for i in range(len(axes[0]) - 1):
        for j in range(len(axes[1]) - 1):
            for k in range(len(axes[2]) - 1):
                p = [(axes[a][n] + axes[a][n + 1]) / 2
                     for a, n in enumerate((i, j, k))]
                inside = lambda b: all(b[2*a] <= p[a] <= b[2*a+1] for a in range(3))
                if any(inside(b) for b in boxes) and not any(inside(b) for b in cuts):
                    occupied.add((i, j, k))
    triangles = []
    for i, j, k in sorted(occupied):
        x0, x1 = axes[0][i:i+2]
        y0, y1 = axes[1][j:j+2]
        z0, z1 = axes[2][k:k+2]
        faces = [
            ((i-1, j, k), ((x0,y0,z0), (x0,y0,z1), (x0,y1,z1), (x0,y1,z0))),
            ((i+1, j, k), ((x1,y0,z0), (x1,y1,z0), (x1,y1,z1), (x1,y0,z1))),
            ((i, j-1, k), ((x0,y0,z0), (x1,y0,z0), (x1,y0,z1), (x0,y0,z1))),
            ((i, j+1, k), ((x0,y1,z0), (x0,y1,z1), (x1,y1,z1), (x1,y1,z0))),
            ((i, j, k-1), ((x0,y0,z0), (x0,y1,z0), (x1,y1,z0), (x1,y0,z0))),
            ((i, j, k+1), ((x0,y0,z1), (x1,y0,z1), (x1,y1,z1), (x0,y1,z1))),
        ]
        for neighbor, quad in faces:
            if neighbor not in occupied:
                triangles.extend([(quad[0], quad[1], quad[2]),
                                  (quad[0], quad[2], quad[3])])
    return triangles


def stl_text(triangles: list[Triangle], name: str = "tesla_headup_display_cradle") -> str:
    lines = [f"solid {name}"]
    for a, b, c in triangles:
        u = tuple(b[i] - a[i] for i in range(3))
        v = tuple(c[i] - a[i] for i in range(3))
        n = (u[1]*v[2] - u[2]*v[1], u[2]*v[0] - u[0]*v[2],
             u[0]*v[1] - u[1]*v[0])
        magnitude = math.sqrt(sum(t*t for t in n))
        lines.append("  facet normal " + " ".join(f"{t/magnitude:.9g}" for t in n))
        lines.append("    outer loop")
        for p in (a, b, c):
            lines.append("      vertex " + " ".join(f"{t:.9g}" for t in p))
        lines.extend(["    endloop", "  endfacet"])
    lines.append(f"endsolid {name}")
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    for field in fields(Dimensions):
        parser.add_argument("--" + field.name.replace("_", "-"), type=float,
                            default=field.default, help=f"mm; default {field.default}")
    parser.add_argument("--output", type=Path,
                        default=Path("build-hardware/display-cradle.stl"))
    parser.add_argument("--force", action="store_true", help="Replace an existing STL")
    args = parser.parse_args()
    d = Dimensions(**{f.name: getattr(args, f.name) for f in fields(Dimensions)})
    try:
        d.validate()
    except ValueError as error:
        parser.error(str(error))
    frame_output = args.output.with_name(args.output.stem + "-frame.stl")
    for output in (args.output, frame_output):
        if output.exists() and not args.force:
            parser.error(f"{output} exists; use --force to replace it")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    for output, boxes, name in (
        (args.output, cradle_boxes(d), "tesla_headup_display_cradle"),
        (frame_output, frame_boxes(d), "tesla_headup_display_frame"),
    ):
        triangles = union_mesh(boxes)
        output.write_text(stl_text(triangles, name), encoding="ascii")
        print(f"Saved {output.resolve()} ({len(triangles)} triangles)")
    print("Assembled X/Y/Z: " + " × ".join(f"{v:g}" for v in d.outer) + " mm")
    print("Cavity X/Y/Z: " + " × ".join(f"{v:g}" for v in d.cavity) + " mm")
    print("USB/loading end: +X; screen: +Z; print flat base on bed.")
    print(f"Frame assembly height: Z={d.base + d.cavity[2]:g} mm.")
    print(f"TOTAL end roof widths: non-USB {d.end_lip:g} mm; USB {d.usb_lip:g} mm.")
    print("Frame has no snap/screw retention: secure externally for bench use.")


if __name__ == "__main__":
    main()
