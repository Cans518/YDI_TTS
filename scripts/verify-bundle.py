#!/usr/bin/env python3
"""Reject packaged binaries that still reference the developer's Qt installation."""

import argparse
import subprocess
from pathlib import Path

MACHO = {b"\xcf\xfa\xed\xfe", b"\xfe\xed\xfa\xcf", b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca", b"\xca\xfe\xba\xbf"}


def verify(root, platform, forbidden):
    root = root.resolve()
    checked = set()
    for entry in root.rglob("*"):
        if not entry.is_file():
            continue
        path = entry.resolve()
        if not path.is_relative_to(root):
            raise RuntimeError(f"A packaged file links outside the bundle: {entry.name}")
        if path in checked:
            continue
        with path.open("rb") as stream:
            magic = stream.read(4)
        if platform == "linux" and magic == b"\x7fELF":
            rpath = subprocess.check_output(["patchelf", "--print-rpath", str(path)], text=True)
            if any(str(prefix) in rpath for prefix in forbidden):
                raise RuntimeError(f"Development RPATH remains in {entry.name}")
            checked.add(path)
        elif platform == "macos" and magic in MACHO:
            dependencies = subprocess.check_output(["otool", "-L", str(path)], text=True)
            for line in dependencies.splitlines():
                if not line.startswith("\t"):
                    continue
                dependency = line.strip().split(" (", 1)[0]
                if dependency.startswith("/") and not dependency.startswith(("/System/Library/", "/usr/lib/")):
                    raise RuntimeError(f"External dependency in {entry.name}: {dependency}")
            load_commands = subprocess.check_output(["otool", "-l", str(path)], text=True)
            for line in load_commands.splitlines():
                if line.strip().startswith("path ") and any(str(prefix) in line for prefix in forbidden):
                    raise RuntimeError(f"Development RPATH remains in {entry.name}")
            checked.add(path)
    if not checked:
        raise RuntimeError("No packaged binaries were inspected")
    print(f"Verified dependencies of {len(checked)} packaged binaries.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("platform", choices=("linux", "macos"))
    parser.add_argument("root", type=Path)
    parser.add_argument("forbidden", nargs="+", type=Path)
    args = parser.parse_args()
    verify(args.root, args.platform, args.forbidden)
