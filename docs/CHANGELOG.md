# Changelog

The changes users notice, newest first.

## 0.23.6 (2026-11-23)

- Separate downloads for the desktop emulator and Libretro core on Windows and Linux.

## 0.23.5 (2026-11-23)

- Libretro core in the Windows and Linux x64 and ARM64 archives.

## 0.23.4 (2026-11-21)

- The controller's Y button finds a file by name in the Load browser.

## 0.23.3 (2026-11-21)

- rec64.exe and its window show the ReC64 keys logo as their icon.

## 0.23.2 (2026-11-21)

- The controller's Y button opens the on-screen keyboard.

## 0.23.1 (2026-11-21)

- Firmware, configuration and limitations are in docs/NOTES.md; the design notes are no longer shipped.

## 0.23.0 (2026-11-21)

- Release archives for Windows and Linux; UTF-8 file names on Windows; a file given on the command line opens as a drop.

## 0.22.6 (2026-11-21)

- The minimal border is 5 pixels; the former 16-pixel minimal border becomes medium.

## 0.22.5 (2026-11-21)

- The keyboard joystick drives the other port when its own holds a mouse or paddles.

## 0.22.4 (2026-11-21)

- T64 programs load beneath the I/O area, tape auto-load relocates BASIC files, TAP headers with a wrong length are accepted.

## 0.22.3 (2026-11-21)

- Auto-load runs programs that load below $0801 and finds READY above the cursor.

## 0.22.2 (2026-11-21)

- Exact drive/C64 port timing, media controls that show only playback actions, a persistent status line.

## 0.22.1 (2026-11-18)

- The load list opens on the file loaded last.

## 0.22.0 (2026-11-18)

- About twice as fast at full speed.

## 0.21.5 (2026-11-18)

- A pending auto-load survives the snapshot slot picker.

## 0.21.4 (2026-11-18)

- Both palettes are computed from the VIC-II's luma steps and chroma angles.

## 0.21.3 (2026-11-18)

- The second trial palette is the tuned one with more saturation.

## 0.21.2 (2026-11-18)

- The second trial palette is a darker copy of the tuned one.

## 0.21.1 (2026-11-18)

- Firmware is recognised by content whatever its file name; a stronger tuned palette.

## 0.21.0 (2026-11-18)

- A screen names the missing firmware; a tuned palette.

## 0.20.6 (2026-11-18)

- The firmware checksums are those of the original Commodore archives; the 1541 image is joined from its two halves.

## 0.20.5 (2026-11-18)

- The documentation is one README.

## 0.20.4 (2026-11-18)

- One control for the loading sounds; menus skip disabled choices.

## 0.20.3 (2026-11-18)

- Menus and file name entry work from a controller.

## 0.20.2 (2026-11-18)

- The 1541 start-up self-test sound is back.

## 0.20.1 (2026-11-18)

- Loading sounds match the medium; a smaller About logo.

## 0.20.0 (2026-11-18)

- Refined setup pages and saving of BASIC programs to media.

## 0.19.13 (2026-11-18)

- The ReC64 Datasette logo.

## 0.19.12 (2026-11-18)

- The drive's motor sounds are cut clear of head noise.

## 0.19.11 (2026-11-18)

- The 1541's sounds are played from recordings of the drive.

## 0.19.10 (2026-11-17)

- Drive sounds rebuilt from struck resonances and motor lines.

## 0.19.9 (2026-11-17)

- RGCD and GMod2 cartridges, with persistent EEPROM saves.

## 0.19.8 (2026-11-17)

- Documentation only: an offline ROM comparison page (removed in 0.20.5).

## 0.19.7 (2026-11-17)

- The drive type is chosen in the Machine settings.

## 0.19.6 (2026-11-17)

- Documentation only: a ROM compatibility report (removed in 0.20.5).

## 0.19.5 (2026-11-17)

- G64 disks and a selectable 1541-II drive.

## 0.19.4 (2026-11-17)

- No volume transients when a SID tune starts.

## 0.19.3 (2026-11-17)

- The sound queue stays short after automatic full speed; SID tunes start at full volume.

## 0.19.2 (2026-11-17)

- SID files without clock or chip hints play on a PAL C64 with a 6581.

## 0.19.1 (2026-11-17)

- Settings alignment, SID choices and cartridge placement corrected.

## 0.19.0 (2026-11-17)

- Media controls and settings are separate; controller actions are unified.

## 0.18.2 (2026-11-17)

- No waiting for vsync on a display slower than the machine.

## 0.18.1 (2026-11-17)

- Four times faster at full speed, every frame shown once, fast mouse movement no longer wraps.

## 0.18.0 (2026-11-17)

- Startup cartridges and utility cartridge hardware.

## 0.17.2 (2026-11-17)

- Characters show at every XSCROLL setting.

## 0.17.1 (2026-11-17)

- Only completed VIC-II frames are shown.

## 0.17.0 (2026-11-17)

- ReC64's own SID is the only engine; rebuilt drive sounds; the SID player returns to the file list.

## 0.16.2 (2026-11-17)

- The built-in SID reworked: exact digital core, calibrated analogue path and resampler.

## 0.16.1 (2026-11-17)

- An opt-in experimental SID engine; snapshots keep their engine.

## 0.16.0 (2026-11-17)

- Reported interface, media and input problems fixed.

## 0.15.0 (2026-11-17)

- No change for users: the SID evaluation builds are kept apart from the release.

## 0.14.0 (2026-11-16)

- SID player, live panel and keyboard host controls.

## 0.13.0 (2026-11-16)

- Cartridges and the expansion bus, flash cartridges included.

## 0.12.0 (2026-11-16)

- Snapshot slots with previews and safe restore.

## 0.11.0 (2026-11-16)

- 1541 mechanism sounds.

## 0.10.1 (2026-11-16)

- Automatic fast loading while the drive is busy.

## 0.10.0 (2026-11-16)

- True 1541 drive on the serial bus.

## 0.9.0 (2026-11-16)

- Datasette playback, recording and loading sound.

## 0.8.0 (2026-11-16)

- KERNAL loading and saving, auto-load.

## 0.7.0 (2026-11-16)

- File browser, image contents and save page.

## 0.6.2 (2026-11-16)

- Starts on Windows without a console; fullscreen keys work before setup.

## 0.6.1 (2026-11-16)

- Interface digits and punctuation match the bold alphabet.

## 0.6.0 (2026-11-16)

- Symbolic C64 keyboard input, controllers, 1351 mouse and paddles.

## 0.5.0 (2026-11-16)

- Setup screen, display palettes and host controls.

## 0.4.2 (2026-11-16)

- C64 boards; the Lorenz CPU, CIA and memory tests pass.

## 0.4.1 (2026-11-16)

- VIA and GCR, with chip and media snapshots.

## 0.4.0 (2026-11-16)

- Disk, tape and program containers.

## 0.3.2 (2026-11-16)

- VIC-II bus phases, graphics sequencers and state.

## 0.3.1 (2026-11-16)

- SID sound through reSID, with resampling and exact audio state.

## 0.3.0 (2026-11-16)

- CIA timers, PLA decoding and the C64 keyboard matrix.

## 0.2.1 (2026-11-16)

- Cycle-stepped 6510 and 6502 processors.

## 0.2.0 (2026-11-16)

- The portable foundation, diagnostic board and headless control.
