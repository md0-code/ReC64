"""One headless foundation acceptance check; output stays under work/verify.

python tools/verify.py [--binary FILE] [--arm64 FILE] [--quick]
"""
from __future__ import annotations
import argparse
import hashlib
import json
import socket
import struct
import subprocess
import sys
import time
import wave
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=ROOT / "work/build-x64" / ("rec64.exe" if sys.platform == "win32" else "rec64"))
    parser.add_argument("--arm64", type=Path)
    parser.add_argument("--quick", action="store_true", help="the foundation has one short acceptance check")
    args = parser.parse_args()
    binary = args.binary.resolve()
    work = ROOT / "work/verify"
    work.mkdir(parents=True, exist_ok=True)
    script = work / "foundation.script"
    script.write_text("\n".join([
        "expect FOUNDATION TEST SCREEN", "type REC64", "expect REC64",
        "audio work/verify/tone.wav", "key A", "frames 3", "release A", "audio off",
        "poke 0400 42", "state save work/verify/stub.rcs", "shot work/verify/before.png",
        "poke 0400 FF", "enter", "state load work/verify/stub.rcs", "expect REC64A",
        "shot work/verify/after.png", "text", ""
    ]), encoding="ascii")
    run = subprocess.run([str(binary), "--hidden", "--script", str(script), "--control", "stdin",
                          "--screenshot", str(work / "final.png")],
                         cwd=ROOT, input="peek 0400\nrewind 1\nquit\n", text=True, capture_output=True, timeout=25)
    if run.returncode:
        raise RuntimeError(run.stderr or run.stdout)
    replies = [json.loads(line) for line in run.stdout.splitlines() if line.startswith('{')]
    assert len(replies) == 3 and replies[0]["ok"] and replies[0]["data"] == "42", replies
    assert not replies[1]["ok"] and replies[2]["ok"], replies
    before = (work / "before.png").read_bytes()
    assert before.startswith(b"\x89PNG\r\n\x1a\n")
    assert struct.unpack(">II", before[16:24]) == (384, 272)
    assert before == (work / "after.png").read_bytes() == (work / "final.png").read_bytes()
    with wave.open(str(work / "tone.wav"), "rb") as wav:
        assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (2, 2, 48000)
        assert any(wav.readframes(wav.getnframes()))
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]
    proc = subprocess.Popen([str(binary), "--hidden", "--control", f"tcp:{port}"], cwd=ROOT,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        deadline = time.monotonic() + 10
        while True:
            try:
                connection = socket.create_connection(("127.0.0.1", port), timeout=1)
                break
            except OSError:
                if proc.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError("TCP control did not start")
                time.sleep(.05)
        with connection, connection.makefile("rwb") as channel:
            channel.write(b"text\n"); channel.flush()
            answer = json.loads(channel.readline())
            assert answer["ok"] and "FOUNDATION TEST SCREEN" in answer["text"]
            channel.write(b"quit\n"); channel.flush()
            assert json.loads(channel.readline())["ok"]
        stdout, stderr = proc.communicate(timeout=5)
        assert proc.returncode == 0, (stdout, stderr)
    finally:
        if proc.poll() is None:
            proc.kill(); proc.communicate()
    if args.arm64:
        pe = args.arm64.read_bytes()
        offset = struct.unpack_from("<I", pe, 0x3C)[0]
        assert pe[offset:offset+4] == b"PE\0\0" and struct.unpack_from("<H", pe, offset+4)[0] == 0xAA64
    result = {"status": "passed", "binary": str(binary), "screenshot_sha256": hashlib.sha256(before).hexdigest(),
              "checks": ["script", "PNG", "state round trip", "audio", "stdin JSON", "TCP JSON"],
              "windows_arm64": bool(args.arm64)}
    (work / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print("Foundation passed: script, PNG, state round trip, audio, stdin and TCP control" + (", ARM64 PE" if args.arm64 else ""))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
