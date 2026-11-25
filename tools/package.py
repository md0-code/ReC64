"""Assembles a ReC64 release archive and checks it. tools/build_dist.ps1 (Windows) and tools/build_linux.sh (Linux)
call it after they have built the program.

    python tools/package.py PLATFORM BINARY [--libretro] [--out DIR] [--no-start]
    python tools/package.py --list PLATFORM [--libretro]

PLATFORM is windows-x64, windows-arm64, linux-x64 or linux-arm64. BINARY is the desktop executable or, with
--libretro, the shared core. The archive uses ReC64 or ReC64-Libretro before the version and platform.
The .zip (Windows) or .tar.gz (Linux, under a folder of the same name) goes to --out (default
dist/releases/), which holds the current version only: archives of other versions are removed.

Both archives include the documents. The desktop archive includes an empty roms/ folder, and the Libretro archive
ships the library and its info file.
The user supplies firmware and software. The program creates rec64.yaml on its first start. The binaries must match
the platform's format and architecture. Each relative link of the shipped documents must resolve inside the archive.
The finished archive is opened again to check its file list and the bytes of each member. Unless --no-start is given, a desktop archive
this host can run is then extracted afresh, the firmware of roms/ is copied in, and the program must start the C64 to
READY there (headless). Anything else stops the build.
"""

from __future__ import annotations

import argparse
import io
import platform as host_platform
import re
import shutil
import struct
import subprocess
import sys
import tarfile
import time
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NAME = "ReC64"
PLATFORMS = ("windows-x64", "windows-arm64", "linux-x64", "linux-arm64")
DOCS = {"README.md": "README.md", "docs/NOTES.md": "docs/NOTES.md", "docs/CHANGELOG.md": "docs/CHANGELOG.md",
        "docs/THIRD_PARTY.md": "docs/THIRD_PARTY.md", "LICENSE": "LICENSE"}
FOLDERS = ("roms",)
EXECUTABLES = {"windows-x64": ("pe", 0x8664), "windows-arm64": ("pe", 0xAA64),
               "linux-x64": ("elf", 0x3E), "linux-arm64": ("elf", 0xB7)}


class PackageError(Exception):
    pass


def version() -> str:
    text = (ROOT / "src" / "VERSION").read_text(encoding="ascii").strip()
    if not re.fullmatch(r"\d+\.\d+\.\d+", text):
        raise PackageError(f"src/VERSION must hold MAJOR.MINOR.PATCH, not {text!r}")
    return text


def program_name(platform: str) -> str:
    return "rec64.exe" if platform.startswith("windows") else "rec64"


def core_name(platform: str) -> str:
    return "rec64_libretro.dll" if platform.startswith("windows") else "rec64_libretro.so"


CORE_INFO = "info/rec64_libretro.info"


def binary_member(platform: str, libretro: bool) -> str:
    return f"cores/{core_name(platform)}" if libretro else program_name(platform)


def archive_name(platform: str, libretro: bool = False) -> str:
    name = f"{NAME}-Libretro" if libretro else NAME
    return f"{name}-{version()}-{platform}" + (".zip" if platform.startswith("windows") else ".tar.gz")


def expected_files(platform: str, libretro: bool = False) -> list[str]:
    files = [binary_member(platform, libretro), *DOCS]
    if libretro:
        files.append(CORE_INFO)
    return sorted(files)


def check_executable(platform: str, path: Path, shared: bool = False) -> None:
    kind, machine = EXECUTABLES[platform]
    with path.open("rb") as binary:
        head = binary.read(4096)
    if kind == "pe":
        offset = struct.unpack_from("<I", head, 0x3C)[0] if head[:2] == b"MZ" and len(head) > 0x40 else 0
        if (head[:2] != b"MZ" or offset + 24 > len(head) or head[offset:offset + 4] != b"PE\0\0" or
                struct.unpack_from("<H", head, offset + 4)[0] != machine or
                bool(struct.unpack_from("<H", head, offset + 22)[0] & 0x2000) != shared):
            raise PackageError(f"{path} is not a Windows {'DLL' if shared else 'executable'} for {platform}")
    else:
        elf_types = (3,) if shared else (2, 3)  # ET_DYN, including position-independent executables, and ET_EXEC
        if (len(head) < 64 or head[:6] != b"\x7fELF\x02\x01" or
                struct.unpack_from("<H", head, 18)[0] != machine or
                struct.unpack_from("<H", head, 16)[0] not in elf_types or
                (struct.unpack_from("<Q", head, 24)[0] == 0) != shared):
            raise PackageError(f"{path} is not a 64-bit little-endian ELF {'library' if shared else 'executable'} for {platform}")


def gather(platform: str, binary: Path, libretro: bool = False) -> dict[str, Path]:
    check_executable(platform, binary, shared=libretro)
    entries = {binary_member(platform, libretro): binary}
    if libretro:
        info = ROOT / "src" / "libretro" / Path(CORE_INFO).name
        if not info.is_file():
            raise PackageError(f"{info} is missing")
        entries[CORE_INFO] = info
    for member, source in DOCS.items():
        if not (ROOT / source).is_file():
            raise PackageError(f"{source} is missing")
        entries[member] = ROOT / source
    return entries


LINK = re.compile(r"(?<!!)\[[^\]]*\]\(([^)\s]+)(?:\s+\"[^\"]*\")?\)")


def broken_links(entries: dict[str, Path], empty_folders: tuple[str, ...]) -> list[str]:
    names = set(entries) | set(empty_folders)
    folders = {str(Path(n).parent).replace("\\", "/") for n in names}
    problems = []
    for name in sorted(n for n in entries if n.endswith(".md")):
        base = Path(name).parent
        for target in LINK.findall(entries[name].read_text(encoding="utf-8")):
            if re.match(r"^[a-z][a-z0-9+.-]*:", target, re.I) or target.startswith("#"):
                continue
            parts: list[str] = []
            for part in (base / target.split("#")[0]).as_posix().split("/"):
                if part == "..":
                    if not parts:
                        break
                    parts.pop()
                elif part and part != ".":
                    parts.append(part)
            else:
                if "/".join(parts) in names or "/".join(parts) in folders:
                    continue
            problems.append(f"{name}: link to {target}")
    return problems


def write_archive(platform: str, entries: dict[str, Path], target: Path, libretro: bool = False) -> None:
    stamp = time.localtime()[:6]
    empty_folders = () if libretro else FOLDERS
    if platform.startswith("windows"):
        with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
            for folder in empty_folders:
                info = zipfile.ZipInfo(folder + "/", stamp)
                info.external_attr = (0o40755 << 16) | 0x10
                z.writestr(info, b"")
            for name in sorted(entries):
                info = zipfile.ZipInfo(name, stamp)
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                z.writestr(info, entries[name].read_bytes())
        return
    top = archive_name(platform, libretro).removesuffix(".tar.gz")
    now = int(time.time())
    with tarfile.open(target, "w:gz", compresslevel=9, format=tarfile.GNU_FORMAT) as t:
        folders = sorted({str(p) for n in entries for p in Path(n).parents} | set(empty_folders))
        for folder in folders:
            info = tarfile.TarInfo(top if folder == "." else f"{top}/{Path(folder).as_posix()}")
            info.type, info.mode, info.mtime, info.uid, info.gid, info.uname, info.gname = tarfile.DIRTYPE, 0o755, now, 0, 0, "root", "root"
            t.addfile(info)
        for name in sorted(entries):
            data = entries[name].read_bytes()
            info = tarfile.TarInfo(f"{top}/{name}")
            info.size, info.mtime, info.uid, info.gid, info.uname, info.gname = len(data), now, 0, 0, "root", "root"
            info.mode = 0o755 if name == program_name(platform) else 0o644
            t.addfile(info, io.BytesIO(data))


def read_back(platform: str, archive: Path, libretro: bool = False) -> dict[str, bytes]:
    found: dict[str, bytes] = {}
    if platform.startswith("windows"):
        with zipfile.ZipFile(archive) as z:
            if z.testzip():
                raise PackageError(f"{archive}: a member is corrupt")
            for info in z.infolist():
                if info.is_dir():
                    continue
                if info.filename in found or info.filename.startswith(("/", "../")) or "\\" in info.filename:
                    raise PackageError(f"{archive}: bad member name {info.filename!r}")
                found[info.filename] = z.read(info)
        return found
    top = archive_name(platform, libretro).removesuffix(".tar.gz") + "/"
    with tarfile.open(archive, "r:gz") as t:
        for info in t:
            if info.isdir():
                continue
            if not info.isfile() or not info.name.startswith(top):
                raise PackageError(f"{archive}: unexpected member {info.name!r}")
            name = info.name[len(top):]
            if name in found or ".." in name.split("/"):
                raise PackageError(f"{archive}: bad member name {info.name!r}")
            found[name] = t.extractfile(info).read()  # type: ignore[union-attr]
    return found


def runnable_here(platform: str) -> bool:
    machine = host_platform.machine().lower()
    arch = "arm64" if machine in ("arm64", "aarch64") else "x64" if machine in ("amd64", "x86_64") else ""
    system = "windows" if sys.platform == "win32" else "linux" if sys.platform.startswith("linux") else ""
    return platform == f"{system}-{arch}"


def start_check(platform: str, archive: Path) -> None:
    """A fresh extraction, given the firmware, starts the C64 to READY."""
    roms = sorted(p for p in (ROOT / "roms").glob("*") if p.is_file())
    if not roms:
        raise PackageError("roms/ holds no firmware for the start check: python tools/fetch_roms.py (or --no-start)")
    work = ROOT / "work" / "package" / "start"
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)
    if platform.startswith("windows"):
        with zipfile.ZipFile(archive) as z:
            z.extractall(work)
        folder = work
    else:
        with tarfile.open(archive, "r:gz") as t:
            t.extractall(work, filter="data") if sys.version_info >= (3, 12) else t.extractall(work)
        folder = work / archive.name[:-len(".tar.gz")]
    if not (folder / "roms").is_dir():
        raise PackageError("the archive has no roms/ folder")
    for rom in roms:
        shutil.copy(rom, folder / "roms" / rom.name)
    script = work / "start.script"
    script.write_text("until 20 READY.\nexpect READY.\n", encoding="ascii")
    run = subprocess.run([str(folder / program_name(platform)), "--hidden", "--script", str(script)], cwd=work,
                         capture_output=True, text=True, timeout=120)
    if run.returncode:
        raise PackageError(f"the extracted program did not start to READY: {run.stderr.strip() or run.stdout.strip()}")
    shutil.rmtree(work, ignore_errors=True)


def package(platform: str, binary: Path, out: Path, start: bool = True, *, libretro: bool = False) -> Path:
    entries = gather(platform, binary, libretro)
    expected = expected_files(platform, libretro)
    problems = broken_links(entries, () if libretro else FOLDERS)
    if problems:
        raise PackageError("links that lead nowhere in the archive:\n  " + "\n  ".join(problems))
    out.mkdir(parents=True, exist_ok=True)
    final = out / archive_name(platform, libretro)
    work = ROOT / "work" / "package"
    work.mkdir(parents=True, exist_ok=True)
    temp = work / final.name
    write_archive(platform, entries, temp, libretro)
    back = read_back(platform, temp, libretro)
    if sorted(back) != expected:
        raise PackageError(f"the archive holds {sorted(set(back) ^ set(expected))} beyond or short of the list")
    for name, path in entries.items():
        if back[name] != path.read_bytes():
            raise PackageError(f"the archive's {name} differs from the staged one")
    smoke_test = start and not libretro and runnable_here(platform)
    if smoke_test:
        start_check(platform, temp)
    archive = re.compile(rf"{NAME}(?:-Libretro)?-(\d+\.\d+\.\d+)-[a-z]+-[a-z0-9]+\.(zip|tar\.gz)")
    for old in out.glob(f"{NAME}-*"):
        m = archive.fullmatch(old.name)
        if m and m.group(1) != version():
            old.unlink(missing_ok=True)
    for old in out.parent.glob(f"{NAME}-*-{platform}.*"):    # archives of the earlier layout, in dist/ itself
        if archive.fullmatch(old.name):
            old.unlink(missing_ok=True)
    shutil.move(str(temp), final)
    print(f"OK: {final} ({final.stat().st_size:,} bytes)" + (", starts to READY" if smoke_test else ""))
    return final


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("platform", choices=PLATFORMS)
    parser.add_argument("binary", nargs="?", type=Path)
    parser.add_argument("--libretro", action="store_true", help="package the shared core")
    parser.add_argument("--out", type=Path, default=ROOT / "dist" / "releases")
    parser.add_argument("--no-start", action="store_true", help="skip the start check of a fresh extraction")
    parser.add_argument("--list", action="store_true", help="print the files of the archive and stop")
    args = parser.parse_args(argv)
    if args.list:
        empty_folders = () if args.libretro else FOLDERS
        print("\n".join([*expected_files(args.platform, args.libretro), *(f + "/" for f in empty_folders)]))
        return 0
    if not args.binary:
        parser.error("a built executable or library is required")
    try:
        package(args.platform, args.binary.resolve(), args.out.resolve(), not args.no_start, libretro=args.libretro)
    except (PackageError, OSError) as e:
        print(f"package: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
