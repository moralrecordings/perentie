#!/usr/bin/env python3

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
    src_dir = pathlib.Path(sys.argv[1])
    dest_dir = pathlib.Path(sys.argv[2])
    for f in src_dir.iterdir():
        if f.is_file():
            transform(f, dest_dir)
