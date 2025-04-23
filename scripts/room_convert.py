#!/usr/bin/env python3

import argparse
import re
import pathlib
import subprocess
import sys

def transform(src_file: pathlib.Path, output_dir: pathlib.Path) -> None:
    if not src_file.is_file():
        raise ValueError("src_file must be a file")
    if not output_dir.is_dir():
        raise ValueError("output_dir must be a dir")

    # get the crop dimensions
    dims_raw = subprocess.run(["identify", "-format", "%@", str(src_file)], capture_output=True, text=True)
    dims = re.match(r"(\d+)x(\d+)\+(\d+)\+(\d+)", dims_raw.stdout)
    if not dims:
        raise ValueError(f"could not extract image crop dimensions: {dims_raw.stdout}, {dims_raw.stderr}")
    width, height, x_off, y_off = map(int, dims.groups())

    img_x = width // 2
    img_y = height

    dest_file = output_dir / src_file.name

    subprocess.call(["magick", str(src_file), "-colors", "256", "-trim", str(dest_file)])
    print(f"{src_file.stem}_bg = PTBackground(PTImage(\"{str(dest_file)}\", {img_x}, {img_y}), {x_off + img_x}, {y_off + img_y})")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Room image converter.\nUse your graphics editor to export the room as one image per layer. This tool will convert them to 256 colours, crop the transparency, and give you the Lua commands to load the cropped image as part of your room definition.")
    parser.add_argument("SOURCE", type=pathlib.Path, nargs="+", help="Source images")
    parser.add_argument("DEST", type=pathlib.Path, help="Destination path")
    args = parser.parse_args()
    for f in args.SOURCE:
        if f.is_file():
            transform(f, args.DEST)
