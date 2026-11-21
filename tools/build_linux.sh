#!/usr/bin/env bash
# Build, test and package ReC64 for Linux: linux-x64 (native) and linux-arm64 (cross-compiled).
#
#   tools/build_linux.sh [x64|arm64|all] [--no-test] [--no-package] [--no-pgo] [--keep]
#   tools/build_linux.sh sysroot          (as root, once before an arm64 build: the arm64 root for the cross build)
#   tools/build_linux.sh clean            (removes everything the script made under /tmp/rec64-build)
#
# Written for and tried in WSL Debian 12, run from the repository, which may be on a Windows drive (/mnt/c/...).
# The sources and the git-ignored roms/ are copied to /tmp/rec64-build/repo, where the build trees and the test
# output are too; only the archives go to the repository's dist/releases/. Once the targets are built the build trees and
# the copy are removed (--keep leaves them); `clean` also removes the arm64 root.
#
#   x64    configure and build (Release, SDL 2 built from source and linked statically), profile-guided when roms/
#          holds the firmware (tools/pgo_train.py, as tools/build_dist.ps1 does for Windows; --no-pgo skips it),
#          ctest on SDL's dummy drivers, and dist/releases/ReC64-<version>-linux-x64.tar.gz through tools/package.py, which
#          also checks that a fresh extraction starts the C64 to READY.
#   arm64  the same build with aarch64-linux-gnu-gcc against an arm64 root (Debian 12's packages, which `sysroot`
#          unpacks into /tmp/rec64-build/sysroot-arm64); when qemu-aarch64 is installed the program's version is
#          checked through it. dist/releases/ReC64-<version>-linux-arm64.tar.gz.
#
# Packages (Debian, root): build-essential cmake ninja-build pkg-config python3 rsync libasound2-dev libpulse-dev
# libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxkbcommon-dev
# libdrm-dev libgbm-dev libgl-dev libgles-dev libegl-dev libdbus-1-dev libudev-dev libwayland-dev wayland-protocols
# (SDL 2's build dependencies; at run time SDL loads X11, Wayland, ALSA, PulseAudio and GL when present, so the
# program links only the C library); for arm64 also gcc-aarch64-linux-gnu and debootstrap.
set -euo pipefail

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
root=$(dirname "$here")
version=$(head -n 1 "$root/src/VERSION" | tr -d '[:space:]')
[[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "src/VERSION must hold MAJOR.MINOR.PATCH, not '$version'" >&2; exit 1; }

base=/tmp/rec64-build
tree=$base/repo
sysroot=$base/sysroot-arm64
test=1 package=1 pgo=1 keep=0
targets=()
while [[ $# -gt 0 ]]; do
    case $1 in
        x64|arm64|sysroot|clean) targets+=("$1") ;;
        all) targets+=(x64 arm64) ;;
        --no-test) test=0 ;;
        --no-package) package=0 ;;
        --no-pgo) pgo=0 ;;
        --keep) keep=1 ;;
        -h|--help) awk 'NR>1 && /^#/ {sub(/^# ?/, ""); print; next} NR>1 {exit}' "${BASH_SOURCE[0]}"; exit 0 ;;
        *) echo "unknown argument: $1 (tools/build_linux.sh --help)" >&2; exit 2 ;;
    esac
    shift
done
[[ ${#targets[@]} -gt 0 ]] || targets=(x64)

sysroot_packages=(libc6-dev linux-libc-dev libasound2-dev libpulse-dev libx11-dev libxext-dev libxrandr-dev
    libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxkbcommon-dev libdrm-dev libgbm-dev libgl-dev libgles-dev
    libegl-dev libdbus-1-dev libudev-dev libwayland-dev wayland-protocols)

make_sysroot() {
    [[ $EUID -eq 0 ]] || { echo "making the sysroot needs root: sudo tools/build_linux.sh sysroot" >&2; exit 1; }
    command -v debootstrap >/dev/null || { echo "debootstrap is not installed (apt-get install debootstrap)" >&2; exit 1; }
    rm -rf "$sysroot"
    mkdir -p "$sysroot"
    debootstrap --foreign --variant=minbase --arch=arm64 --include="$(IFS=,; echo "${sysroot_packages[*]}")" \
        bookworm "$sysroot" http://deb.debian.org/debian
    # The first stage unpacks only the base packages and downloads the others: unpack those too.
    local deb
    for deb in "$sysroot"/var/cache/apt/archives/*.deb; do dpkg-deb -x "$deb" "$sysroot"; done
    rm -rf "$sysroot"/var/cache/apt/archives/*.deb
    # The build does not run as root: whoever ran sudo, or else the repository's owner, owns the root.
    local owner="${SUDO_UID:-}:${SUDO_GID:-}"
    [[ $owner != ":" ]] || owner=$(stat -c %u:%g "$root")
    chown -R "$owner" "$base"
    chmod -R u+rwX "$sysroot"
    echo "sysroot: $sysroot"
}

sync_sources() {
    mkdir -p "$tree"
    rsync -a --delete --exclude /.git --exclude /work --exclude /dist --exclude /build --exclude /rec64.yaml \
        --exclude /states "$root/" "$tree/"
    mkdir -p "$tree/work"
}

build_target() {
    local arch=$1 dir="$base/$1"
    local args=(-S "$tree/src" -G Ninja -DCMAKE_BUILD_TYPE=Release)
    if [[ $arch == arm64 ]]; then
        command -v aarch64-linux-gnu-gcc >/dev/null || { echo "aarch64-linux-gnu-gcc is not installed (apt-get install gcc-aarch64-linux-gnu)" >&2; exit 1; }
        [[ -f $sysroot/usr/include/X11/Xlib.h ]] || { echo "no arm64 sysroot at $sysroot: sudo tools/build_linux.sh sysroot" >&2; exit 1; }
        mkdir -p "$dir"
        cat > "$base/toolchain-arm64.cmake" <<EOF
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_SYSROOT "$sysroot")
set(CMAKE_FIND_ROOT_PATH "$sysroot")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOF
        args+=(-DCMAKE_TOOLCHAIN_FILE="$base/toolchain-arm64.cmake")
        export PKG_CONFIG_SYSROOT_DIR="$sysroot"
        export PKG_CONFIG_LIBDIR="$sysroot/usr/lib/aarch64-linux-gnu/pkgconfig:$sysroot/usr/lib/pkgconfig:$sysroot/usr/share/pkgconfig"
        unset PKG_CONFIG_PATH
    fi
    echo "== linux-$arch: $dir"
    local trained=0
    if [[ $arch == x64 && $pgo -eq 1 ]] && python3 "$tree/tools/fetch_roms.py" --check >/dev/null 2>&1; then
        rm -f "$dir"/*.gcda
        find "$dir" -name '*.gcda' -delete 2>/dev/null || true
        cmake "${args[@]}" -B "$dir" -DRC_PGO=generate
        cmake --build "$dir" --target rec64
        python3 "$tree/tools/pgo_train.py" "$dir/rec64"
        cmake "${args[@]}" -B "$dir" -DRC_PGO=use
        trained=1
    else
        cmake "${args[@]}" -B "$dir" -DRC_PGO=
    fi
    cmake --build "$dir"
    [[ $trained -eq 0 ]] || echo "profile-guided build"
    if [[ $test -eq 1 ]]; then
        if [[ $arch == x64 ]]; then
            SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --test-dir "$dir" --output-on-failure -j "$(nproc)"
        elif command -v qemu-aarch64 >/dev/null; then
            [[ $(qemu-aarch64 -L "$sysroot" "$dir/rec64" --version) == "$version" ]] || { echo "the arm64 program does not run under qemu-aarch64" >&2; exit 1; }
            echo "arm64 program runs under qemu-aarch64"
        else
            echo "qemu-aarch64 is not installed: the arm64 program is built but not run"
        fi
    fi
    if [[ $package -eq 1 ]]; then
        local start=()
        [[ $test -eq 1 ]] || start=(--no-start)
        python3 "$tree/tools/package.py" "linux-$arch" "$dir/rec64" --out "$root/dist/releases" "${start[@]}"
    fi
}

built=0
for t in "${targets[@]}"; do
    case $t in
        sysroot) make_sysroot ;;
        clean) rm -rf "$base"; echo "removed $base" ;;
        *) [[ $built -eq 1 ]] || sync_sources; built=1; build_target "$t" ;;
    esac
done
if [[ $built -eq 1 && $keep -eq 0 ]]; then
    rm -rf "$tree" "$base/x64" "$base/arm64" "$base/toolchain-arm64.cmake"
    echo "build trees removed (the arm64 root stays until: tools/build_linux.sh clean)"
fi
