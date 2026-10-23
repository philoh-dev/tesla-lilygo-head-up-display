from collections import Counter, defaultdict
from dataclasses import replace
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from zipfile import ZipFile

from scripts.generate_optical_bench import (
    Bench, base_mesh, combiner_mesh, foot_mesh, holder_wedge, jaw_ranges, parts, wedge_mesh,
)


def assert_solid(test, mesh):
    edges = Counter()
    directed = Counter()
    adjacency = defaultdict(set)
    volume = 0
    for tri in mesh:
        a,b,c = [tuple(round(t,8) for t in p) for p in tri]
        for p,q in ((a,b),(b,c),(c,a)):
            test.assertNotEqual(p,q)
            edges[tuple(sorted((p,q)))] += 1
            directed[p,q] += 1
            adjacency[p].add(q)
            adjacency[q].add(p)
        volume += (a[0]*(b[1]*c[2]-b[2]*c[1])
                   -a[1]*(b[0]*c[2]-b[2]*c[0])
                   +a[2]*(b[0]*c[1]-b[1]*c[0]))/6
    test.assertTrue(edges)
    test.assertTrue(all(n == 2 for n in edges.values()))
    for (p,q),n in directed.items():
        test.assertEqual(n,directed[q,p])
    seen = set()
    pending = [next(iter(adjacency))]
    while pending:
        p = pending.pop()
        if p not in seen:
            seen.add(p)
            pending.extend(adjacency[p]-seen)
    test.assertEqual(len(seen),len(adjacency))
    test.assertGreater(volume,0)
    return volume


class OpticalBenchTests(unittest.TestCase):
    def test_all_printable_parts_are_single_watertight_outward_solids(self):
        for name,mesh in parts(Bench()).items():
            with self.subTest(part=name):
                assert_solid(self,mesh)

    def test_base_and_foot_volumes_match_real_m4_clearances(self):
        b = Bench()
        expected = (b.length*b.width-2*(b.length-20)*b.bolt_clearance)*b.plate
        self.assertAlmostEqual(assert_solid(self,base_mesh(b)),expected)
        expected = (b.foot_length*b.foot_width-2*b.bolt_clearance**2)*b.plate
        self.assertAlmostEqual(assert_solid(self,foot_mesh(b)),expected)

    def test_jaws_engage_each_real_pin_not_hinge_body(self):
        b = Bench()
        centre = b.foot_width/2
        left,right = jaw_ranges(b)
        self.assertAlmostEqual(right[0]-left[1],25.1)
        self.assertGreater(left[0],centre-b.hinge_width/2)
        self.assertLess(left[1],centre-b.hinge_gap/2)
        self.assertGreater(right[0],centre+b.hinge_gap/2)
        self.assertLess(right[1],centre+b.hinge_width/2)
        self.assertEqual(b.pin_axis_front,4.5)
        self.assertEqual(b.pin_axis_bottom,4.5)
        # Both lower pin tangents lie at axis_height - 2.5 mm.
        mesh = combiner_mesh(b)
        self.assertIn(b.axis_height-2.5,{p[2] for t in mesh for p in t})

    def test_wedges_have_exact_requested_angles(self):
        for angle in (15,30,45,60,75):
            mesh = wedge_mesh(angle)
            height = max(p[2] for t in mesh for p in t)
            self.assertAlmostEqual(height,40*math.tan(math.radians(angle)))
            self.assertAlmostEqual(assert_solid(self,mesh),40*10*height/2,places=4)
        for angle in (0,90,float("nan")):
            with self.assertRaises(ValueError):
                wedge_mesh(angle)

    def test_invalid_bench_geometry(self):
        for b in (Bench(length=100),Bench(width=90),Bench(pin_clearance=-1),
                  replace(Bench(),jaw_thickness=6),replace(Bench(),pin_axis_bottom=2)):
            with self.assertRaises(ValueError):
                parts(b)

    def test_holder_wedges_do_not_project_past_the_holders(self):
        for angle in (10,30,45,60,75):
            for holder,available_length in (("display",32),("mirror",59.6)):
                mesh = holder_wedge(angle,holder)
                run = max(p[0] for tri in mesh for p in tri)
                self.assertLessEqual(run,40)
                self.assertLess(run/math.cos(math.radians(angle)),available_length)

    def test_cli_outputs_and_overwrite_protection(self):
        with tempfile.TemporaryDirectory() as directory:
            command = [sys.executable,"-m","scripts.generate_optical_bench",
                       "--output-dir",directory]
            subprocess.run(command,check=True,capture_output=True)
            self.assertTrue((Path(directory)/"base.stl").exists())
            self.assertTrue((Path(directory)/"manifest.json").exists())
            self.assertTrue((Path(directory)/"ASSEMBLY-PREVIEW-DO-NOT-PRINT.stl").exists())
            with ZipFile(Path(directory)/"printable-parts.zip") as bundle:
                self.assertIsNone(bundle.testzip())
                self.assertIn("base.stl",bundle.namelist())
                self.assertIn("README.md",bundle.namelist())
                self.assertNotIn("ASSEMBLY-PREVIEW-DO-NOT-PRINT.stl",bundle.namelist())
            self.assertNotEqual(subprocess.run(command,capture_output=True).returncode,0)
            subprocess.run(command+["--force"],check=True,capture_output=True)


if __name__ == "__main__":
    unittest.main()
