# ReC64

ReC64 is an original, portable Commodore 64 emulator. The CPU, VIC-II, CIAs,
SID, 1541 drive and Datasette are its own implementations, written from the
hardware documentation and emulated cycle by cycle, and they run the
unmodified Commodore firmware. It is written in C11 and runs on Windows and
Linux, on x64 and ARM64.

- PAL and NTSC C64 and C64C, with a 6581 or 8580 SID
- True 1541 and 1541-II drive (D64, G64) and Datasette (TAP), both reading
  and writing, with automatic fast loading and optional loading sounds
- PRG, P00 and T64 programs, CRT cartridges with freezers and flash
  cartridges, REU and GeoRAM
- Snapshots, a SID music player, a CRT display filter
- Joysticks, 1351 mouse and paddles; every menu works from a game controller
- On-screen C64 keyboard for touch, mouse and controllers

## Getting started

Supply the [firmware](#firmware), then run `rec64.exe`. A PAL C64 starts at
the BASIC prompt. Press **Ctrl+F1** to
load a program, disk, tape or cartridge, or drop a file on the window.
**F12** opens setup, where every setting can be changed.

## Firmware

ReC64 contains no Commodore firmware. Copy the ROM images below into the
`roms` folder beside the program; until they are there, ReC64 shows which
ones are missing. File names do not matter: each image is recognised by its
checksum. BASIC, KERNAL and the character generator are required, and at
least one of the two drive ROMs.

| ROM | What it is | Bytes | SHA-256 |
| --- | --- | --- | --- |
| BASIC 901226-01 | The BASIC interpreter | 8192 | `89878cea0a268734696de11c4bae593eaaa506465d2029d619c0e0cbccdfa62d` |
| KERNAL 901227-03 | The operating system, revision 3 | 8192 | `83c60d47047d7beab8e5b7bf6f67f80daa088b7a6a27de0d7e016f6484042721` |
| Characters 901225-01 | The screen font | 4096 | `fd0d53b8480e86163ac98998976c72cc58d5dd8eb824ed7b829774e74213b420` |
| 1541 DOS 325302-01 + 901229-05 | Firmware of the 1541 disk drive | 16384 | `d1d45afb46fd4e2b48d93ca367b889d75654a3b7acf73044e51f6d880c09369e` |
| 1541-II DOS 251968-03 | Firmware of the 1541-II disk drive | 16384 | `326c289c38753323d7e8167897447cf61ef35189d82eb8d75210ece949adda7c` |

The 1541 image is one file: the 325302-01 ROM followed by the 901229-05 ROM.
Other revisions and replacement ROMs such as JiffyDOS are not accepted.

## Keys

| Key | Action |
| --- | --- |
| F12 | Setup |
| Ctrl+F1 / Ctrl+F2 | Load / save |
| Ctrl+F3 / Ctrl+F4 | Drive controls / tape controls |
| Ctrl+F5 / Ctrl+F6 | Save / restore the current snapshot slot |
| F9 / Shift+F9 | Status line / on-screen keyboard |
| F10 | Normal or full speed (no sound at full speed) |
| F11 / Shift+F11 | Keyboard joystick on or off / swap the joystick ports |
| Alt+F12 / Ctrl+F12 / Shift+F12 | Reset / cold boot / exit |
| Alt+F / Alt+S / Alt+T | Drive fast loading / drive sounds / tape sound |
| Alt+Enter | Full screen |
| Pause / Print Screen | Pause / screenshot into `work/screenshots/` |
| Right Alt, pressed alone | Freeze button of the cartridge |

Typing follows your host keyboard layout: each character presses the C64
keys that produce it. F1 to F8 are the C64 function keys. Esc is RUN/STOP,
Page Up RESTORE, Tab CTRL, Page Down the Commodore key, Home CLR/HOME and
Backspace INST/DEL.

In setup, Left/Right or Tab changes the page, Up/Down selects a row, Enter
changes its value and Esc closes. Changing the model, video standard or SID
restarts the machine.

## Loading and saving

Load (Ctrl+F1, or drag and drop) is one browser for every file type:

| File | Result |
| --- | --- |
| PRG, P00 | Loaded and run |
| D64, T64, TAP | Lists the contents; choose a program to load it |
| G64 | Inserted in the drive and loaded |
| CRT | Cartridge attached, machine restarted |
| SID | Opens the music player |
| RCS | Snapshot restored |

Typing a name searches the list, Left or Backspace goes up a folder, and
Space inserts a disk or tape without loading it, for disk swaps. With
auto-load on (Media page), ReC64 types the `LOAD` and `RUN` commands for you.
Apart from a cartridge set to **Start inserted**, inserted media last for the
session.

Save (Ctrl+F2) writes the BASIC program in memory as a PRG file, a new TAP or
a new D64. Tab moves between the file list, format and name; F2 renames a
file and Delete removes it. With a disk inserted, `SAVE"NAME",8` writes into
that image. Changed images are written back on eject and exit.

Disks always run through the emulated drive and its own firmware, so custom
loaders work. With both drive ROMs present the Machine page selects the 1541
or 1541-II; the Media page holds the device number (8 to 11), fast loading,
loading sounds and the default media folder. Fast loading runs the machine at full speed while the
drive or tape is busy, without changing what the C64 sees; loading sounds
play only with fast loading off. Ctrl+F3 and Ctrl+F4 open the drive and tape
controls: load, save, eject, and for tape also play, stop, rewind and record.

## Cartridges and memory expansions

Supported cartridges: normal 8K and 16K, Ultimax, Ocean, Magic Desk, Fun
Play, Super Games, Dinamic, Zaxxon, System 3, Simons' BASIC, Epyx Fastload,
EasyFlash, GMod2, RGCD, Action Replay 4 to 6, Retro Replay, Final Cartridge
I/II and III, KCS Power Cartridge and Super Snapshot 5.

- Attaching or ejecting a cartridge resets the machine. **Start inserted** on
  the Cartridge page keeps it inserted for the next launch.
- Right Alt pressed and released on its own is the freeze button; AltGr
  combinations still type. The Cartridge page has a Freeze row as well.
- EasyFlash and GMod2 flash writes are saved into the CRT file unless it is
  write protected. GMod2 keeps its game progress in `<cartridge>.crt.eeprom`.
- Final Cartridge III starts its desktop; hold RUN/STOP at startup for BASIC.
- The Cartridge page also selects a REU (128 KiB to 16 MiB) or a GeoRAM
  (512 KiB to 4 MiB), one at a time.

## Snapshots

Ctrl+F5 saves and Ctrl+F6 restores the current slot; the first use in a
session asks which slot. The States page shows ten slots per model with a
preview and save time, and deletes them. Snapshots are stored as
`states/<model>/slot-N.rcs` beside `rec64.yaml` and also open through Load.

A snapshot resumes disk and tape positions and unfinished loads. The media it
refers to must still exist unchanged, and a snapshot of another model or ROM
is refused.

## Controllers

Controllers are detected when plugged in; the first one starts on port 2. The
keyboard joystick (cursor keys and Left Ctrl) switches off while a controller
is connected. Each port can be a joystick, a 1351 mouse, paddles or nothing
(Controllers page); F12 releases the captured mouse.

| Input | Action |
| --- | --- |
| D-pad / left stick | Joystick; menu navigation |
| A / B | Fire, accept / open setup, go back |
| X | Open Load; in a browser, insert the image or use the current folder; rename in Save |
| Y | Media controls; delete a file in Save or a snapshot in States |
| LB / RB | Swap ports / full speed; page the Load list; switch fields in Save |
| LT / RT | Quick save / quick load |
| Start | Setup; held with Y cold boot, with X exit, with R3 screenshot |
| Back | On-screen keyboard; find by name in a browser |
| L3 / R3 | Pause / status line |

In a name field, Up/Down cycles the character and Left/Right moves the cursor.

## SID player

Opening a SID file (PSID or RSID, single SID) starts the player, with title,
author, release and elapsed time. Left/Right changes the sub-tune, Space
pauses and Esc returns to the file list. The clock and SID chip follow the
file, or a PAL 6581 when it names none. The SID is ReC64's own engine,
described in the [design notes](docs/DESIGN.md).

## Status line and on-screen keyboard

F9 shows one line such as `D8:18.5*M T:001234PM J:2`: drive device and track
with LED (`*`) and motor (`M`), the tape counter with play, stop or record
(`P`, `S`, `R`), and the keyboard joystick port.

Shift+F9, or Back on a controller, opens the C64 keyboard for mouse, touch
(several fingers at once) and controllers. Tap Shift, C= or Ctrl to latch it.

## Configuration

Settings are saved in `rec64.yaml` beside the program. Setup writes it and
keeps your comments; `--config FILE` selects another file, and
`src/app/rec64.yaml` is an example. Unknown keys and invalid values are
rejected with a line number.

| Setting | Values |
| --- | --- |
| `machine.model` | `c64-pal`, `c64c-pal`, `c64-ntsc`, `c64c-ntsc` (also `--model`) |
| `machine.options.sid` | `6581`, `8580` |
| `machine.options.drive_model`, `drive_device` | `1541`, `1541-ii`; 8 to 11 |
| `machine.options.reu` | `off`, `128k`, `256k`, `512k`, `1m`, `2m`, `4m`, `8m`, `16m` |
| `machine.options.georam` | `off`, `512k`, `1m`, `2m`, `4m` |
| `video.look`, `video.filter` | `crt`, `plain`; `nearest`, `linear` |
| `video.border` | `full`, `minimal`, `none` |
| `video.palette` | `default` (follows PAL or NTSC), `color-corrected`, `tuned`, `tuned-dark` |
| `media.cartridge.startup` | `true` keeps the cartridge between launches |
| `ui.media_folder` | Folder the first file browser opens |

C64 keys and the keyboard joystick can be rebound with SDL key names; F9 to
F12, Pause and Print Screen stay reserved:

```yaml
keyboard:
  RUN STOP: Escape
  RESTORE: PageUp
controllers:
  joystick2: {type: joystick, device: controller 1}
  keyboard_port: 2
  keys: {up: Up, down: Down, left: Left, right: Right, fire: Left Ctrl, fire2: Left Shift}
```

## Building

Windows needs Visual Studio's C++ tools, CMake 3.24 or later and Ninja:

```powershell
./src/build.ps1                # x64, into work/build-x64
./src/build.ps1 -Arch arm64    # needs the ARM64 compiler tools
python tools/fetch_roms.py     # firmware for the checks, into roms/
./work/build-x64/rec64.exe
./tools/build_dist.ps1         # portable build in dist/, ARM64 in dist/arm64/
```

On Linux:

```sh
cmake -S src -B work/build && cmake --build work/build
```

SDL 2 is downloaded during configuration. `python tools/verify.py` runs the
checks without opening a window. The [design notes](docs/DESIGN.md) describe
the architecture, the snapshot format and the scripting interface.

## License

ReC64 is licensed under [GPL-3.0-or-later](docs/LICENSE); see the
[component notices](docs/THIRD_PARTY.md). The Commodore firmware remains the
property of its owners.
