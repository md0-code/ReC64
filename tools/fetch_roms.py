"""Fetch the hash-identified firmware images.

Only the firmware URLs in tools/pins.py are requested; an image
listed with several URLs is joined from those parts in order. --check
verifies installed files without writing.
--from-dir DIR imports matching images by content, whatever their filenames.
"""
from __future__ import annotations
import argparse
import hashlib
import sys
import urllib.request
from pathlib import Path
import pins
ROOT = Path(__file__).resolve().parents[1]

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--from-dir", type=Path)
    args = parser.parse_args()
    local = {}
    if args.from_dir and not args.check:
        for path in args.from_dir.iterdir():
            if path.is_file() and path.stat().st_size <= 32768:
                data = path.read_bytes()
                local[hashlib.sha256(data).hexdigest()] = data
    failures = 0
    for rom in pins.ROMS:
        name = rom["key"] + ".bin"   # any name works: ROMs are recognised by content
        path = ROOT / "roms" / name
        def valid(data: bytes) -> bool:
            return len(data) == rom["bytes"] and hashlib.sha256(data).hexdigest() == rom["sha256"]
        if path.is_file() and valid(path.read_bytes()):
            print(f'{name}: OK')
            continue
        if args.check:
            print(f'{name}: missing or wrong hash')
            failures += 1
            continue
        try:
            if args.from_dir:
                data = local[rom["sha256"]]
            else:
                data = b""
                for url in rom["urls"]:
                    with urllib.request.urlopen(url, timeout=30) as reply:
                        data += reply.read(rom["bytes"] + 1)
            if not valid(data):
                raise ValueError("firmware size or SHA-256 differs from the pin")
            path.parent.mkdir(exist_ok=True)
            path.write_bytes(data)
            print(f'{name}: installed')
        except (OSError, ValueError, KeyError) as error:
            print(f'{name}: {error}', file=sys.stderr)
            failures += 1
    return int(failures != 0)

if __name__ == "__main__":
    raise SystemExit(main())
