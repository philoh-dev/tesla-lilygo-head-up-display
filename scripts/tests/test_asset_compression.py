"""Lossless asset round trips against an independent LZ4 block reader."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("asset_compression", ROOT / "scripts/asset_compression.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

def expand(block):
    result = bytearray()
    cursor = 0
    while cursor < len(block):
        token = block[cursor]
        cursor += 1
        literals = token >> 4
        if literals == 15:
            while True:
                extra = block[cursor]; cursor += 1; literals += extra
                if extra != 255: break
        result.extend(block[cursor:cursor+literals]); cursor += literals
        if cursor == len(block): break
        offset = int.from_bytes(block[cursor:cursor+2], "little"); cursor += 2
        match = token & 15
        if match == 15:
            while True:
                extra = block[cursor]; cursor += 1; match += extra
                if extra != 255: break
        for _ in range(match + 4): result.append(result[-offset])
    return bytes(result)

class AssetCompressionTests(unittest.TestCase):
    def test_literal_run_overlap_and_large_offsets_round_trip(self):
        import random
        noise = random.Random(42).randbytes(70000)
        for data in (b"", b"a", b"abc", b"a" * 70000, b"abcdef" * 10000,
                     bytes(range(256)) * 300, noise, noise + noise[:1000]):
            with self.subTest(length=len(data)):
                self.assertEqual(expand(module.compress_block(data)), data)

    def test_native_pngs_retain_exact_rgb565_colors_and_alpha(self):
        try:
            from PIL import Image
        except ImportError:
            self.skipTest("Pillow required for native asset checks")
        total = 0
        paths = list((ROOT / "assets/vehicles/prepared").glob("*-wide.png"))
        paths += [ROOT / "assets/icons/project/prepared/autopilot-road-scene-wide.png"]
        for path in paths:
            with Image.open(path) as image:
                colors, alpha = bytearray(), bytearray()
                for r, g, b, a in image.convert("RGBA").get_flattened_data():
                    colors.extend((((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)).to_bytes(2, "little"))
                    alpha.append(a)
            raw = bytes(colors + alpha)
            packed = module.compress_block(raw)
            self.assertEqual(expand(packed), raw)
            total += len(packed)
        self.assertLess(total, 225000)

    def test_compact_park_retains_exact_rgb565_colors_and_alpha(self):
        try:
            from PIL import Image
        except ImportError:
            self.skipTest("Pillow required for asset checks")
        with Image.open(ROOT / "assets/vehicles/prepared/road-scene-park.png") as image:
            self.assertEqual(image.size, (256, 160))
            colors, alpha = bytearray(), bytearray()
            for r, g, b, a in image.convert("RGBA").get_flattened_data():
                colors.extend((((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)).to_bytes(2, "little"))
                alpha.append(a)
        raw = bytes(colors + alpha)
        packed = module.compress_block(raw)
        self.assertEqual(len(raw), 122880)
        self.assertEqual(expand(packed), raw)
        self.assertLess(len(packed), len(raw))
