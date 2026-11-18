# Configuration and control

`--config FILE` reads a YAML configuration. Unknown keys, duplicate keys and
invalid values are rejected with a line number. Paths inside it are relative
to the configuration file. `src/app/rec64.yaml` is an example. Models are
`c64-pal` (default), `c64c-pal`, `c64-ntsc`, `c64c-ntsc`, and the firmware-free
diagnostic model `test`. Run `python tools/fetch_roms.py` to obtain the pinned
firmware. `machine.options.sid` selects `6581` (the default) or `8580`.
Older configuration files using `default` are updated to `6581`.
Firmware discovery recognizes five exact ROM images: stock BASIC, KERNAL
revision 3, the standard character generator, and the 1541/1541-II drive ROMs.
Supported files can be renamed because identification uses SHA-256. Different
revisions, localized ROMs and JiffyDOS are not accepted by copying or renaming
them. See the [ROM compatibility comparison](C64-ROM-AUDIT.md) for the full
catalogue, purposes and differences from VICE.
Snapshots saved by the releases that used the
reSID engine still load: registers are restored and notes start again.

The Cartridge settings page loads/ejects CRT images, selects expansion
memory and startup insertion, and provides the cartridge Freeze action.
Supported cartridges are Normal 8K/16K, Ultimax, Ocean, Magic Desk, Fun Play,
Super Games, Dinamic, Zaxxon, System 3, Simons' BASIC, EasyFlash, Action
Replay 4-6, Retro Replay, Final Cartridge I/II and III, Epyx Fastload,
KCS Power Cartridge, Super Snapshot 5, RGCD and GMod2. Attaching or ejecting a
cartridge resets the machine. Right Alt pressed and released alone triggers
the physical freeze button; using AltGr with another key does not freeze.
The page's Freeze row performs the same action. EasyFlash retains completed
flash writes in the attached CRT, including banks omitted from a sparse
image; write protection inhibits programming.
GMod2 retains completed flash writes in its CRT too. Its 2 KiB EEPROM is
loaded from and saved to the cartridge filename with `.eeprom` appended
(for example, `game.crt.eeprom`). A missing file starts erased and is created
only after the contents change. The EEPROM file stores 1024 words, high byte
first. It is saved when media are flushed, including on eject, restart and
normal exit; its folder must be writable. Snapshots include the EEPROM and
any unfinished serial command or write. RGCD's disable latch clears on reset;
CRT revision 1 selects the reverse-bank Hucky variant.
The **Start inserted** row remembers the attached cartridge for the next
launch (`media.cartridge.startup: true`). It defaults to off. Ejecting the
cartridge clears the saved image; switching the option off restores
session-only insertion. Final Cartridge III starts its desktop on cold boot;
hold RUN/STOP during startup to enter BASIC instead.

`machine.options.reu` accepts `off`, `128k`, `256k`, `512k`, `1m`, `2m`,
`4m`, `8m`, or `16m`. The first three sizes model the original 1700, 1764
and 1750. Larger sizes use a contiguous 24-bit expansion. `georam` accepts
`off`, `512k`, `1m`, `2m`, or `4m`. Selecting one expansion disables the
other. Capacity changes restart on closing setup; snapshots retain RAM and
transfers in progress. Cartridge write protection uses `media.cartridge.write_protect`.

Single-SID PSID v1-v4 and RSID v2-v4 files open through Load or drag and
drop. The player shows title, author, release, sub-tune and elapsed guest
time. Left/Right chooses a sub-tune, Space pauses/resumes, and Escape stops
playback and returns to BASIC. File clock and SID-chip hints choose PAL or
NTSC and the C64/C64C model; a file without them plays on a PAL C64 with a
6581. Loading a tune disconnects the cartridge;
disk and tape images remain attached. Multi-SID files and MUS data requiring
an external player are refused.

Disk, tape and program choices last for the current session. Cartridges
also last for the session unless **Start inserted** is on; snapshots retain
their media references. Configuration image paths without this opt-in are
ignored and removed when settings are saved. Auto-load
pastes each complete command instantly at the BASIC prompt.

F9 toggles one status line: `D8:18.5*M T:001234PM J:2` identifies
drive device/track, LED (`*`) and motor (`M`), tape byte position and
play/stop/record (`P`/`S`/`R`) with motor, and keyboard joystick port
(`-` means off). Tape position counts bytes after the TAP header; the compact
counter wraps at one million bytes. Temporary messages share this line.

Ctrl+F3 opens drive controls; Ctrl+F4 opens tape controls. Up/Down selects,
Enter acts, Left/Right switches device, and Escape or the same shortcut
closes the widget. Left/Right switches between drive and tape
controls. The guest continues running. Drive controls offer loading, eject
and file saving. Tape adds play, stop, rewind and record. Cartridge
selection remains on the Cartridge settings page. The Media settings page holds device
number, fast loading, sounds and auto-load, grouped by drive and tape.

Shift+F9 or controller Back toggles the on-screen C64 keyboard. Click/touch
a key to hold it; multiple fingers can press different keys together. Tap
Shift, C= or Ctrl to latch it, then tap again to release. Controller arrows
select a key, A holds it, and B closes; host arrows and Return work too.
Closing the keyboard or losing focus releases its contacts. Snapshots retain
the keyboard's visibility, selection and latched modifiers. The keyboard
always touches the screen bottom and covers the status line while open.

F12 opens setup. Left/Right or Tab changes page, Up/Down selects a row,
Enter or +/- changes its value, and F12/Escape closes. Model (C64/C64C),
video standard (PAL/NTSC) and SID type are separate settings. These changes
restart the machine on close; display and sound changes apply immediately.
Setup, file browsers and the SID player follow the selected border size
and the running model's display aspect, just like the guest screen.
Changes preserve comments in the selected YAML file. Unavailable controls
are disabled.

`video.look` is `crt` or `plain`; `video.filter` is `nearest` or `linear`.
The Display page combines these into CRT, Nearest and Linear. Border is
`full`, `minimal` (16 pixels around the picture), or `none`. Palette is
`default` (automatically follows the running PAL/NTSC model) or
`color-corrected` (Christopher Jam colors). Both are built in; no palette
files are required. Full screen, integer scaling, pixel shape and vsync are live.
On a display slower than 48 Hz, such as a remote desktop session, pictures
are shown without waiting for vsync, since waiting would hold the machine up.

Alt+F12 resets; Ctrl+F12 cold boots; both close the menu. Shift+F12 powers off. F10 switches
100%/full speed (sound is muted at full speed), Pause stops/resumes, and
Alt+Enter switches full screen, including while setup is open. Print Screen writes a PNG under
`work/screenshots/`. Setup and status messages use the same presenter as
the machine, including its display filter.

Keyboard text follows the host layout. The C64 keys for each character are
pressed with the required Shift; F1-F8 and cursor keys produce the C64's
shifted combinations. Esc is RUN/STOP, Page Up RESTORE, Tab CTRL, Page Down
Commodore, Home CLR/HOME, Backspace INST/DEL, and Insert shifted INST/DEL.
Override any physical C64 key or shifted function/cursor key under `keyboard`:

```yaml
keyboard:
  RUN STOP: Escape
  RESTORE: PageUp
  F2: F2
controllers:
  joystick1: {type: joystick, device: none}
  joystick2: {type: joystick, device: controller 1}
  keyboard_joystick: false
  keyboard_port: 2
  keys: {up: Up, down: Down, left: Left, right: Right, fire: Left Ctrl, fire2: Left Shift}
```

Binding values are SDL scancode names, case insensitive; F9-F12, Pause and
Print Screen are reserved by the shell. Character input remains symbolic.
F11 switches the keyboard joystick; Shift+F11 swaps complete port assignments
and identifies the active joystick port. The Controllers page has one
`KEYBOARD JOYSTICK (P1/P2) F11` entry using that same action; it shows OFF
when disabled. A connected controller temporarily disables keyboard
joystick input and the entry shows AUTO OFF. Disconnecting the last
controller enables the keyboard joystick again. `keyboard_port` selects
its socket independently of each port's input source. Port 2 is the default.
Controllers keep their number while connected; disconnect releases their input.

Controller mapping:

| Input | Action |
| --- | --- |
| D-pad / left stick | Movement on the assigned C64 port; menu navigation |
| A | Fire; accept in menus |
| B | F12 menu; close the current dialog |
| X | Open the Load browser (Ctrl+F1) |
| Y | Open/close the media control OSD |
| LB | Swap control ports (Shift+F11) |
| RB | Full speed on/off (F10) |
| LT | Quick save (Ctrl+F5) |
| RT | Quick load (Ctrl+F6) |
| Start / Back | F12 menu / on-screen keyboard |

Each trigger acts once per squeeze. In the on-screen keyboard, A holds
the selected key and B closes it.

Each port's `type` is `joystick`, `mouse1351`, `paddles`, or `none`; its
`device` is `none`, `controller 1`, `controller 2`, or
`mouse` (paddles). The 1351 always uses relative host mouse movement and the
left/right buttons; F12 releases capture while setup is open. Paddles use
absolute mouse X/Y and the left/right buttons with `device: mouse`, or a
controller's left X/Y axes and A/B. Mouse and paddle values reach the SID
through the CIA-controlled 4066 switch and the 512-cycle ADC conversion.

Normal Windows startup opens only the emulator window. Command-line output
uses an existing terminal or redirected pipes.

`--hidden` never opens a window or an audio device. Supply `--frames N`,
`--script FILE` or `--control stdin|tcp:PORT`. TCP binds to localhost.
`--screenshot FILE.png` writes the final raw frame; `--audio FILE.wav` records
guest audio. `--list-models`, `--list-roms` and `--version` print information.

Scripts contain one command per line; blank lines and `#` comments are ignored.
`pointer ID X Y down|move|up|cancel` sends a contact to the open on-screen
keyboard in raw frame coordinates. Independent IDs exercise multi-touch;
`key Pad A`/`release Pad A` and `key Pad B` exercise controller actions.
Supported commands are `wait SECONDS`, `frames N`, `type TEXT`, `enter`,
`key NAME`, `release NAME`, `joy PORT [up down left right fire fire2]`,
`shot FILE.png`, `text`, `expect TEXT`, `until SECONDS TEXT`,
`state save|load FILE.rcs`, `poke HEXADDR HEXBYTE...`, `peek HEXADDR [HEXCOUNT]`,
`trace FILE|off`, `audio FILE.wav|off`, `reset [power]`, `speed full|normal`
and `quit [STATUS]`. `type` accepts `\n`, `\b` and `\\` escapes. Ports are
numbered from zero. Media commands report unavailable slots until the
corresponding devices are available. Host controls are accepted by `key`
too, for example `key F12`, `key DOWN` and `key Ctrl+F12`.

Control takes the same commands, one per line, and returns one JSON object
per command: `{"ok":true,"frame":N}`, with `text` or `data` for a result,
or `{"ok":false,"error":"..."}`. The machine waits between headless commands.
Script errors stop the run with the failing line number and a nonzero status.

The test screen echoes letters and digits, accepts two diagnostic joystick
inputs and exposes 64 KiB of diagnostic RAM. Its bus trace is synthetic.
States include time, RAM, keys, joystick input and audio timing. Model and ROM
hash mismatches are rejected. The diagnostic trace is distinct from the
C64 models' actual CPU bus trace.

The Load browser uses Up/Down, Page Up/Down and Home/End to select a file;
Enter opens a folder or image directory and loads the selected program.
Left or Backspace goes back; at a filesystem root it lists the available
roots. Typing a name searches the list. Space inserts a selected image
without loading it. D64 and T64 contents show their PETSCII names, including
reverse controls; TAP contents use the machine's tape-directory decoder.
Snapshot, cartridge and SID files load directly on C64 models.

The Save page uses Tab to switch between the file list, format and name.
Left/Right or Enter changes the format (PRG, TAP or D64); the list shows
only that type plus folders. F2 renames the selected file; Delete removes it
after confirmation. In the name field, Ctrl+A selects the whole name for replacement,
Backspace edits it and Enter saves. The matching extension is added when
missing. Replacing an existing file requires a separate Enter/Y confirmation;
Escape/N cancels that replacement. Escape closes either browser.

Ctrl+F1 opens Load and Ctrl+F2 opens Save, including from setup. Dropped
PRG/P00 files load and run; dropped D64/T64/TAP files open their contents.
G64 images mount directly for loading through the emulated drive; Space in
the file list inserts one without auto-loading. Raw data, half-tracks and
per-byte recording speeds are retained when writing to a G64 image.
With auto-load enabled, choosing Load runs the machine's LOAD/RUN commands through real keyboard
input after a cold boot. Space inserts without auto-loading. Opening a
browser pauses the guest; closing returns to the previous setup or machine
screen. Manual keyboard input, focus loss and reset cancel auto-typing.

The control OSD places Load and Save first, followed by available tape
Play, Stop, Rewind, Record and Eject actions. Unavailable actions are hidden.
The Machine settings page selects the drive type (1541 or 1541-II).
The Media settings page selects device 8–11, disk/tape fast loading,
sounds and auto-load. **Default media folder** opens a folder selector:
Enter opens a folder, Space selects the current folder, and Escape cancels.
`ui.media_folder` stores it for the first browser opened after startup;
later browsers remember the last visited folder. Relative paths are relative
to the configuration file. Alt+F toggles drive fast loading, Alt+S drive sounds, and Alt+T
tape sound. `machine.options.drive_device` stores the device number;
`machine.options.drive_model` accepts `1541` (the default) or `1541-ii`.
Saving requires a nonempty BASIC program. PRG exports its bytes and load
address; TAP and D64 create and insert an image containing the program.
The saved tape is stopped, so saving does not leave fast playback running.
Modified media are flushed before replacement, ejection, restart and exit.
A model restart retains mounted images and selected entries.

`media.tape` supplies default fast, sound, autoload, full_speed, traps,
save_traps and accelerate settings for `media.program` as well. An explicit
program setting overrides that one default; storing an image path alone
preserves inheritance. `media.disk` has independent fast, sound, autoload
and full_speed settings. Fast loading mutes the corresponding media sound.
Turn fast loading off to restore that sound preference. D64 and G64 use the
1541 recordings during disk access. TAP uses only the tape signal during
playback or recording. PRG, P00 and T64 loads are silent, including when
they use the synthesized tape path with fast loading off.
D64 and G64 loading and saving always use the selected emulated drive.
KERNAL traps and skipped loader instructions never apply to disks.
Automatic full speed runs while the configured media is active, with every
guest cycle still executed. Manual F10 full speed continues to mute sound.

## Snapshots

Ctrl+F5 saves the selected slot; Ctrl+F6 restores it, including from setup.
If no slot has been selected since startup, either action opens a popup:
choose with Up/Down and confirm with Enter/A, or cancel with Escape/B.
That selection serves both quick actions for the rest of the session.
The previous slot remains highlighted at the next launch and needs confirmation.
The States page has ten slots per model, a guest-screen preview, save time,
and Save/Load/Delete actions. Up/Down chooses a slot; Enter on a slot selects it.
Delete or the Delete row removes the current snapshot after confirmation;
Escape/N cancels. An invalid snapshot can also be deleted.
`ui.state_slot` remembers the selection (1–10). Files are stored atomically
at `states/<model>/slot-N.rcs` beside the configuration file. The Load browser
and dropped `.rcs` files use the same restore path.

Snapshots retain chip timing, disk/tape positions, incomplete transfers,
and pending auto-load/RUN input. Media are referenced by absolute path and
SHA-256; modified media are flushed when saving. Loading checks integrity,
model, ROMs, machine options and media before replacing the running machine.
Missing or changed media and incompatible snapshots are refused. Display,
sound, manual speed and host input preferences remain current. Returning
from full speed reanchors pacing immediately.

The headless `drop FILE` command takes the same path as a native drop;
`insert SLOT FILE` mounts only, for explicit LOAD or tape-control commands.
Headless `key Pad A|B|X|Y|LB|RB|LT|RT|Start|Back` invokes the same controller
actions; `key Pad Up|Down|Left|Right` navigates open menus.
Typing while a file dialog is active edits its search/name field, and
`enter` activates the current dialog selection.
