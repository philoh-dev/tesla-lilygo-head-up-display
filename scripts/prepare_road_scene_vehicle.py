#!/usr/bin/env python3

"""Prepare road-scene vehicle assets for fixed LVGL presentation boxes."""

from pathlib import Path

from PIL import Image, ImageOps


PROJECT_ROOT = Path(__file__).resolve().parent.parent
SOURCE = PROJECT_ROOT / "assets" / "vehicles" / "supplied" / "drive-chase.png"
PARK_SOURCE = PROJECT_ROOT / "assets" / "vehicles" / "road-scene-front-three-quarter.png"
OUTPUT = PROJECT_ROOT / "assets" / "vehicles" / "prepared" / "road-scene-vehicle.png"
PRESENTATION_SIZE = (96, 92)
BLIND_SPOT_PRESENTATION_SIZE = (128, 112)
MINIMUM_VISIBLE_ALPHA = 8
DRIVING_CAR_SCALE_PERCENT = 75


def visible_artwork(path: Path) -> Image.Image:
    with Image.open(path) as source:
        artwork = source.convert("RGBA")

    alpha = artwork.getchannel("A").point(
        lambda opacity: opacity if opacity >= MINIMUM_VISIBLE_ALPHA else 0
    )
    artwork.putalpha(alpha)
    visible_bounds = alpha.getbbox()
    if visible_bounds is None:
        raise ValueError(f"{path.name} has no visible pixels")
    return artwork.crop(visible_bounds)


def centered_asset(
    artwork: Image.Image, size: tuple[int, int], scale_percent: int = 100
) -> Image.Image:
    artwork = artwork.copy()
    fit = tuple(dimension * scale_percent // 100 for dimension in size)
    artwork.thumbnail(fit, Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", size, (0, 0, 0, 0))
    offset = (
        (size[0] - artwork.width) // 2,
        (size[1] - artwork.height) // 2,
    )
    canvas.alpha_composite(artwork, offset)
    return canvas


def main() -> None:
    artwork = visible_artwork(SOURCE)
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    centered_asset(artwork, PRESENTATION_SIZE, DRIVING_CAR_SCALE_PERCENT).save(
        OUTPUT, format="PNG", optimize=True
    )
    parked = visible_artwork(PARK_SOURCE)
    centered_asset(parked, (256, 160)).save(OUTPUT.with_name("road-scene-park.png"))
    native_park = centered_asset(parked, (410, 307))
    # Align visible artwork, not the transparent presentation-box padding.
    bottom_aligned = Image.new("RGBA", native_park.size, (0, 0, 0, 0))
    bottom_aligned.alpha_composite(
        native_park, (0, native_park.height - native_park.getchannel("A").getbbox()[3]))
    bottom_aligned.save(OUTPUT.with_name("road-scene-park-wide.png"))
    # Both states share the approved perspective; mirror their right-side views.
    for suffix, filename in (("", "blind-spot-left-red-perspective.png"),
                             ("-grey", "blind-spot-left-grey-perspective.png")):
        source = visible_artwork(PROJECT_ROOT / "assets/vehicles" / filename)
        for side in ("left", "right"):
            artwork_side = source if side == "left" else ImageOps.mirror(source)
            name = f"blind-spot-{side}{suffix}"
            centered_asset(artwork_side, (160, 140), DRIVING_CAR_SCALE_PERCENT).save(
                OUTPUT.parent / f"{name}-wide.png")
            centered_asset(artwork_side, BLIND_SPOT_PRESENTATION_SIZE, DRIVING_CAR_SCALE_PERCENT).save(
                OUTPUT.parent / f"{name}.png")
    centered_asset(artwork, (128, 121), DRIVING_CAR_SCALE_PERCENT).save(
        OUTPUT.with_name("road-scene-vehicle-wide.png"))


if __name__ == "__main__":
    main()
