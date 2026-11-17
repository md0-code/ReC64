# Configuration and control

`--config FILE` reads a YAML configuration. Unknown keys, duplicate keys and
invalid values are rejected with a line number. Paths inside it are relative
to the configuration file. `src/app/rec64.yaml` is an example. Models are
`c64-pal` (default), `c64c-pal`, `c64-ntsc`, `c64c-ntsc`, and the firmware-free
diagnostic model `test`. Run `python tools/fetch_roms.py` to obtain the pinned
firmware. `machine.options.sid` accepts `default`, `6581`, or `8580`.
This selects the chip model. Release audio uses reSID; the separately built
original candidate is not a selectable backend. Its admission criteria and
availability are in [LIMITATIONS.md](LIMITATIONS.md).

The Cartridge page loads/ejects CRT images and selects expansion memory.
Supported cartridges are Normal 8K/16K, Ultimax, Ocean, Magic Desk, Fun Play,
Super Games, Dinamic, Zaxxon, System 3, Simons' BASIC, EasyFlash, Action
Replay 4-6, Retro Replay and Final Cartridge III. Attaching or ejecting a
cartridge resets the machine. Right Alt pressed and released alone triggers
the physical freeze button; using AltGr with another key does not freeze.
The page's Freeze row performs the same action. EasyFlash retains completed
flash writes in the attached CRT, including banks omitted from a sparse
image; write protection inhibits programming.

`machine.options.reu` accepts `off`, `128k`, `256k`, `512k`, `1m`, `2m`,
`4m`, `8m`, or `16m`. The first three sizes model the original 1700, 1764
and 1750. Larger sizes use a contiguous 24-bit expansion. `georam` accepts
`off`, `512k`, `1m`, `2m`, or `4m`. Selecting one expansion disables the
other. Capacity changes restart on closing setup; snapshots retain RAM and
transfers in progress. Cartridge images use `media.cartridge.image` and
`media.cartridge.write_protect`.

Single-SID PSID v1-v4 and RSID v2-v4 files open through Load or drag and
drop. The player shows title, author, release, sub-tune and elapsed guest
time. Left/Right chooses a sub-tune, Space pauses/resumes, and Escape stops
playback and returns to BASIC. File clock and SID-chip hints choose PAL or
NTSC and the C64/C64C model. Loading a tune disconnects the cartridge;
disk and tape images remain attached. The startup file is
`media.sid.image`. Multi-SID files and MUS data requiring an external
player are refused.

F9 toggles the live status panel while the guest runs, including during
loads. It shows the 1541 device, track, LED and motor; the Datasette byte
position, motor and transport; and the keyboard joystick port. Tape position
counts TAP data bytes after the file header.

Shift+F9 or controller Back toggles the on-screen C64 keyboard. Click/touch
a key to hold it; multiple fingers can press different keys together. Tap
Shift, C= or Ctrl to latch it, then tap again to release. Controller arrows
select a key, A holds it, and B closes; host arrows and Return work too.
Closing the keyboard or losing focus releases its contacts. Snapshots retain
the keyboard's visibility, selection and latched modifiers.

F12 opens setup. Left/Right or Tab changes page, Up/Down selects a row,
Enter or +/- changes its value, and F12/Escape closes. Model and SID changes
restart the machine on close; display and sound changes apply immediately.
Changes preserve comments in the selected YAML file. Unavailable controls
are disabled.

`video.look` is `crt` or `plain`; `video.filter` is `nearest` or `linear`.
The Display page combines these into CRT, Nearest and Linear. Border is
`full`, `minimal` (16 pixels around the picture), or `none`. Palette is
`cjam` (Christopher Jam, default), `colodore`, `pepto-pal`, `pepto-ntsc`, or
a `.vpl` filename in `palettes/` beside the configuration, working directory
or executable. External palettes appear on the Display page automatically.
VPL takes sixteen RGB hex triples with `#` comments (legacy dither columns
are accepted). Full screen, integer scaling, pixel shape and vsync are live.

Alt+F12 resets; Ctrl+F12 cold boots; Shift+F12 powers off. F10 switches
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
F11 switches the keyboard joystick; Shift+F11 swaps complete port assignments.
The keyboard joystick shares port 2 with controller 1 by default. Explicit
`device: keyboard` assignments select its socket instead of `keyboard_port`.
Controllers keep their number while connected; disconnect releases their input.
The d-pad/left stick supplies directions, A fire, and B a second fire button
(where the connected device supports it). Start opens setup; d-pad navigates,
A accepts, B goes back, and shoulders change values.

Each port's `type` is `joystick`, `mouse1351`, `paddles`, or `none`; its
`device` is `none`, `keyboard`, `controller 1` through `controller 8`, or
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
Snapshot files load directly. Cartridge and SID files remain disabled.

The Save page uses Tab to switch between the file list, format and name.
Left/Right or Enter changes the format (PRG program, TAP recording or blank
D64). In the name field, Ctrl+A selects the whole name for replacement,
Backspace edits it and Enter saves. The matching extension is added when
missing. Replacing an existing file requires a separate Enter/Y confirmation;
Escape/N cancels that replacement. Escape closes either browser.

Ctrl+F1 opens Load and Ctrl+F2 opens Save, including from setup. Dropped
PRG/P00 files load and run; dropped D64/T64/TAP files open their contents.
With auto-load enabled, choosing Load runs the machine's LOAD/RUN commands through real keyboard
input after a cold boot. Space inserts without auto-loading. Opening a
browser pauses the guest; closing returns to the previous setup or machine
screen. Manual keyboard input, focus loss and reset cancel auto-typing.

The Media page has tape Play, Stop, Rewind, Record and Eject controls,
plus tape fast loading, sound and auto-load. Disk controls select device
8–11, eject the image, and switch disk fast loading, drive sounds and
auto-load. Alt+F toggles drive fast loading, Alt+S drive sounds, and Alt+T
tape sound. `machine.options.drive_device` stores the device number.
Saving to TAP creates and mounts a recording target: use BASIC SAVE to
record the program. Saving a blank D64 creates and inserts that image.
Modified media are flushed before replacement, ejection, restart and exit.
A model restart retains mounted images and selected entries.

`media.tape` supplies default fast, sound, autoload, full_speed, traps,
save_traps and accelerate settings for `media.program` as well. An explicit
program setting overrides that one default; storing an image path alone
preserves inheritance. `media.disk` has independent fast, sound, autoload
and full_speed settings. D64 loading and saving always use the true 1541;
KERNAL traps and skipped loader instructions never apply to disks.
Automatic full speed runs while the configured media is active, with every
guest cycle still executed. Manual F10 full speed continues to mute sound.

## Snapshots

Ctrl+F5 saves the selected slot; Ctrl+F6 restores it, including from setup.
The States page has ten slots per model, a guest-screen preview, save time,
and Save/Load actions. Up/Down chooses a slot; Enter on a slot selects it.
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
Typing while a file dialog is active edits its search/name field, and
`enter` activates the current dialog selection.
