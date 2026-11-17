# ReC64

A Commodore 64 emulator for Windows and Linux, written in C11.
Runs the original BASIC and KERNAL firmware on PAL and NTSC C64/C64C models.

Build on Windows with Visual Studio's C++ tools, CMake 3.24 or later and Ninja:

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
F12 opens setup; Alt+F12 resets and Ctrl+F12 cold boots. F10 switches speed,
Pause stops/resumes, and Alt+Enter switches full screen. F11 toggles the
keyboard joystick; Shift+F11 swaps ports. Controllers, the 1351 mouse and
paddles are configured on the Controllers page. Keyboard text follows your
host layout.
SID selection offers 6581 (the default) or 8580.
See [ROM compatibility](docs/C64-ROM-AUDIT.md) for supported firmware,
cartridge families and their purposes compared with VICE.

Ctrl+F1 opens Load for PRG/P00 files and D64/T64/TAP contents; dropped files
use the same browser. Ctrl+F2 saves a PRG, creates a TAP recording target,
or makes a blank D64. G64 images load directly and preserve raw tracks and
half-tracks. Both disk formats support disk SAVE through the emulated drive.
The Machine page selects the drive type (1541 or 1541-II). The Media page holds
device number, fast loading and loading sounds.
Ctrl+F3/F4 opens the media control OSD. Alt+F switches drive fast loading, Alt+S drive sounds,
and Alt+T tape sound.

Ctrl+F5 saves the current snapshot slot; Ctrl+F6 restores it. The States
page selects among ten slots per model and shows a preview of each save.
The first quick save/load in a session opens a slot picker.
Snapshot files (`.rcs`) also open through Ctrl+F1 or drag and drop.

Load CRT cartridges through Ctrl+F1, drag and drop, or the Cartridge page.
The Cartridge settings page selects REU or GeoRAM memory. Right Alt pressed and released
alone activates supported freezer cartridges. EasyFlash writes are saved
to the attached CRT image unless it is write protected.
Turn on **Start inserted** on the Cartridge page to keep the attached
cartridge inserted when ReC64 next starts.

Controller d-pad/left stick moves and A fires. B opens F12, X opens Load,
Y opens controls, LB swaps ports, RB toggles full speed, and LT/RT quick
save/load. Connecting a controller disables the keyboard joystick;
disconnecting the last controller enables it again.

SID files open in the music player with title, author, release, elapsed time
and sub-tune selection. Left/Right changes tune, Space pauses, and Escape
stops playback and returns to the file list. Clock and SID chip follow the
file's hints (PAL and 6581 without them). The SID is ReC64's own; see [its limitations](docs/LIMITATIONS.md).

F9 toggles live drive, tape and joystick status. Shift+F9 opens the C64
on-screen keyboard for mouse, multi-touch or controller input; controller
Back also toggles it. Shift, C= and Ctrl can be latched with a tap.

Create a portable Windows build in `dist/` with `./tools/build_dist.ps1`.
Launch `dist/rec64.exe`; the native ARM64 copy is `dist/arm64/rec64.exe`.
See [configuration and control](docs/CONFIGURATION.md) and
[component notices](docs/THIRD_PARTY.md). Licensed under
[GPL-3.0-or-later](docs/LICENSE). Firmware retains its owners' rights.
