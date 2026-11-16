# Configuration and control

`--config FILE` reads a YAML configuration. Unknown keys, duplicate keys and
invalid values are rejected with a line number. Paths inside it are relative
to the configuration file. `src/app/rec64.yaml` is an example. The only current
model is `test`, the firmware-free diagnostic screen.

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
corresponding board hardware is implemented.

Control takes the same commands, one per line, and returns one JSON object
per command: `{"ok":true,"frame":N}`, with `text` or `data` for a result,
or `{"ok":false,"error":"..."}`. The machine waits between headless commands.
Script errors stop the run with the failing line number and a nonzero status.

The test screen echoes letters and digits, accepts two diagnostic joystick
inputs and exposes 64 KiB of diagnostic RAM. Its bus trace is synthetic.
States include time, RAM, keys, joystick input and audio timing. Model and ROM
hash mismatches are rejected. These are foundation checks, not C64 execution.
