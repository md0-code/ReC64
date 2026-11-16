# Configuration and control

`--config FILE` reads a YAML configuration. Unknown keys, duplicate keys and
invalid values are rejected with a line number. Paths inside it are relative
to the configuration file. `src/app/rec64.yaml` is an example. Models are
`c64-pal` (default), `c64c-pal`, `c64-ntsc`, `c64c-ntsc`, and the firmware-free
diagnostic model `test`. Run `python tools/fetch_roms.py` to obtain the pinned
firmware. `machine.options.sid` accepts `default`, `6581`, or `8580`.

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
Alt+Enter switches full screen. Print Screen writes a PNG under
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

`--hidden` never opens a window or an audio device. Supply `--frames N`,
`--script FILE` or `--control stdin|tcp:PORT`. TCP binds to localhost.
`--screenshot FILE.png` writes the final raw frame; `--audio FILE.wav` records
guest audio. `--list-models`, `--list-roms` and `--version` print information.

Scripts contain one command per line; blank lines and `#` comments are ignored.
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
