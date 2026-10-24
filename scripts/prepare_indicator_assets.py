#!/usr/bin/env python3

"""Prepare project-created dashboard indicator assets."""

from pathlib import Path
import math
import xml.etree.ElementTree as ET

from PIL import Image, ImageDraw, ImageOps


PROJECT_ROOT = Path(__file__).resolve().parent.parent
SOURCE_DIRECTORY = PROJECT_ROOT / "assets" / "icons" / "project"
OUTPUT_DIRECTORY = SOURCE_DIRECTORY / "prepared"
PROJECT_ICON_DIRECTORY = PROJECT_ROOT / "assets" / "icons" / "project"


def native_autopilot(output_size=(77, 72)) -> Image.Image:
    """Project-local geometric steering badge, not an enlarged manual bitmap.

    Draw the curved bridge and swept lower openings at 8x during preparation.
    The final outer alpha mask is computed from a circle's signed distance,
    independently of the inner artwork or Pillow's ellipse approximation.
    """
    scale = 8
    width, height = 96, 90
    blue = (0, 113, 255, 255)  # RGB565 0x039F, shared with active road lines.
    white = (244, 247, 248, 255)
    image = Image.new("RGBA", (width * scale, height * scale), blue)
    draw = ImageDraw.Draw(image)

    def circle(radius: float, color: tuple[int, ...]) -> None:
        draw.ellipse((int((48 - radius) * scale), int((45 - radius) * scale),
                      int((48 + radius) * scale) - 1, int((45 + radius) * scale) - 1),
                     fill=color)

    circle(26, white)

    def curve(start, control1, control2, end):
        points = []
        for step in range(65):
            t = step / 64
            points.append(tuple((1-t)**3 * start[axis] + 3*(1-t)**2*t * control1[axis]
                                + 3*(1-t)*t*t * control2[axis] + t**3 * end[axis]
                                for axis in (0, 1)))
        return points

    # Keep the original Tesla-style upper bridge and two swept lower windows,
    # rather than a generic Y-shaped steering wheel.
    upper = [(48 + 21.5 * math.cos(math.radians(angle)),
              45 + 21.5 * math.sin(math.radians(angle))) for angle in range(191, 350)]
    upper += curve(upper[-1], (59, 37.5), (37, 37.5), upper[0])
    draw.polygon([(x * scale, y * scale) for x, y in upper], fill=blue)
    lower = curve((27, 49), (33, 46), (44, 54), (43, 64))
    lower += curve(lower[-1], (42.8, 67.5), (37, 64.5), (33, 60.5))
    lower += curve(lower[-1], (29.5, 57), (27.5, 53), lower[0])
    for points in (lower, [(96 - x, y) for x, y in lower]):
        draw.polygon([(x * scale, y * scale) for x, y in points], fill=blue)
    # Remove subpixel rasterizer bias without changing the designed silhouette.
    image = Image.blend(image, ImageOps.mirror(image), 0.5)
    # Area coverage provides antialiasing without a resampling halo outside
    # the badge, particularly visible against the HUD's black background.
    # Native footer badge: 80% of its previous 96x90 presentation size.
    width, height = output_size
    image = image.resize((width, height), Image.Resampling.BOX)
    mask = Image.new("L", (width, height))
    radius = 41 * min(width / 96, height / 90) + 0.5
    mask.putdata([round(255 * max(0, min(1, radius - math.hypot(x - (width - 1) / 2, y - (height - 1) / 2))))
                  for y in range(height) for x in range(width)])
    image.putalpha(mask)
    # Keep fully transparent RGB black too, for previews which flatten alpha.
    pixels = image.load()
    for y in range(height):
        for x in range(width):
            if pixels[x, y][3] == 0:
                pixels[x, y] = (0, 0, 0, 0)
    return image


def project_turn_arrow(right: bool = False) -> Image.Image:
    """Rasterize the project's simple SVG polygon; never read a Tesla bitmap.

    Rounded SVG joins are the polygon plus a centered stroke and vertex discs.
    Supersampled area coverage avoids ringing around the transparent silhouette.
    The right raster is mirrored after downsampling to guarantee pixel symmetry.
    """
    svg = ET.parse(PROJECT_ICON_DIRECTORY / "turn-left.svg").getroot()
    polygon = svg.find("{http://www.w3.org/2000/svg}polygon")
    if polygon is None:
        raise ValueError("Turn-arrow SVG must contain its geometric polygon")
    scale = 16
    size = (int(svg.attrib["width"]), int(svg.attrib["height"]))
    color = polygon.attrib["fill"]
    points = [tuple(float(value) * scale for value in pair.split(","))
              for pair in polygon.attrib["points"].split()]
    radius = float(polygon.attrib["stroke-width"]) * scale / 2
    image = Image.new("RGBA", (size[0] * scale, size[1] * scale))
    draw = ImageDraw.Draw(image)
    draw.polygon(points, fill=color)
    draw.line(points + [points[0]], fill=color, width=round(2 * radius))
    for x, y in points:
        draw.ellipse((x - radius, y - radius, x + radius, y + radius), fill=color)
    image = image.resize(size, Image.Resampling.BOX)
    # Flat RGB beneath edge alpha prevents dark color fringes after RGB565 packing.
    alpha = image.getchannel("A")
    image = Image.new("RGBA", size, color)
    image.putalpha(alpha)
    return ImageOps.mirror(image) if right else image


def main() -> None:
    OUTPUT_DIRECTORY.mkdir(parents=True, exist_ok=True)
    project_output = PROJECT_ICON_DIRECTORY / "prepared"
    project_output.mkdir(parents=True, exist_ok=True)
    for right, filename in ((False, "turn-left.png"), (True, "turn-right.png")):
        project_turn_arrow(right).save(project_output / filename, optimize=True)
    native_autopilot().save(OUTPUT_DIRECTORY / "autopilot-road-scene-wide.png",
                           format="PNG", optimize=True)
    native_autopilot((38, 36)).save(OUTPUT_DIRECTORY / "autopilot-road-scene.png",
                                  format="PNG", optimize=True)


if __name__ == "__main__":
    main()
