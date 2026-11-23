# Notes

What the [README](../README.md) leaves out: the [firmware](#firmware) ReC64 needs, the
[configuration](#configuration) file and the [limitations](#limitations).

## Libretro

Copy the library from `cores/` and its matching file from `info/` into
RetroArch's Cores and Core Info directories. Use the archive that matches the
operating system and architecture of RetroArch. Put the [firmware](#firmware)
in `rec64/` inside the frontend's System/BIOS directory.

Start the core on its own for the BASIC prompt, or use Load Content for
PRG, P00, T64, TAP, D64, G64, CRT or SID files. Images opened through Libretro
are write-protected. Core Options selects the model, SID, border and automatic
loading for the next time content or the core is opened. Save states through
the frontend.

Enable RetroArch's Game Focus to send keyboard shortcuts to the C64 keyboard.
The first RetroPad uses C64 control port 2 and the second uses port 1. B is
fire, and L and R on the first RetroPad play and stop a mounted tape.

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
| `video.border` | `full`, `medium` (16 pixels), `minimal` (5 pixels), `none` |
| `video.palette` | `default`, or `color-corrected` for a truer blue and more saturation |
| `media.cartridge.startup` | `true` keeps the cartridge between launches |
| `ui.media_folder` | Folder the first file browser opens |
| `ui.status_line` | `true` shows the status line |

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

## Limitations

The SID is ReC64's own engine, and two parts of it are approximations.
Combined waveforms come from a model, not from sampled chips, so their OSC3
readback and the 6581's waveform feedback differ on part of the cycles. The
6581 filter only approximates how the cutoff moves with signal level: a
single quiet filtered voice and the highest resonance settings deviate most.
