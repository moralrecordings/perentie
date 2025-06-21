#!/usr/bin/env python3

import argparse
import pathlib

from PIL import Image

def fix_outline(path: pathlib.Path):
    img = Image.open(open(path, "rb")) 
    px = img.load()
    for y in range(1, img.height - 1):
        for x in range(1, img.width - 1):
            if px[x, y] == 0xff:
                px[x-1, y-1] |= 0x7f
                px[x, y-1] |= 0x7f
                px[x+1, y-1] |= 0x7f
                px[x-1, y] |= 0x7f
                px[x+1, y] |= 0x7f
                px[x-1, y+1] |= 0x7f
                px[x, y+1] |= 0x7f
                px[x+1, y+1] |= 0x7f
    img.save(f"{path}.mod", "PNG")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Font outline fixer.\nBMFont creates nice fontmaps with a precomputed single-pixel outline. Only trouble is, it doesn't square the corners, making them slightly less readable. This script fixes them in-place to have square outlines.")
    parser.add_argument("SOURCE", type=pathlib.Path, nargs="+", help="Source images")
    args = parser.parse_args()
    for f in args.SOURCE:
        if f.is_file():
            fix_outline(f)
