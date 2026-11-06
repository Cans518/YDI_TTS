#!/usr/bin/env python3
"""Download the official packaging tools and verify GitHub's asset digests."""

import hashlib
import json
import os
import sys
import urllib.request
from pathlib import Path

TOOLS = {
    "linuxdeploy/linuxdeploy": "linuxdeploy-x86_64.AppImage",
    "linuxdeploy/linuxdeploy-plugin-qt": "linuxdeploy-plugin-qt-x86_64.AppImage",
}


def fetch_tools(destination):
    destination.mkdir(parents=True, exist_ok=True)
    manifest = []
    for repository, filename in TOOLS.items():
        headers = {"Accept": "application/vnd.github+json", "User-Agent": "YDI-TTS-build"}
        token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
        if token:
            headers["Authorization"] = f"Bearer {token}"
        request = urllib.request.Request(
            f"https://api.github.com/repos/{repository}/releases/tags/continuous",
            headers=headers,
        )
        with urllib.request.urlopen(request, timeout=60) as response:
            release = json.load(response)
        asset = next(item for item in release["assets"] if item["name"] == filename)
        expected = asset.get("digest", "")
        if not expected.startswith("sha256:"):
            raise RuntimeError(f"Missing SHA-256 digest for {filename}")
        url = asset["browser_download_url"]
        if not url.startswith(f"https://github.com/{repository}/releases/download/"):
            raise RuntimeError(f"Unexpected download origin for {filename}")
        # Do not forward the API token to release download redirects.
        with urllib.request.urlopen(url, timeout=60) as response:
            data = response.read()
        actual = hashlib.sha256(data).hexdigest()
        if actual != expected.removeprefix("sha256:"):
            raise RuntimeError(f"Checksum mismatch for {filename}")
        target = destination / filename
        target.write_bytes(data)
        target.chmod(0o755)
        manifest.append({"file": filename, "sha256": actual, "url": url})
        print(f"Verified {filename}", flush=True)
    (destination / "tools.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: fetch-appimage-tools.py DESTINATION")
    fetch_tools(Path(sys.argv[1]))
