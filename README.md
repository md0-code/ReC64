# ReC64

A Commodore 64 emulator for Windows and Linux, written in C11.
The current build provides a diagnostic test screen; it does not yet execute
C64 programs.

Build on Windows with Visual Studio's C++ tools, CMake 3.24 or later and Ninja:

```powershell
./src/build.ps1
./src/build.ps1 -Arch arm64
./work/build-x64/rec64.exe
```

SDL 2 is downloaded during configuration. Windows ARM64 requires the ARM64
compiler tools. Linux builds use `cmake -S src -B work/build` followed by
`cmake --build work/build`.

For headless use:

```powershell
./work/build-x64/rec64.exe --hidden --frames 2 --screenshot work/test.png
python tools/verify.py
```

The optional configuration is `rec64.yaml`; an example is in `src/app/`.
See [configuration and control](docs/CONFIGURATION.md) and
[component notices](docs/THIRD_PARTY.md). Licensed under
[GPL-3.0-or-later](docs/LICENSE). Firmware retains its owners' rights.
