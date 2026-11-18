"""Fetch Wolfgang Lorenz's public-domain test suite and run it on the C64 board.

python tools/lorenz_suite.py fetch
python tools/lorenz_suite.py run [--jobs 8] [--only cia1ta,irq]
Outputs, downloaded programs and logs stay in work/testprogs.
"""
from __future__ import annotations
import argparse
import concurrent.futures
import hashlib
import io
import json
from pathlib import Path
import re
import subprocess
import sys
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "work/testprogs"
SECTORS = [21 if t <= 17 else 19 if t <= 24 else 18 if t <= 30 else 17 for t in range(1, 36)]
LOAD_NEXT = bytes((0x4C, 0x6F, 0xE1))  # every test ends by loading its successor

def programs(disk: bytes) -> dict[str, bytes]:
    """The PRG files of a D64 image, by lower-case name."""
    def block(track: int, sector: int) -> bytes:
        offset = (sum(SECTORS[:track-1]) + sector) * 256
        return disk[offset:offset+256]
    found = {}
    track, sector = 18, 1
    while track:
        directory = block(track, sector)
        for i in range(8):
            entry = directory[i*32:(i+1)*32]
            if entry[2] & 7 != 2:
                continue
            name = entry[5:21].rstrip(bytes([0xA0])).decode("ascii").lower()
            body, (at, s) = b"", entry[3:5]
            while at:
                data = block(at, s)
                at, s = data[:2]
                body += data[2:] if at else data[2:s+1]
            if LOAD_NEXT in body:  # skips the start, finish and change-disk programs
                found[name] = body
        track, sector = directory[:2]
    return found

def fetch() -> None:
    pin = json.loads((ROOT / "docs/sources.lock.json").read_text(encoding="utf8"))["test_suites"]["lorenz"]
    archive = urllib.request.urlopen(pin["url"], timeout=45).read()
    assert len(archive) == pin["bytes"] and hashlib.sha256(archive).hexdigest() == pin["sha256"], "archive differs from the lock"
    WORK.mkdir(parents=True, exist_ok=True)
    for old in WORK.glob("*.prg"):
        old.unlink()
    lock = {"sha256": {}}
    with zipfile.ZipFile(io.BytesIO(archive)) as z:
        for disk in ("Disk1.d64", "Disk2.d64", "Disk3.d64"):
            for name, body in programs(z.read(disk)).items():
                (WORK / f"{name}.prg").write_bytes(body)
                lock["sha256"][f"{name}.prg"] = hashlib.sha256(body).hexdigest()
    lock["programs"] = sorted(name[:-4] for name in lock["sha256"])
    (WORK / "lock.json").write_text(json.dumps(lock, indent=2) + "\n", encoding="utf8")
    print(f"Fetched {len(lock['programs'])} C64 tests")

def run(args: argparse.Namespace) -> int:
    lock = json.loads((WORK / "lock.json").read_text(encoding="utf8"))
    names = lock["programs"]
    if args.only:
        wanted = set(args.only.split(","))
        assert wanted <= set(names), wanted - set(names)
        names = [name for name in names if name in wanted]
    files = [WORK / f"{name}.prg" for name in names]
    for file in files:
        assert hashlib.sha256(file.read_bytes()).hexdigest() == lock["sha256"][file.name], file
    jobs = min(args.jobs, len(files))
    label = "all"
    if args.only:
        label += "-" + hashlib.sha256(",".join(names).encode()).hexdigest()[:8]
    def worker(job: int) -> dict:
        batch = files[job::jobs]
        listing = WORK / f"run-{label}-{job}.txt"
        listing.write_text("".join(str(p.resolve()) + "\n" for p in batch), encoding="utf8")
        log = WORK / f"result-{label}-{job}.log"
        with log.open("w", encoding="utf8") as output:
            result = subprocess.run([str(args.runner.resolve()), str(listing), "0"],
                                    cwd=ROOT, text=True, stdout=output, stderr=output)
        text = log.read_text(encoding="utf8")
        passed = len(re.findall(r"^PASS ", text, re.M))
        print(f"{label} batch {job+1}/{jobs}: {passed}/{len(batch)} passed", flush=True)
        if result.returncode:
            print("\n".join(line for line in text.splitlines() if line.startswith("FAIL ")), flush=True)
        return {"passed": passed, "total": len(batch), "status": result.returncode, "log": str(log)}
    with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
        batches = list(pool.map(worker, range(jobs)))
    result = {"passed": sum(b["passed"] for b in batches),
              "total": len(files), "batches": batches}
    (WORK / f"result-{label}.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf8")
    print(f"Lorenz {label}: {result['passed']}/{result['total']} passed")
    return int(any(b["status"] for b in batches))

def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("action", choices=("fetch", "run"))
    p.add_argument("--runner", type=Path, default=ROOT / "work/build-x64" / ("lorenz_suite.exe" if sys.platform == "win32" else "lorenz_suite"))
    p.add_argument("--jobs", type=int, default=8)
    p.add_argument("--only")
    args = p.parse_args()
    if args.action == "fetch":
        fetch()
        return 0
    assert 1 <= args.jobs <= 32
    return run(args)

if __name__ == "__main__":
    raise SystemExit(main())
