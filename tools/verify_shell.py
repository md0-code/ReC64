"""One headless check of setup, configuration, borders and host controls."""
from pathlib import Path
import argparse
import json
import struct
import subprocess
import zlib

ROOT = Path(__file__).resolve().parents[1]

def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--binary", type=Path, default=ROOT / "work/build-x64/rec64.exe")
    args = p.parse_args()
    work = ROOT / "work/verify-shell"
    work.mkdir(parents=True, exist_ok=True)
    config = work / "rec64.yaml"
    config.write_text("# Keep this comment\nmachine: {model: c64-pal}\nvideo: {look: crt, filter: nearest, palette: cjam, border: full}\n", encoding="utf8")
    script = work / "shell.script"
    script.write_text("\n".join([
        "frames 200", "expect READY.", "state save work/verify-shell/before-preview.rcs", "key F12", "expect MACHINE", "shot work/verify-shell/machine.png",
        "key RETURN", "key RETURN", "key RETURN", "key RETURN", "key F12", "state save work/verify-shell/after-preview.rcs", "key F12",
        "key RIGHT", "key RIGHT", "expect DISPLAY", "key RETURN", "key DOWN", "key RETURN",
        "key DOWN", "key RETURN", "expect MINIMAL", "shot work/verify-shell/display.png", "key F12",
        "shot work/verify-shell/minimal.png", "key F12", "key DOWN", "key DOWN", "key RETURN", "key F12",
        "shot work/verify-shell/none.png", "state save work/verify-shell/before-pause.rcs", "key PAUSE",
        "frames 3", "state save work/verify-shell/after-pause.rcs", "key PAUSE", "key F10", "expect Full speed",
        "key F10", "expect 100% speed", "poke C000 42", "key Alt+F12", "frames 200", "peek C000",
        "key Ctrl+F12", "frames 200", "peek C000", "expect READY.",
        "key F12", "key LEFT", "key LEFT", "key RETURN", "key DOWN", "key RETURN", "key F12",
        "frames 200", "expect READY.", "type PRINT 42", "enter", "frames 3", "expect 42", ""
    ]), encoding="ascii")
    result = subprocess.run([str(args.binary.resolve()), "--hidden", "--config", str(config), "--script", str(script)],
                            cwd=ROOT, text=True, capture_output=True, timeout=80)
    assert result.returncode == 0, (result.stdout, result.stderr)
    assert result.stdout.splitlines() == ["42", "00"], result.stdout
    assert (work / "before-pause.rcs").read_bytes() == (work / "after-pause.rcs").read_bytes()
    assert (work / "before-preview.rcs").read_bytes() == (work / "after-preview.rcs").read_bytes()
    for name, shape in [("machine", (384, 272)), ("display", (384, 272)), ("minimal", (352, 232)), ("none", (320, 200))]:
        data = (work / f"{name}.png").read_bytes()
        assert struct.unpack(">II", data[16:24]) == shape
    saved = config.read_text(encoding="utf8")
    assert saved.startswith("# Keep this comment") and "colodore" in saved and "c64c-pal" in saved and "6581" in saved
    # A fresh process accepts the comment-preserved file and its selected model.
    result = subprocess.run([str(args.binary.resolve()), "--hidden", "--config", str(config), "--frames", "200"],
                            cwd=ROOT, text=True, capture_output=True, timeout=25)
    assert result.returncode == 0, result.stderr
    palettes = work / "palettes"
    palettes.mkdir(exist_ok=True)
    colors = [0] * 16
    colors[1], colors[6], colors[14] = 0xffffff, 0x112233, 0xaabbcc
    (palettes / "custom.vpl").write_text("# Custom test palette\n" + "".join(f"{c>>16:02X} {c>>8&255:02X} {c&255:02X}\n" for c in colors), encoding="ascii")
    custom = work / "custom.yaml"
    custom.write_text("machine: {model: test}\nvideo: {palette: custom.vpl}\n", encoding="ascii")
    run = subprocess.run([str(args.binary.resolve()), "--hidden", "--config", str(custom), "--frames", "1", "--screenshot", str(work / "custom.png")], cwd=ROOT, capture_output=True, timeout=15)
    assert run.returncode == 0, run.stderr
    png = (work / "custom.png").read_bytes()
    at, compressed = 8, bytearray()
    while at < len(png):
        length = struct.unpack_from(">I", png, at)[0]
        if png[at+4:at+8] == b"IDAT":
            compressed += png[at+8:at+8+length]
        at += length+12
    assert zlib.decompress(compressed)[1:4] == bytes.fromhex("AABBCC")
    # Power off must finish even while a control channel remains open.
    proc = subprocess.Popen([str(args.binary.resolve()), "--hidden", "--model", "test", "--control", "stdin"], cwd=ROOT, text=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        proc.stdin.write("key Shift+F12\n")
        proc.stdin.flush()
        proc.wait(timeout=5)
        assert proc.returncode == 0 and json.loads(proc.stdout.readline())["ok"]
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.wait()
        proc.stdin.close();proc.stdout.close();proc.stderr.close()
    (work / "result.json").write_text(json.dumps({"status": "passed", "checks": ["setup", "palette and external VPL", "border", "pause", "speed", "reset", "cold boot", "model/SID change", "configuration reload", "power off"]}, indent=2) + "\n")
    print("Shell passed: setup, colours, borders, controls, model/SID change and configuration reload")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
