#!/usr/bin/env python3
"""Generate Inter LVGL assets with tabular digits mapped before rasterization."""
import argparse
from pathlib import Path
import shutil
import subprocess
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont

ROOT = Path(__file__).resolve().parents[1]
FONTS = ROOT / 'assets/fonts'
OUTPUT = ROOT / 'components/adapters/lvgl_rm67162/assets'

def prepare(weight, name, optical_size):
    font = instantiateVariableFont(TTFont(FONTS / 'Inter-Variable.ttf'),
                                   {'wght': weight, 'opsz': optical_size})
    gsub = font['GSUB'].table
    substitutions = {}
    for record in gsub.FeatureList.FeatureRecord:
        if record.FeatureTag != 'tnum':
            continue
        for index in record.Feature.LookupListIndex:
            for subtable in gsub.LookupList.Lookup[index].SubTable:
                if hasattr(subtable, 'ExtSubTable'):
                    subtable = subtable.ExtSubTable
                substitutions.update(getattr(subtable, 'mapping', {}))
    assert substitutions, 'Inter must contain the tnum feature'
    for cmap in font['cmap'].tables:
        if cmap.isUnicode():
            for code in range(ord('0'), ord('9') + 1):
                glyph = cmap.cmap.get(code)
                if glyph in substitutions:
                    cmap.cmap[code] = substitutions[glyph]
    cmap = font.getBestCmap()
    widths = {font['hmtx'].metrics[cmap[code]][0] for code in range(48,58)}
    assert len(widths) == 1, f'{name}: digits are not tabular: {widths}'
    path = FONTS / f'Inter-{name}.ttf'
    font.save(path)
    return path

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--converter', type=Path, help='Path to lv_font_conv.js (version 1.5.3)')
    parser.add_argument('--node', default=shutil.which('node') or 'node')
    args = parser.parse_args()
    regular = prepare(400, 'Regular', 32)
    medium = prepare(500, 'Medium', 14)
    semibold = prepare(600, 'SemiBold', 14)
    fonts = [(regular,64,'tesla_headup_inter_regular_64','0x30-0x39'),
             (regular,100,'tesla_headup_inter_regular_100','0x30-0x39'),
             (regular,110,'tesla_headup_inter_regular_110','0x30-0x39'),
             (medium,16,'tesla_headup_inter_medium_16','0x20-0x7e'),
             (medium,18,'tesla_headup_inter_medium_18','0x20-0x7e'),
             (medium,24,'tesla_headup_inter_medium_24','0x20-0x7e'),
             (semibold,18,'tesla_headup_inter_semibold_18','0x30-0x39'),
             (semibold,24,'tesla_headup_inter_semibold_24','0x20-0x7e'),
             (semibold,40,'tesla_headup_inter_semibold_40','0x30-0x39,0x41,0x4d,0x58')]
    for source,size,symbol,chars in fonts:
        command = [args.node,str(args.converter)] if args.converter else [shutil.which('npx') or 'npx','--yes','lv_font_conv@1.5.3']
        command += ['--font',source.relative_to(ROOT).as_posix(),'--range',chars,'--size',str(size),'--bpp','4',
                    '--format','lvgl','--no-compress','--no-prefilter','--no-kerning',
                    '--lv-include','lvgl.h','--lv-font-name',symbol,
                    '--output',(OUTPUT/f'{symbol}.c').relative_to(ROOT).as_posix()]
        subprocess.run(command,check=True,cwd=ROOT)
        print(f"Generated {symbol}: Inter {source.stem.replace('Inter-', '')}, {size}px, tabular digits")
if __name__ == '__main__':
    main()
