"""Headless host controls, port swapping, persistence and Controllers page."""
from pathlib import Path
import argparse
import json
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=ROOT / "work/build-x64/rec64.exe")
    args = parser.parse_args()
    work = ROOT / "work/verify-input"
    work.mkdir(parents=True, exist_ok=True)
    config = work / "rec64.yaml"
    config.write_text("# Preserve input settings\nmachine: {model: c64-pal}\nkeyboard: {RUN STOP: F8}\n", encoding="utf8")
    script = work / "input.script"
    script.write_text("\n".join([
        "frames 200", "expect READY.", "poke DC02 00", "poke DC03 00",
        "key F11", "expect Keyboard joystick on", "key Up", "peek DC00", "release Up", "peek DC00",
        "key Shift+F11", "expect Joystick port 1", "key Up", "peek DC01", "release Up", "peek DC01",
        "key F11", "expect Keyboard joystick off", "key F12", "key RIGHT", "key RIGHT", "key RIGHT", "key RIGHT",
        "expect CONTROLLERS", "expect JOYSTICK", "shot work/verify-input/controllers.png", "key RETURN",
        "expect 1351 MOUSE", "key DOWN", "key RETURN", "key RETURN", "expect PADDLES", "key F12", ""
    ]), encoding="ascii")
    run = subprocess.run([str(args.binary.resolve()), "--hidden", "--config", str(config), "--script", str(script)],
                         cwd=ROOT, capture_output=True, text=True, timeout=30)
    assert run.returncode == 0, (run.stdout, run.stderr)
    assert run.stdout.splitlines() == ["FE", "FF", "FE", "FF"], run.stdout
    saved = config.read_text(encoding="utf8")
    assert saved.startswith("# Preserve input settings") and "mouse1351" in saved and "paddles" in saved and "RUN STOP" in saved
    reload = subprocess.run([str(args.binary.resolve()), "--hidden", "--config", str(config), "--frames", "1"],
                            cwd=ROOT, capture_output=True, text=True, timeout=15)
    assert reload.returncode == 0, reload.stderr
    (work / "result.json").write_text(json.dumps({"status": "passed", "checks": ["F11", "port swap", "CIA inputs", "Controllers page", "configuration reload"]}, indent=2) + "\n")
    print("Input shell passed: F11, port swap, CIA inputs, Controllers page and configuration reload")

if __name__ == "__main__":
    main()
