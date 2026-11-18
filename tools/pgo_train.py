"""Exercise an instrumented build for profile-guided optimisation.

python tools/pgo_train.py BINARY

Headless sessions that need only the firmware in roms/: BASIC, a disk written and
read back through the emulated drive, sprites and the graphics modes, and sound on
both SID types, at normal and at full speed. Scratch files go to work/pgo.
"""
from __future__ import annotations
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "work/pgo"

def blank_d64() -> bytes:
    """An empty formatted 35-track disk."""
    sectors = [21 if t <= 17 else 19 if t <= 24 else 18 if t <= 30 else 17 for t in range(1, 36)]
    data = bytearray(174848)
    bam = 17 * 21 * 256
    data[bam:bam + 3] = bytes((18, 1, 0x41))
    for track, count in enumerate(sectors, 1):
        at = bam + 4 * track
        data[at] = count
        for s in range(count):
            data[at + 1 + s // 8] |= 1 << (s & 7)
    data[bam + 4 * 18] = 17
    data[bam + 4 * 18 + 1] &= 0xFC
    data[bam + 144:bam + 171] = b"\xa0" * 27
    data[bam + 144:bam + 149] = b"TRAIN"
    data[bam + 165:bam + 167] = b"2A"
    data[bam + 257] = 255
    return bytes(data)

def video() -> list[str]:
    """Eight sprites over text, then every graphics mode, scroll and border setting."""
    lines = ["frames 160", "poke 3000 " + " ".join("ff" if i % 3 != 1 else "a5" for i in range(63)),
             "poke 07f8 c0 c0 c0 c0 c0 c0 c0 c0", "poke d000 18 32 40 60 a0 90 f0 c8 20 fa 58 30 10 e0 ff 80",
             "poke d010 c0", "poke d025 02 05", "poke d027 01 02 03 04 05 07 08 0a",
             "poke 0400 " + " ".join("%02x" % ((i * 7 + 1) & 255) for i in range(200)),
             "poke d800 " + " ".join("%02x" % (i & 15) for i in range(200)),
             "poke d015 ff", "frames 40", "poke d017 aa", "poke d01d cc", "poke d01c f0", "frames 40"]
    for d011, d016 in (("1b", "c8"), ("1b", "d8"), ("3b", "c8"), ("3b", "d8"), ("5b", "c8"), ("1b", "c3"), ("13", "c8"), ("17", "c5")):
        lines += [f"poke d011 {d011}", f"poke d016 {d016}", "frames 30"]
    return lines + ["poke d011 1b", "poke d016 c8", "poke d015 00", "frames 100"]

def sound() -> list[str]:
    """Three voices through every waveform, with the filter swept and each of its outputs in turn."""
    lines = ["frames 160", "poke d405 09 f0", "poke d40c 09 f0", "poke d413 09 f0",
             "poke d400 45 1d 00 08", "poke d407 a2 0e 00 04", "poke d40e 51 07 00 0c", "poke d417 f7"]
    for control, mode in (("21", "1f"), ("41", "2f"), ("11", "4f"), ("81", "1f"), ("51", "3f"), ("23", "5f"), ("15", "0f")):
        lines += [f"poke d404 {control}", "poke d40b 41", "poke d412 11", f"poke d418 {mode}"]
        for cutoff in ("10", "40", "80", "c0", "f0"):
            lines += [f"poke d416 {cutoff}", "frames 25"]
    return lines + ["poke d404 20", "poke d40b 40", "poke d412 10", "frames 100"]

def disk(image: Path) -> list[str]:
    return ["frames 160", f"insert disk {image.as_posix()}", 'type 10 FORI=1TO40:PRINT"TRAINING";I:NEXT', "enter",
            'type SAVE"TRAIN",8', "enter", "frames 1200", "type NEW", "enter", 'type LOAD"TRAIN",8', "enter", "frames 700",
            "type RUN", "enter", "frames 250", 'type LOAD"$",8', "enter", "frames 500", "type LIST", "enter", "frames 150"]

def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    binary = Path(sys.argv[1]).resolve()
    WORK.mkdir(parents=True, exist_ok=True)
    basic = ["frames 160", "type 10 FORI=1TO200:PRINT I*I;:NEXT", "enter", "type RUN", "enter", "frames 600"]
    sessions = [("basic", "c64-pal", "6581", basic), ("ntsc", "c64c-ntsc", "8580", basic),
                ("video", "c64-pal", "6581", video()), ("video-ntsc", "c64-ntsc", "6581", video()),
                ("video-fast", "c64-pal", "6581", ["speed full"] + video()),
                ("sound-6581", "c64-pal", "6581", sound()), ("sound-8580", "c64c-pal", "8580", sound())]
    for name, fast in (("disk", False), ("disk-fast", True)):
        image = WORK / f"{name}.d64"
        image.write_bytes(blank_d64())
        sessions.append((name, "c64-pal", "6581", (["speed full"] if fast else []) + disk(image)))
    for name, model, sid, commands in sessions:
        config = WORK / f"{name}.yaml"
        script = WORK / f"{name}.txt"
        config.write_text(f"machine:\n  model: {model}\n  options:\n    sid: {sid}\n", encoding="utf8")
        script.write_text("\n".join(commands) + "\n", encoding="utf8")
        run = subprocess.run([str(binary), "--hidden", "--config", str(config), "--script", str(script)],
                             cwd=ROOT, capture_output=True, text=True)
        if run.returncode:
            print(f"{name}: {run.stderr.strip() or run.stdout.strip()}", file=sys.stderr)
            return 1
        print(f"{name}: trained", flush=True)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
