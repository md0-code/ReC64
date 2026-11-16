# ReC64

A Commodore 64 emulator for Windows and Linux, written in C11.
Runs the original BASIC and KERNAL firmware on PAL and NTSC C64/C64C models.

Build on Windows with Visual Studio's C++ tools, CMake 3.24 or later and Ninja, plus Perl (included with Git for Windows):

```powershell
./src/build.ps1
./src/build.ps1 -Arch arm64
python tools/fetch_roms.py
./work/build-x64/rec64.exe --model c64-pal
```

SDL 2 is downloaded during configuration. Windows ARM64 requires the ARM64
compiler tools. Linux builds use `cmake -S src -B work/build` followed by
`cmake --build work/build`.

For headless use:

```powershell
./work/build-x64/rec64.exe --model c64-pal --hidden --frames 200 --screenshot work/test.png
python tools/verify.py
```

Use `--list-models` to see the model identifiers. The `test` model is a
diagnostic screen that runs without firmware. The optional configuration is
`rec64.yaml`; an example is in `src/app/`.
See [configuration and control](docs/CONFIGURATION.md) and
[component notices](docs/THIRD_PARTY.md). Licensed under
[GPL-3.0-or-later](docs/LICENSE). Firmware retains its owners' rights.
