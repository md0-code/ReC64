"""One headless file UI flow, actual saves, auto-load and settings roundtrip."""
from pathlib import Path
import argparse
import json
import struct
import subprocess
from verify_states import machine_bytes

ROOT = Path(__file__).resolve().parents[1]

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=ROOT / "build/x64/rec64.exe")
    args = parser.parse_args()
    work = ROOT / "work/verify-files"
    work.mkdir(parents=True, exist_ok=True)
    for name in ("saved.prg", "recording.tap", "blank.d64"):
        target = work / name
        if target.exists():
            target.unlink()
    config = work / "rec64.yaml"
    config.write_text("# Keep file settings\nmachine: {model: c64-pal}\nui: {setup_page: media}\n", encoding="utf8")
    def run(name: str, commands: list[str], cfg: Path = config) -> None:
        script = work / f"{name}.script"
        script.write_text("\n".join(commands)+"\n", encoding="ascii")
        result = subprocess.run([str(args.binary.resolve()), "--hidden", "--config", str(cfg), "--script", str(script)], cwd=ROOT, text=True, capture_output=True, timeout=80)
        (work / f"{name}.stdout").write_text(result.stdout, encoding="utf8")
        (work / f"{name}.stderr").write_text(result.stderr, encoding="utf8")
        assert result.returncode == 0, (name, result.stdout, result.stderr)
    def save_name(name: str, changes: int = 0) -> list[str]:
        return ["key Ctrl+F2", "key TAB"] + ["key RETURN"]*changes + ["key TAB", "key Ctrl+A", f"type {name}"]
    run("save", ["frames 200", "expect READY.", "type 10 PRINT 42", "enter", "frames 3"] + save_name("saved.prg") + [
        "shot work/verify-files/save.png", "enter", "expect Saved", "state save work/verify-files/before-browser.rcs",
        "key Ctrl+F1", "expect FILES AND FOLDERS", "shot work/verify-files/files.png", "frames 3",
        "state save work/verify-files/after-browser.rcs", "key ESCAPE", "key F12", "expect TAPE", "expect FAST LOAD", "shot work/verify-files/media.png"])
    original = (work / "saved.prg").read_bytes()
    assert original[:2] == b"\x01\x08" and b"42" in original
    assert machine_bytes(work / "before-browser.rcs") == machine_bytes(work / "after-browser.rcs")
    # The browser selects the second real T64 entry; the original KERNAL
    # and BASIC then LOAD and RUN it through the auto-typist.
    programs = [original[2:], original[2:].replace(b"42", b"43", 1)]
    tape = bytearray(64+64+sum(map(len, programs)))
    tape[:19] = b"C64S tape image file"
    struct.pack_into("<HHH", tape, 32, 0x101, 2, 2)
    offset = 128
    for i, data in enumerate(programs):
        entry = 64+32*i
        struct.pack_into("<BBHH", tape, entry, 1, 0x82, 0x801, 0x801+len(data))
        struct.pack_into("<I", tape, entry+8, offset)
        tape[entry+16:entry+32] = (b"FIRST" if not i else b"SECOND").ljust(16, b" ")
        tape[offset:offset+len(data)] = data
        offset += len(data)
    (work / "programs.t64").write_bytes(tape)
    run("load-and-save", ["drop work/verify-files/programs.t64", "expect SECOND", "shot work/verify-files/contents.png", "key DOWN", "enter", "until 20 43", "expect 43", "shot work/verify-files/loaded.png"] +
        save_name("recording.tap", 1) + ["enter", "key Ctrl+F4", "expect RECORD", "key ESCAPE", "key Alt+T", "expect Tape sound muted by fast load"] +
        save_name("blank.d64", 1) + ["enter"] + save_name("saved.prg", 1) + ["enter", "expect REPLACE EXISTING FILE?", "shot work/verify-files/overwrite.png", "key ESCAPE", "key ESCAPE"])
    assert (work / "saved.prg").read_bytes() == original, "Cancel overwrote the program"
    assert (work / "recording.tap").read_bytes()[:13] == b"C64-TAPE-RAW\x01"
    assert (work / "blank.d64").stat().st_size == 174848
    assert config.read_text(encoding="utf8").startswith("# Keep file settings")
    # Explicit setting masks preserve inheritance after comments and images
    # are written back. Read the actual board's serialized option bits.
    def program_bits(state: Path) -> int:
        data = state.read_bytes(); at = 12
        for _ in range(2):
            length = struct.unpack_from("<H", data, at)[0]; at += 2+length
        roms = struct.unpack_from("<I", data, at)[0]; at += 4+32*roms
        while at+12 <= len(data):
            tag, _, length = struct.unpack_from("<4sII", data, at); at += 12
            if tag == b"CMED":
                version = struct.unpack_from("<I", data, at)[0]
                assert version in (1, 2, 3, 4, 5, 6)
                slots = at + 6 + (26 if version >= 2 else 0)
                assert data[slots+8] == 0  # no tape image precedes program
                return struct.unpack_from("<I", data, slots+13)[0]
            at += length
        raise AssertionError("CMED not found")
    for explicit, expected in [(False, 0), (True, 14)]:
        cfg = work / f"inherit-{int(explicit)}.yaml"
        cfg.write_text("# Keep inheritance\nmachine: {model: c64-pal}\nmedia:\n  tape: {fast: false, sound: false, autoload: false}\n  program: {image: " + json.dumps(str(work / "saved.prg")) + (", fast: true" if explicit else "") + "}\n", encoding="utf8")
        for pass_ in range(2):
            state = f"work/verify-files/inherit-{int(explicit)}-{pass_}.rcs"
            run(f"inherit-{int(explicit)}-{pass_}", [f"state save {state}"], cfg)
            assert program_bits(ROOT / state) == expected
        assert cfg.read_text(encoding="utf8").startswith("# Keep inheritance")
    (work / "result.json").write_text(json.dumps({"status": "passed", "checks": ["native load/save pages", "browser pauses guest", "PRG save", "T64 entry auto-load and RUN", "TAP record target", "blank D64", "overwrite cancellation", "tape controls", "configuration inheritance roundtrip"]}, indent=2)+"\n")
    print("File shell passed: browser, native saves, T64 auto-load, overwrite and configuration inheritance")

if __name__ == "__main__":
    main()
