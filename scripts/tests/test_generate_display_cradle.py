from collections import Counter
from itertools import product
import math
import subprocess
import sys
import tempfile
from pathlib import Path
import unittest

from scripts.generate_display_cradle import Dimensions, cradle_boxes, frame_boxes, union_mesh


class DisplayCradleTests(unittest.TestCase):
    def test_dimensions(self):
        d = Dimensions()
        self.assertEqual(d.cavity, (56, 28, 10.5))
        self.assertEqual(d.outer, (57, 32, 13.7))

    def test_manifold_outward_winding_and_volume(self):
        for d, is_frame in product(
            (Dimensions(), Dimensions(clearance=0.3, lip=0.9),
             Dimensions(end_wall=0.8, end_lip=1.6, usb_lip=6)),
            (False, True),
        ):
            mesh = union_mesh(frame_boxes(d) if is_frame else cradle_boxes(d))
            edges = Counter()
            oriented = Counter()
            volume = 0
            for a, b, c in mesh:
                for p, q in ((a, b), (b, c), (c, a)):
                    self.assertNotEqual(p, q)
                    edges[tuple(sorted((p, q)))] += 1
                    oriented[p, q] += 1
                volume += (a[0]*(b[1]*c[2]-b[2]*c[1])
                           - a[1]*(b[0]*c[2]-b[2]*c[0])
                           + a[2]*(b[0]*c[1]-b[1]*c[0])) / 6
            self.assertTrue(edges)
            self.assertTrue(all(count == 2 for count in edges.values()))
            for (p, q), count in oriented.items():
                self.assertEqual(count, oriented[q, p])
            x, y, _ = d.outer
            cavity_x, _, cavity_z = d.cavity
            expected = x*y*d.base + (d.end_wall*y + 2*cavity_x*d.wall)*cavity_z
            if is_frame:
                strips = 2*x*(d.wall+d.lip)
                ends = (d.end_lip+d.usb_lip)*(y-2*(d.wall+d.lip))
                expected = (strips+ends)*d.lip_thickness
            self.assertTrue(math.isclose(volume, expected, rel_tol=1e-9))

    def test_end_ceilings_are_total_widths_not_extra_overhangs(self):
        d = Dimensions()
        def covered(x):
            return any(b[0] < x < b[1] and b[2] < d.outer[1]/2 < b[3]
                       and b[4] < 0.5 < b[5] for b in frame_boxes(d))
        self.assertTrue(covered(1.79))
        self.assertFalse(covered(1.81))
        self.assertFalse(covered(d.outer[0] - 7.01))
        self.assertTrue(covered(d.outer[0] - 6.99))
        self.assertAlmostEqual(d.end_lip - d.end_wall, 0.8)

    def test_frame_is_separate_and_fits_on_top_of_cradle(self):
        d = Dimensions()
        shoulder = d.base + d.cavity[2]
        self.assertEqual(max(b[5] for b in cradle_boxes(d)), shoulder)
        self.assertEqual(min(b[4] for b in frame_boxes(d)), 0)
        self.assertEqual(max(b[5] for b in frame_boxes(d)), d.lip_thickness)
        self.assertEqual(shoulder + d.lip_thickness, d.outer[2])

    def test_usb_end_is_open_below_lips(self):
        d = Dimensions()
        x, y, _ = d.outer
        for z in (d.base + 0.1, d.base + d.cavity[2] - 0.1):
            self.assertFalse(any(b[0] < x-0.01 < b[1] and
                                 b[2] < y/2 < b[3] and b[4] < z < b[5]
                                 for b in cradle_boxes(d)))

    def test_invalid_dimensions(self):
        for d in (Dimensions(length=0), Dimensions(depth=-1),
                  Dimensions(clearance=-1), Dimensions(wall=float("nan")),
                  Dimensions(lip=0.2), Dimensions(lip=20),
                  Dimensions(end_lip=-1), Dimensions(end_lip=0.2),
                  Dimensions(end_wall=2), Dimensions(usb_lip=-7),
                  Dimensions(end_lip=56), Dimensions(usb_lip=56)):
            with self.assertRaises(ValueError):
                cradle_boxes(d)

    def test_cli_and_overwrite_protection(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "cradle.stl"
            command = [sys.executable, "-m", "scripts.generate_display_cradle",
                       "--output", str(output)]
            subprocess.run(command, check=True, capture_output=True)
            self.assertTrue(output.read_text().startswith("solid tesla_headup_display_cradle"))
            frame = output.with_name("cradle-frame.stl")
            self.assertTrue(frame.read_text().startswith("solid tesla_headup_display_frame"))
            self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)
            output.unlink()
            self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)
            self.assertFalse(output.exists())
            subprocess.run(command + ["--force"], check=True, capture_output=True)


if __name__ == "__main__":
    unittest.main()
