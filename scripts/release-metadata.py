#!/usr/bin/env python3
"""Generate portable release checksums without publishing local machine paths."""

import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def project_version():
    text = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r"project\(YDI_TTS VERSION ([0-9.]+)", text)
    if not match:
        raise RuntimeError("Project version not found")
    return match.group(1)


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def write_manifest(directory):
    assets = sorted(
        path for path in directory.iterdir()
        if path.is_file() and path.suffix.lower() in {".exe", ".appimage", ".dmg", ".zip"}
    )
    if not assets:
        raise RuntimeError("No release assets found")
    revision = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
    ).strip()
    entries = [
        {"file": path.name, "bytes": path.stat().st_size, "sha256": sha256(path)}
        for path in assets
    ]
    (directory / "SHA256SUMS").write_text(
        "".join(f'{item["sha256"]}  {item["file"]}\n' for item in entries),
        encoding="utf-8", newline="\n",
    )
    manifest = {
        "application": "YDI_TTS",
        "version": project_version(),
        "source_commit": revision,
        "qt_version": "6.8.2",
        "macos_signing": "ad-hoc; not Developer ID signed or notarized",
        "artifacts": entries,
    }
    (directory / "release-manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8", newline="\n",
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=("version", "manifest"))
    parser.add_argument("directory", nargs="?", type=Path)
    arguments = parser.parse_args()
    if arguments.command == "version":
        print(project_version())
    else:
        if arguments.directory is None:
            parser.error("manifest requires a release directory")
        write_manifest(arguments.directory)
