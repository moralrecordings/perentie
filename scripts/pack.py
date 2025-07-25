#!/usr/bin/env python3

import argparse
import datetime
import pathlib
import shutil
import subprocess
import zipfile

now = datetime.datetime.now()

def add_file_to_archive(z: zipfile.ZipFile, src: pathlib.Path, dest: str, use_luac: bool):
    if src.suffix == ".lua":
        result = subprocess.run(["luac", "-o", "-", str(src)], capture_output=True)
        if result.returncode == 0:
            z.writestr(zipfile.ZipInfo(filename=str(dest), date_time=(now.year, now.month, now.day, now.hour, now.minute, now.second)), result.stdout)
            return
        else:
            print(f"Failed to run luac against {str(src)}, falling back to storage")
    z.write(src, dest)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("target", type=pathlib.Path, metavar="TARGET", help="Target archive (e.g. data.pt)")
    parser.add_argument("source", nargs="+", type=pathlib.Path, metavar="FILE", help="Source path to add; can be a file or directory")
    parser.add_argument("--no-luac", dest="luac", action="store_false", required=False, help="Don't precompile Lua files")
    parser.add_argument("--force", action="store_true", required=False, help="Overwrite destination")

    args = parser.parse_args()
    if args.target.exists():
        if args.target.is_file():
            if not args.force:
                parser.exit(1, f"{str(args.target)} exists, aborting\n")
        else:
            parser.exit(1, f"{str(args.target)} is a directorym, aborting\n")

    use_luac = args.luac
    if use_luac and not shutil.which("luac"):
        print("Couldn't find luac! Will not precompile scripts")
        use_luac = False

    with zipfile.ZipFile(args.target, "w", compression=zipfile.ZIP_STORED) as z: 
        for src in args.source:
            if not src.exists():
                print(f"Couldn't find {str(src)}, skipping")
            elif src.is_file():
                add_file_to_archive(z, src, src.name, use_luac)
            elif src.is_dir():
                parent = src.parent
                pre_size = len(str(parent)) + 1
                for srcin, _, files in src.walk():
                    for file in files:
                        path = srcin / file
                        add_file_to_archive(z, path, str(path)[pre_size:], use_luac)
        


if __name__ == "__main__":
    main()
