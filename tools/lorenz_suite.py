"""Fetch pinned public-domain Lorenz programs and run them on the C64 board.

python tools/lorenz_suite.py fetch
python tools/lorenz_suite.py run [--jobs 8] [--only cia1ta,irq] [--new-cia]
Outputs, downloaded programs and logs stay in work/testprogs.
"""
from __future__ import annotations
import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "work/testprogs"
REVISION = 46417
URL = f"https://svn.code.sf.net/p/vice-emu/code/!svn/bc/{REVISION}/testprogs/general/Lorenz-2.15/"
NEW = {"cia1ta", "cia1tb", "cia1tab", "cia2ta", "cia2tb", "icr01", "imr", "irq", "nmi"}

def fetch() -> None:
    WORK.mkdir(parents=True, exist_ok=True)
    lock = {"revision": REVISION, "url": URL, "sha256": {}}
    def get(name: str) -> bytes:
        data = urllib.request.urlopen(URL + name, timeout=45).read()
        (WORK / Path(name).name).write_bytes(data)
        lock["sha256"][Path(name).name] = hashlib.sha256(data).hexdigest()
        return data
    names = set()
    for disk in range(1, 5):
        data = get(f"Disk{disk}.d64")
        sectors = [21 if t <= 17 else 19 if t <= 24 else 18 if t <= 30 else 17 for t in range(1, 36)]
        track, sector = 18, 1
        seen = set()
        while track:
            assert (track, sector) not in seen
            seen.add((track, sector))
            offset = (sum(sectors[:track-1]) + sector) * 256
            block = data[offset:offset+256]
            for i in range(8):
                entry = block[i*32:(i+1)*32]
                if entry[2] & 7 == 2:
                    names.add(entry[5:21].rstrip(b"\xa0").decode("ascii").lower())
            track, sector = block[:2]
    names -= {"nextdisk", "nextdisk1", "nextdisk2", "nextdisk3"}
    names |= {"cia1tab", "cia1tb123", "cia2tb123"}
    files = [f"src/{name}.prg" for name in sorted(names)] + [f"src/{name}new.prg" for name in sorted(NEW)]
    with concurrent.futures.ThreadPoolExecutor(8) as pool:
        list(pool.map(get, files))
    get("readme.txt")
    lock["programs"] = sorted(names)
    (WORK / "lock.json").write_text(json.dumps(lock, indent=2) + "\n", encoding="utf8")
    print(f"Fetched {len(names)} C64 tests and {len(NEW)} new-CIA variants at r{REVISION}")

def run(args: argparse.Namespace) -> int:
    lock = json.loads((WORK / "lock.json").read_text(encoding="utf8"))
    names = sorted(NEW) if args.new_cia else lock["programs"]
    if args.only:
        wanted = set(args.only.split(","))
        assert wanted <= set(names), wanted - set(names)
        names = [name for name in names if name in wanted]
    files = [WORK / f"{name}{'new' if args.new_cia else ''}.prg" for name in names]
    for file in files:
        assert hashlib.sha256(file.read_bytes()).hexdigest() == lock["sha256"][file.name], file
    jobs = min(args.jobs, len(files))
    label = "new" if args.new_cia else "old"
    if args.only:
        label += "-" + hashlib.sha256(",".join(names).encode()).hexdigest()[:8]
    def worker(job: int) -> dict:
        batch = files[job::jobs]
        listing = WORK / f"run-{label}-{job}.txt"
        listing.write_text("".join(str(p.resolve()) + "\n" for p in batch), encoding="utf8")
        log = WORK / f"result-{label}-{job}.log"
        with log.open("w", encoding="utf8") as output:
            result = subprocess.run([str(args.runner.resolve()), str(listing), str(int(args.new_cia))],
                                    cwd=ROOT, text=True, stdout=output, stderr=output)
        text = log.read_text(encoding="utf8")
        passed = len(re.findall(r"^PASS ", text, re.M))
        print(f"{label} batch {job+1}/{jobs}: {passed}/{len(batch)} passed", flush=True)
        if result.returncode:
            print("\n".join(line for line in text.splitlines() if line.startswith("FAIL ")), flush=True)
        return {"passed": passed, "total": len(batch), "status": result.returncode, "log": str(log)}
    with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
        batches = list(pool.map(worker, range(jobs)))
    result = {"revision": REVISION, "new_cia": args.new_cia, "passed": sum(b["passed"] for b in batches),
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
    p.add_argument("--new-cia", action="store_true")
    args = p.parse_args()
    if args.action == "fetch":
        fetch()
        return 0
    assert 1 <= args.jobs <= 32
    return run(args)

if __name__ == "__main__":
    raise SystemExit(main())
