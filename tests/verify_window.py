"""Check Windows GUI startup, redirected control and global fullscreen keys."""
from pathlib import Path
import json
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main() -> None:
    work = ROOT / "work/verify-window"
    work.mkdir(parents=True, exist_ok=True)
    binary = ROOT / "build/x64/rec64.exe"
    for arch, machine in [("x64", 0x8664), ("arm64", 0xaa64)]:
        data = (ROOT / f"build/{arch}/rec64.exe").read_bytes()
        pe = struct.unpack_from("<I", data, 0x3c)[0]
        assert data[pe:pe+4] == b"PE\0\0" and struct.unpack_from("<H", data, pe+4)[0] == machine
        assert struct.unpack_from("<H", data, pe+24+68)[0] == 2, "Executable must use the Windows GUI subsystem"
    config = work / "settings with spaces.yaml"
    config.write_text("# Keep settings\nmachine: {model: test}\nvideo: {fullscreen: false}\nui: {setup_page: display}\n", encoding="utf8")
    commands = [
        "state save work/verify-window/before.rcs", "key F12", "text",
        "key Alt+Return", "text", "release Alt+Return", "text",
        "key Alt+Keypad Enter", "text", "release Alt+Keypad Enter",
        "key Alt+Enter", "text", "release Alt+Enter", "key F12",
        "key Alt+Return", "release Alt+Return", "key F12", "text",
        "state save work/verify-window/after.rcs", "quit",
    ]
    run = subprocess.run([str(binary), "--hidden", "--config", str(config), "--control", "stdin"],
                         input="\n".join(commands)+"\n", text=True, capture_output=True, cwd=ROOT, timeout=15)
    assert run.returncode == 0, (run.stdout, run.stderr)
    replies = [json.loads(line) for line in run.stdout.splitlines()]
    assert len(replies) == len(commands) and all(reply["ok"] for reply in replies), replies
    screens = [reply["text"] for reply in replies if "text" in reply]
    values = [re.search(r"FULL SCREEN\s+(ON|OFF)", screen).group(1) for screen in screens]
    assert values == ["OFF", "ON", "ON", "OFF", "ON", "OFF"], values
    assert all(re.search(r"FILTER\s+CRT", screen) for screen in screens), "Alt+Enter must not edit the selected setup row"
    assert (work / "before.rcs").read_bytes() == (work / "after.rcs").read_bytes(), "Fullscreen keys reached the guest"
    assert config.read_text(encoding="utf8").startswith("# Keep settings")
    bad = subprocess.run([str(binary), "--hidden", "--frames", "0"], capture_output=True, text=True, timeout=5)
    assert bad.returncode == 2 and "positive count" in bad.stderr
    (work / "result.json").write_text(json.dumps({"status": "passed", "checks": ["x64/ARM64 GUI subsystem", "quoted arguments", "stdin/stdout JSON", "fullscreen on/off inside and outside setup", "key release", "keypad Enter", "guest state unchanged", "stderr and exit status"]}, indent=2)+"\n")
    print("Window checks passed: GUI subsystem, redirected I/O and fullscreen keys inside/outside setup")

if __name__ == "__main__":
    main()
