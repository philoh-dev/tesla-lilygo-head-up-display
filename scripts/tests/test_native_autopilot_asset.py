"""Geometry contracts for project-created steering and turn indicators."""

import importlib.util
from pathlib import Path
import sys
import math
import unittest

ROOT = Path(__file__).resolve().parents[2]


class NativeAutopilotAssetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        # Repository tooling checks also run without optional asset dependencies.
        try:
            import PIL  # noqa: F401
        except ImportError:
            raise unittest.SkipTest("Pillow is required for asset geometry checks")
        spec = importlib.util.spec_from_file_location("native_indicator_assets", ROOT / "scripts/prepare_indicator_assets.py")
        cls.module = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = cls.module
        spec.loader.exec_module(cls.module)

    def test_native_badge_is_centered_symmetric_and_has_no_external_marks(self) -> None:
        image = self.module.native_autopilot()
        self.assertEqual(image.size, (77, 72))
        pixels = image.load()
        for y in range(72):
            for x in range(77):
                self.assertEqual(pixels[x, y], pixels[76 - x, y])
                distance = math.hypot(x - 38, y - 35.5)
                self.assertEqual(pixels[x, y][3], round(255 * max(0, min(1, 33.3 - distance))))
                if distance > 34:
                    self.assertEqual(pixels[x, y][3], 0)
                if 25 < distance < 29:
                    self.assertEqual(pixels[x, y], (0, 113, 255, 255))
        self.assertEqual(pixels[38, 38], (244, 247, 248, 255))
        for point in ((38, 24), (29, 45), (47, 45)):
            self.assertEqual(pixels[point], (0, 113, 255, 255))

    def test_prepared_asset_matches_reproducible_geometry(self) -> None:
        from PIL import Image
        with Image.open(ROOT / "assets/icons/project/prepared/autopilot-road-scene-wide.png") as prepared:
            self.assertEqual(prepared.convert("RGBA").tobytes(), self.module.native_autopilot().tobytes())

    def test_compact_badge_is_presized_symmetric_and_reproducible(self) -> None:
        from PIL import Image
        image = self.module.native_autopilot((38, 36))
        self.assertEqual(image.size, (38, 36))
        pixels = image.load()
        radius = 41 * min(38 / 96, 36 / 90) + 0.5
        for y in range(36):
            for x in range(38):
                self.assertEqual(pixels[x, y], pixels[37 - x, y])
                distance = math.hypot(x - 18.5, y - 17.5)
                self.assertEqual(pixels[x, y][3], round(255 * max(0, min(1, radius - distance))))
        with Image.open(ROOT / "assets/icons/project/prepared/autopilot-road-scene.png") as prepared:
            self.assertEqual(prepared.convert("RGBA").tobytes(), image.tobytes())

    def test_project_arrows_are_presized_mirrored_and_have_clean_alpha(self) -> None:
        from PIL import Image, ImageOps
        directory = ROOT / "assets/icons/project/prepared"
        with Image.open(directory / "turn-left.png") as source:
            left = source.convert("RGBA")
        with Image.open(directory / "turn-right.png") as source:
            right = source.convert("RGBA")
        self.assertEqual(left.size, (32, 30))
        self.assertEqual(right.tobytes(), ImageOps.mirror(left).tobytes())
        self.assertEqual(left.getpixel((3, 15))[3], 255)  # arrowhead
        self.assertEqual(left.getpixel((29, 15))[3], 255)  # shaft
        self.assertEqual(left.getpixel((2, 2))[3], 0)  # clear corner
        alphas = set(left.getchannel("A").tobytes())
        self.assertIn(0, alphas)
        self.assertIn(255, alphas)
        self.assertGreater(len(alphas), 10)  # antialiased edges
        for pixel in left.get_flattened_data():
            if pixel[3]:
                self.assertEqual(pixel[:3], (53, 215, 116))

    def test_project_arrow_preparation_needs_no_manual_source(self) -> None:
        from unittest.mock import patch
        with patch.object(self.module, "SOURCE_DIRECTORY", ROOT / "unused-manual-input"):
            image = self.module.project_turn_arrow()
        from PIL import Image
        with Image.open(ROOT / "assets/icons/project/prepared/turn-left.png") as prepared:
            self.assertEqual(prepared.convert("RGBA").tobytes(), image.tobytes())


if __name__ == "__main__":
    unittest.main()
