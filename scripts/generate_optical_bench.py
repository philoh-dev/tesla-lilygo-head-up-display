#!/usr/bin/env python3
"""Printable adjustable Tesla Head-Up Display optical bench. Python 3.9+, no dependencies.

All coordinates are mm. X runs along the base, Y across it, Z upward.
M4 fasteners/large washers lock sliding feet; tape/straps secure holders to
wedges during bench experiments. Not an automotive mount or a ray tracer.
"""

import argparse
from dataclasses import dataclass
import json
import math
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

from scripts.generate_display_cradle import (
    Dimensions, cradle_boxes, frame_boxes, stl_text, union_mesh,
)


@dataclass(frozen=True)
class Bench:
    length: float = 220
    width: float = 140
    foot_length: float = 40
    foot_width: float = 120
    plate: float = 4
    bolt_clearance: float = 4.5  # Square passages for round M4 screws.
    hinge_width: float = 36
    hinge_gap: float = 24.5  # Body span between inner pin roots.
    hinge_depth: float = 16
    hinge_height: float = 11
    pin_diameter: float = 5
    pin_axis_front: float = 4.5
    pin_axis_bottom: float = 4.5
    axis_height: float = 30  # Local Z, from underside of combiner foot.
    pin_clearance: float = 0.4  # Total pocket-width clearance.
    jaw_thickness: float = 4
    body_clearance: float = 0.3  # Each side between hinge body and jaws.
    mirror_width: float = 75
    mirror_height: float = 55
    mirror_depth: float = 1.1
    mirror_clearance: float = 0.3  # Per edge; no unknown rubber sleeves assumed.

    def validate(self):
        for name, value in vars(self).items():
            if not math.isfinite(value) or value <= 0:
                raise ValueError(f"{name} must be finite and positive")
        if self.length < 160 or self.width < self.foot_width + 20:
            raise ValueError("base needs length >=160 and width >= foot_width+20")
        protrusion = (self.hinge_width - self.hinge_gap) / 2
        if self.jaw_thickness + self.body_clearance >= protrusion:
            raise ValueError("jaws must engage pins without touching the hinge body")
        if self.axis_height - self.hinge_height < self.plate:
            raise ValueError("axis too low for hinge-body rotation clearance")
        if self.pin_axis_front < self.pin_diameter/2 or self.pin_axis_bottom < self.pin_diameter/2:
            raise ValueError("pin would protrude outside measured hinge envelope")
        if self.pin_axis_front + self.pin_diameter/2 > self.hinge_depth:
            raise ValueError("pin exceeds hinge depth")
        if self.pin_axis_bottom + self.pin_diameter/2 > self.hinge_height:
            raise ValueError("pin exceeds hinge height")


def foot_cuts(b: Bench, thickness: float):
    r = b.bolt_clearance / 2
    return [(b.foot_length/2-r, b.foot_length/2+r, y-r, y+r, -1, thickness+1)
            for y in (10, b.foot_width-10)]


def base_mesh(b: Bench):
    margin = (b.width-b.foot_width)/2
    r = b.bolt_clearance/2
    cuts = [(10, b.length-10, y-r, y+r, -1, b.plate+1)
            for y in (margin+10, b.width-margin-10)]
    return union_mesh([(0,b.length,0,b.width,0,b.plate)], cuts)


def foot_mesh(b: Bench, thickness=None):
    t = b.plate if thickness is None else thickness
    return union_mesh([(0,b.foot_length,0,b.foot_width,0,t)], foot_cuts(b,t))


def jaw_ranges(b: Bench):
    centre = b.foot_width/2
    gap = b.hinge_gap/2 + b.body_clearance
    return [(centre-gap-b.jaw_thickness, centre-gap),
            (centre+gap, centre+gap+b.jaw_thickness)]


def combiner_mesh(b: Bench):
    x = b.foot_length/2
    r = b.pin_diameter/2
    half_slot = (b.pin_diameter+b.pin_clearance)/2
    boxes = [(0,b.foot_length,0,b.foot_width,0,b.plate)]
    cuts = foot_cuts(b,b.axis_height+10)
    for y0,y1 in jaw_ranges(b):
        boxes.append((x-10,x+10,y0,y1,b.plate,b.axis_height+r))
        # Open-top square saddle: the pin rests on its lower tangent, with
        # its nominal centre exactly at axis_height. Not a circular bearing.
        cuts.append((x-half_slot,x+half_slot,y0-1,y1+1,
                     b.axis_height-r,b.axis_height+r+1))
    return union_mesh(boxes,cuts)


def mirror_mesh(b: Bench):
    wall, floor = 2, 2
    x = b.mirror_height+2*b.mirror_clearance+2*wall
    y = b.mirror_width+2*b.mirror_clearance+2*wall
    z = floor+b.mirror_depth+0.4
    return union_mesh([
        (0,x,0,y,0,floor), (0,wall,0,y,floor,z),
        (x-wall,x,0,y,floor,z), (0,x,0,wall,floor,z),
        (0,x,y-wall,y,floor,z),
    ])


def wedge_mesh(angle: float, run=40.0, width=10.0):
    """Exact triangular prism; angle is relative to the horizontal base."""
    if not math.isfinite(angle) or not 10 <= angle <= 75:
        raise ValueError("wedge angle must be between 10 and 75 degrees")
    h = run*math.tan(math.radians(angle))
    a,b,c = (0,0,0),(run,0,0),(run,0,h)
    d,e,f = (0,width,0),(run,width,0),(run,width,h)
    return [(a,b,c),(d,f,e), (a,d,e),(a,e,b),
            (b,e,f),(b,f,c), (a,c,f),(a,f,d)]


def shifted(mesh, offset=(0,0,0), angle=0):
    c,s = math.cos(math.radians(angle)),math.sin(math.radians(angle))
    return [tuple((p[0]*c-p[2]*s+offset[0],p[1]+offset[1],
                   p[0]*s+p[2]*c+offset[2]) for p in tri) for tri in mesh]


def holder_wedge(angle, holder, foot_length=40):
    # Keep the wedge below the holder's back, not projecting past its screen
    # or mirror. Display cradle is 32 mm deep after its 90-degree rotation.
    contact_length = 28 if holder == "display" else 50
    run = min(foot_length,contact_length*math.cos(math.radians(angle)))
    return wedge_mesh(angle,run=run)


def parts(b: Bench, angles=(30,45,60)):
    b.validate()
    d = Dimensions()
    result = {
        "base": base_mesh(b), "sliding-foot": foot_mesh(b),
        "stand-foot": union_mesh([(0,14,0,14,0,8)]),
        "combiner-foot": combiner_mesh(b),
        "pin-keeper": union_mesh([(0,20,0,b.jaw_thickness,0,2)]),
        "mirror-tray": mirror_mesh(b),
        "display-cradle": union_mesh(cradle_boxes(d)),
        "display-frame": union_mesh(frame_boxes(d)),
        "pin-fit-coupon": union_mesh([(0,20,0,b.jaw_thickness,0,10)],
            [(10-(b.pin_diameter+b.pin_clearance)/2,
              10+(b.pin_diameter+b.pin_clearance)/2,-1,b.jaw_thickness+1,3,11)]),
    }
    for height in (5,10,20):
        result[f"riser-{height}mm"] = foot_mesh(b,height)
    for angle in angles:
        for holder in ("display","mirror"):
            result[f"{holder}-wedge-{angle:g}deg"] = holder_wedge(angle,holder,b.foot_length)
    return result


def preview_mesh(b: Bench, meshes):
    """Illustrative assembly only; inter-part contact meshes must not be printed."""
    y0 = (b.width-b.foot_width)/2
    lift = 8  # Separate stand feet leave clearance for bolt heads/nuts below base.
    preview = shifted(meshes["base"],(0,0,lift))
    for x in (0,b.length-14):
        for y in (0,b.width-14):
            preview += shifted(meshes["stand-foot"],(x,y,0))
    # Display, mirror, combiner: illustrative positions, NOT solved optics.
    for x,name in ((15,"sliding-foot"),(85,"sliding-foot"),(160,"combiner-foot")):
        preview += shifted(meshes[name],(x,y0,lift+b.plate))
    for x,holder in ((15,"display"),(85,"mirror")):
        for y in (b.width/2-22,b.width/2+12):
            preview += shifted(holder_wedge(45,holder),(x,y,lift+2*b.plate))
    d = Dimensions()
    # Rotate the display's long dimension across the bench, then incline it.
    display = [tuple((d.outer[1]-p[1],p[0],p[2]) for p in tri)
               for tri in meshes["display-cradle"]]
    frame = shifted(meshes["display-frame"],(0,0,d.base+d.cavity[2]))
    frame = [tuple((d.outer[1]-p[1],p[0],p[2]) for p in tri) for tri in frame]
    preview += shifted(display+frame,(15,b.width/2-d.outer[0]/2,lift+2*b.plate),45)
    mirror_y = b.mirror_width+2*b.mirror_clearance+4
    preview += shifted(meshes["mirror-tray"],(85,b.width/2-mirror_y/2,lift+2*b.plate),45)
    for lo,hi in jaw_ranges(b):
        preview += shifted(meshes["pin-keeper"],
                           (170,y0+lo,lift+b.plate+b.axis_height+b.pin_diameter/2))
    return preview


def render_preview(mesh, output):
    """Optional CAD thumbnail; requires the repository's Pillow asset dependency."""
    from PIL import Image, ImageDraw
    def project(p):
        x,y,z = p
        return (0.8*x+0.6*y, 0.3*x-0.4*y-0.9*z)
    projected = [project(p) for tri in mesh for p in tri]
    xmin,xmax = min(p[0] for p in projected),max(p[0] for p in projected)
    ymin,ymax = min(p[1] for p in projected),max(p[1] for p in projected)
    scale = min(1060/(xmax-xmin),620/(ymax-ymin))
    def screen(p):
        u,v = project(p)
        return (70+(u-xmin)*scale,70+(v-ymin)*scale)
    image = Image.new("RGB",(1200,800),(244,247,250))
    draw = ImageDraw.Draw(image)
    def depth(tri):
        return sum(0.6*p[0]-0.8*p[1]+0.6*p[2] for p in tri)/3
    for tri in sorted(mesh,key=depth):
        a,b,c = tri
        u = [b[i]-a[i] for i in range(3)]
        v = [c[i]-a[i] for i in range(3)]
        n = (u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
        if n[0]*0.6-n[1]*0.8+n[2]*0.6 <= 0:
            continue
        size = math.sqrt(sum(t*t for t in n))
        light = max(0,(n[0]*0.3-n[1]*0.5+n[2]*0.8)/size)
        shade = 0.55+0.45*light
        color = tuple(int(t*shade) for t in (137,173,197))
        draw.polygon([screen(p) for p in tri],fill=color)
    draw.text((35,740),"Tesla Head-Up Display OPTICAL TEST BENCH - illustrative assembly, not solved optics",fill=(20,30,45))
    draw.text((35,762),"Display + wedges / mirror tray + wedges / combiner pin saddles. Real glass and hardware not shown.",fill=(20,30,45))
    image.save(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir",type=Path,default=Path("build-hardware/optical-bench"))
    parser.add_argument("--base-length",type=float,default=220)
    parser.add_argument("--base-width",type=float,default=140)
    parser.add_argument("--angles",type=float,nargs="+",default=[30,45,60])
    parser.add_argument("--pin-clearance",type=float,default=0.4)
    parser.add_argument("--force",action="store_true")
    parser.add_argument("--preview-png",action="store_true",help="Optional Pillow-rendered assembly thumbnail")
    args = parser.parse_args()
    b = Bench(length=args.base_length,width=args.base_width,pin_clearance=args.pin_clearance)
    try:
        meshes = parts(b,args.angles)
    except ValueError as error:
        parser.error(str(error))
    # Model-space defaults in the preview require the default-size base.
    if b.length >= 210:
        meshes["ASSEMBLY-PREVIEW-DO-NOT-PRINT"] = preview_mesh(b,meshes)
    if args.preview_png and b.length < 210:
        parser.error("illustrative preview needs base length >=210")
    outputs = {args.output_dir/(name+".stl"): stl_text(mesh,"tesla_headup_"+name)
               for name,mesh in meshes.items()}
    manifest = {
        "units":"mm", "dimensions":vars(b),
        "quantity":{"base":1,"stand-foot":4,"sliding-foot":2,"combiner-foot":1,
                    "pin-keeper":2,"mirror-tray":1,"display-cradle":1,
                    "display-frame":1},
        "optional":"Print risers as needed; two matching wedges per tilted holder.",
        "preview":"Illustrative assembly, no mirror/glass optics; NOT for printing.",
        "fasteners":"M4 bolts/nuts/large washers for feet; lengths depend on risers.",
        "retention":"External removable tape/straps required for stand feet, wedges, mirror tray, display frame and pin keepers. No snap/angle locks.",
    }
    outputs[args.output_dir/"manifest.json"] = json.dumps(manifest,indent=2)+"\n"
    archive = args.output_dir/"printable-parts.zip"
    if not args.force:
        for output in list(outputs) + [archive] + ([args.output_dir/"preview.png"] if args.preview_png else []):
            if output.exists():
                parser.error(f"{output} exists; use --force")
    args.output_dir.mkdir(parents=True,exist_ok=True)
    for output,content in outputs.items():
        output.write_text(content,encoding="ascii")
    if args.preview_png:
        render_preview(meshes["ASSEMBLY-PREVIEW-DO-NOT-PRINT"],args.output_dir/"preview.png")
    with ZipFile(archive,"w",ZIP_DEFLATED) as bundle:
        for output,content in outputs.items():
            if not output.name.startswith("ASSEMBLY-PREVIEW"):
                bundle.writestr(output.name,content)
        guide = Path(__file__).resolve().parents[1]/"docs"/"optical-bench.md"
        if guide.exists():
            bundle.writestr("README.md",guide.read_text(encoding="utf-8"))
    print(f"Generated {len(meshes)} STL files in {args.output_dir.resolve()}")
    print(f"Printable parts bundle: {archive.resolve()}")
    print("Print individual parts only, NOT the assembly preview. See docs/optical-bench.md.")


if __name__ == "__main__":
    main()
