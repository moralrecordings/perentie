#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import pathlib
import subprocess
import sys
from typing import Sequence

CROP_RE = re.compile(r"^(\d+)x(\d+)\+(\d+)\+(\d+)$")
OFFSET_RE = re.compile(r"^(\d+),(\d+)$")

def get_crop_rect_from_dims(crop: str) -> tuple[int, int, int, int]:
    dims = CROP_RE.match(crop)
    if not dims:
        raise ValueError(f"could not extract image crop dimensions: {crop}")
    return (int(dims.group(1)), int(dims.group(2)), int(dims.group(3)), int(dims.group(4)))


def get_crop_rect_from_path(src_file: pathlib.Path, collapse: bool) -> tuple[int, int, int, int]:
    # get the crop dimensions from image
    dims_args = ["identify", "-format"] + (["%@"] if collapse else ["%g"]) +[str(src_file)] 
    dims_raw = subprocess.run(dims_args, capture_output=True, text=True)
    return get_crop_rect_from_dims(dims_raw.stdout)


def get_best_fit_rect(src_files: Sequence[pathlib.Path]):
    srces = iter(src_files)
    width, height, x_off, y_off = get_crop_rect_from_path(next(srces), True)
    left, top, right, bottom = x_off, y_off, x_off+width, y_off+height
    for src in srces:
        dims = get_crop_rect_from_path(src, True)
    
        left = min(left, dims[2])
        top = min(top, dims[3])
        right = max(right, dims[2]+dims[0])
        bottom = max(bottom, dims[3]+dims[1])
    return (right-left, bottom-top, left, top)


def transform(src_file: pathlib.Path, output_dir: pathlib.Path, collapse: bool, crop: tuple[int, int, int, int]|None, offset: str|None) -> None:
    if not src_file.is_file():
        raise ValueError("src_file must be a file")
    if not output_dir.is_dir():
        raise ValueError("output_dir must be a dir")

    if crop:
        dims = crop
    else:
        # get the crop dimensions from image
        dims = get_crop_rect_from_path(src_file, collapse)
    width, height, x_off, y_off = dims 

    img_x = width // 2
    img_y = height

    if offset:
        off = OFFSET_RE.match(offset)
        if not off:
            raise ValueError(f"could not extract offset: {offset}")
        img_x, img_y = int(off.group(1)) - x_off, int(off.group(2)) - y_off

    dest_file = output_dir / src_file.name

    args = ["magick", str(src_file), "-colors", "256"] + (["-crop", f"{crop[0]}x{crop[1]}+{crop[2]}+{crop[3]}"] if crop else ["-trim"] if (collapse) else []) + [str(dest_file)]

    subprocess.call(args)
    print(f"{src_file.stem}_bg = PTBackground(PTImage(\"{str(dest_file)}\", {img_x}, {img_y}), {x_off + img_x}, {y_off + img_y})")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Room image converter.\nUse your graphics editor to export the room as one image per layer. This tool will convert them to 256 colours, crop the transparency, and give you the Lua commands to load the cropped image as part of your room definition.")
    parser.add_argument("SOURCE", type=pathlib.Path, nargs="+", help="Source images")
    parser.add_argument("DEST", type=pathlib.Path, help="Destination path")
    parser.add_argument("--no-collapse", dest="collapse", help="Don't collapse the empty margin of the image", action="store_false")
    parser.add_argument("--crop", dest="crop", type=str, help="Use a custom crop rect: {w}x{h}+{x_off}+{y_off}")
    parser.add_argument("--offset", dest="offset", type=str, help="Use a custom offset: {x_off},{y_off}")
    parser.add_argument("--best-fit", dest="best_fit", action="store_true", help="Choose a crop rect that fits every image")
    args = parser.parse_args()

    crop = None
    if args.crop:
        crop = get_crop_rect_from_dims(args.crop)
    elif args.best_fit:
        crop = get_best_fit_rect(args.SOURCE)

    for f in args.SOURCE:
        if f.is_file():
            transform(f, args.DEST, args.collapse, crop, args.offset)
