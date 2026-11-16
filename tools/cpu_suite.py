"""Fetch and run the pinned NMOS 6502 SingleStepTests, including every bus cycle.

The JSON is converted without changing any state or expected result. Only
test vectors and their MIT licence are fetched; no emulator source is used.
"""
from __future__ import annotations
import argparse
import concurrent.futures
import hashlib
import json
import struct
import subprocess
import time
import urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
COMMIT="2f6980a2d95757486c7bee24355c360e40e2a224"
BASE=f"https://raw.githubusercontent.com/SingleStepTests/65x02/{COMMIT}"
WORK=ROOT/"work/cpu"

def pack_state(state):
    out=struct.pack("<H5B",*(state[k] for k in ("pc","s","a","x","y","p")))
    out+=struct.pack("<H",len(state["ram"]))
    return out+b"".join(struct.pack("<HB",a,v) for a,v in state["ram"])

def fetch(op):
    path=WORK/f"{op:02x}.json"
    if not path.exists():
        for retry in range(3):
            try:
                path.write_bytes(urllib.request.urlopen(f"{BASE}/6502/v1/{path.name}",timeout=60).read())
                break
            except OSError:
                if retry==2:raise
                time.sleep(1)
    data=path.read_bytes();tests=json.loads(data)
    target=path.with_suffix(".bin")
    if not target.exists():
        with target.open("wb") as f:
            for test in tests:
                f.write(b"T"+pack_state(test["initial"])+pack_state(test["final"]))
                f.write(bytes([len(test["cycles"])]))
                for a,v,kind in test["cycles"]:f.write(struct.pack("<HBB",a,v,int(kind=="write")))
    return {"opcode":f"{op:02x}","sha256":hashlib.sha256(data).hexdigest(),"tests":len(tests)}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action",choices=["fetch","run"])
    parser.add_argument("--opcode",help="one hex opcode for a focused retry")
    parser.add_argument("--runner",type=Path,default=ROOT/"work/build-x64/cpu_suite.exe")
    args=parser.parse_args();WORK.mkdir(parents=True,exist_ok=True)
    opcodes=[int(args.opcode,16)] if args.opcode else list(range(256))
    if args.action=="fetch":
        with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
            rows=list(pool.map(fetch,opcodes))
        if not args.opcode:
            (WORK/"vectors.lock.json").write_text(json.dumps({"commit":COMMIT,"files":rows},indent=2)+"\n")
            (WORK/"LICENSE").write_bytes(urllib.request.urlopen(BASE+"/LICENSE").read())
        print(f"Fetched {sum(row['tests'] for row in rows)} pinned vectors",flush=True)
        return 0
    passed=0;failures=[]
    for op in opcodes:
        result=subprocess.run([str(args.runner.resolve()),str(WORK/f"{op:02x}.bin")],capture_output=True,text=True,timeout=60)
        if result.returncode:failures.append(result.stderr or f"opcode {op:02x}: exit {result.returncode}")
        else:passed+=int(result.stdout.split()[0])
    for failure in failures:print(failure.strip())
    report={"commit":COMMIT,"passed":passed,"failed_opcodes":len(failures),"bus_cycles_checked":True}
    (WORK/"result.json").write_text(json.dumps(report,indent=2)+"\n")
    print(f"{passed} vectors passed; {len(failures)} failing opcodes")
    return int(bool(failures))

if __name__=="__main__":raise SystemExit(main())
