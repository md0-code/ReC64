"""Fetch hash-identified firmware data from the pinned VICE release.

Only the five firmware URLs in docs/sources.lock.json are requested. No
emulator source is fetched. --check verifies installed files without writing.
--from-dir DIR imports matching images by content, whatever their filenames.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import sys
import urllib.request
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--from-dir", type=Path)
    args = parser.parse_args()
    manifest = json.loads((ROOT / "docs/sources.lock.json").read_text())
    local = {}
    if args.from_dir and not args.check:
        for path in args.from_dir.iterdir():
            if path.is_file() and path.stat().st_size <= 32768:
                data = path.read_bytes()
                local[hashlib.sha256(data).hexdigest()] = data
    failures = 0
    for rom in manifest["roms"]:
        path = ROOT / "roms" / rom["file"]
        def valid(data: bytes) -> bool:
            return len(data) == rom["bytes"] and hashlib.sha256(data).hexdigest() == rom["sha256"]
        if path.is_file() and valid(path.read_bytes()):
            print(f'{rom["file"]}: OK')
            continue
        if args.check:
            print(f'{rom["file"]}: missing or wrong hash')
            failures += 1
            continue
        try:
            if args.from_dir:
                data = local[rom["sha256"]]
            else:
                request = urllib.request.Request(rom["url"], headers={"User-Agent": "rec64-fetch-roms"})
                with urllib.request.urlopen(request, timeout=30) as reply:
                    data = reply.read(rom["bytes"] + 1)
            if not valid(data):
                raise ValueError("firmware size or SHA-256 differs from the lock")
            path.parent.mkdir(exist_ok=True)
            path.write_bytes(data)
            print(f'{rom["file"]}: installed')
        except (OSError, ValueError, KeyError) as error:
            print(f'{rom["file"]}: {error}', file=sys.stderr)
            failures += 1
    return int(failures != 0)

if __name__ == "__main__":
    raise SystemExit(main())
