# Hardware interfaces

`hw/hw.h` defines the chip operations (`read`, `write`, `event`, `reset`,
`input`, `save`, `load`) and board callbacks. A chip never calls another chip
or the bus. `rc_cpu_cycle` describes the address, byte, read/write direction
and instruction-fetch pin of a single CPU access.

Guest time counts model CPU cycles. `rc_sched_cycles` runs the board's phi1,
phi2 and tick callbacks once per cycle, with deadlines delivered in time order
and registration order for ties. The board performs the address decode and
BA/AEC/RDY arbitration, and advances the CPU only on a granted access.
The 1541's separate 1 MHz clock advances by an integer ratio on the board.

The family registry in `core/machine.c` exposes models, options, slots,
keyboards and the board implementation to the shell. Core and chip files use
portable headers only. Host services live behind `host/host.h`.

`.rcs` uses `REC64ST1`, a little-endian versioned header with family, model
and ROM SHA-256 values, then tagged chunks and an `END ` marker. Unknown
chunks are skipped. Memory is run-length packed. Each chip restores its own
deadlines on load. The host writes the resulting buffer atomically.

Snapshot files add media paths and SHA-256 hashes (`MDIA`), a palette-indexed
guest-screen preview (`THMB`), local save time and application version, and
the pending auto-typist/keyboard queue (`AUTO`). A final `SHA2` chunk covers
the header and every preceding chunk. Restoring validates a separate machine
and its media before exchanging the live machine. Host pacing, display and
sound preferences remain outside guest state. A speed-mode transition resets
the wall-clock anchor; full-speed input timing follows every emulated frame,
including those whose picture is skipped.

## NMOS 6510 / 6502

`hw/mos6510.h` exposes the pins of one access and accepts one granted cycle.
RDY repeats reads; writes continue. Boards implement AEC arbitration outside
this chip. The same core runs a 6502 without the integrated port. It handles
all 256 opcodes, NMOS decimal arithmetic, page-crossing and dummy accesses,
stack/interrupt/reset cycles, JAM, and ROM fetch hooks. The unstable immediate
opcodes use the documented $EE silicon constant. The six external port pins
are supplied by the board. State includes every in-flight microcycle latch.

`python tools/cpu_suite.py fetch` downloads the pinned NMOS vectors;
`python tools/cpu_suite.py run` checks registers, RAM and every bus cycle.
The JSON and lossless binary conversion stay under `work/cpu/`.

IRQ recognition uses the penultimate-cycle sample, with the early sample
of a taken branch that stays on its page. NMI keeps its edge latch and
instruction-boundary decision separately, including BRK/IRQ vector hijacks.
These latches are part of snapshots. Lorenz's unmodified interrupt programs
exercise them on the running board, alongside the CIA register interactions.

## C64 board

The C64/C64C PAL/NTSC models wire the 6510, VIC-II, two CIAs, PLA, keyboard
and reSID to the published address/data/control signals. The PLA selects
ROM, RAM and mirrored I/O; writes reach the RAM under ROM. CIA2 selects the
VIC bank, including the character-ROM windows. BA stops CPU reads before
AEC gives phi2 to VIC DMA. Colour RAM drives only the low four data bits.
The keyboard and joystick switches resolve in both scan directions.
The unconnected cassette input and retained 6510 port charge are modelled
on the board. TOD derives from the model's mains rate with integer timing.

The unchanged KERNAL and BASIC perform RAM initialization and reach READY.
RESET retains RAM and the VIC raster (VIC has no reset pin); a cold boot
initializes the complete board. States contain the clock, memories, bus
latches, port charge and every chip, with model/ROM checks at the machine
interface. Text readback uses screen RAM in the selected VIC bank. Debug
reads leave chip flags and saved state unchanged.

`python tools/lorenz_suite.py fetch` retrieves the public-domain programs
from SVN revision 46417. `run` executes the C64 programs on the booted board
without ROM traps; `run --new-cia` selects the corresponding CIA variants.
Disk-change prompts are transport helpers. Results and downloaded binaries
stay in `work/testprogs/`.

## CIA, PLA and keyboard

The 6526/8521 has cycle-pipelined timers with CNT/cascade sources, force load,
one shot, PB6/PB7 outputs, model-specific IRQ timing, FLAG, serial transfer,
and external 50/60 Hz TOD pulses with BCD clock, alarm and read latching.
The board ticks it before the phi2 register access and supplies port inputs.
The timer smoke uses Lorenz's published twelve-cycle cascade sequence.

The PLA evaluates the original 82S100 product equations. Input flags are the
physical levels in the paper; output flags are asserted chip selections.
The board supplies CPU/VIC ownership, address, cartridge and banking pins.
The keyboard resolves the bidirectional 8x8 switch network to a fixed point,
including ghost keys and both joysticks. RESTORE has its own output line.
Each module includes its state interface and a focused state-round-trip check.

## SID

`hw/sid.h` is the C-only 6581/8580 chip and audio interface. The backend uses
reSID resampling at the configured PAL/NTSC clock and sample rate. Its state
journal combines adjacent clock advances and records register reads/writes.
Restore replays that journal, reconstructing the analogue filter and FIR
history omitted by reSID's public digital State, then restores queued audio.
Backend construction uses cleared storage, avoiding an uninitialised filter
lookup observed with the pinned release on Windows. Vendored files are
unchanged. The focused check requires non-silent audio and identical samples
across a state round trip for both chip models.

## VIC-II

The 6569/8565 and 6567R8/8562 models expose phi1/phi2 memory requests and
BA/AEC/IRQ outputs. The board supplies RAM, character ROM and colour RAM;
the VIC never calls the bus. Address generation includes matrix/graphics,
sprite pointer/data, refresh and idle accesses. The sequencers cover the
text/bitmap modes, multicolour, ECM, invalid-mode priority data, scrolling,
border flip-flops, eight sprites with expansion and priority, collision
latches, raster IRQ and lightpen. The frame is the complete raw raster in
time order (504x312 PAL or 520x263 NTSC); visible-area cropping belongs to
the board/presenter. Every pipeline latch and the partially drawn frame
are saved, including a save between the two bus phases.

## Media containers

The format modules parse and create byte images without host I/O. Their
`image` member owns a portable media buffer, which the host may write through
its existing media save path. Opened byte images are read-only; the caller
sets their writable flag from the host file's permissions. Created images
are writable. Close an image before opening a replacement.

D64 supports 35/40/42 tracks and optional per-sector error bytes, bounded
sector access and writes, directory enumeration and linked-file extraction.
The blank-disk writer creates a standard 35-track DOS 2A BAM and directory.
T64 reads the actual program entries, checks offsets, handles the common
CONV64 $C3C6 end-address defect, and writes multi-entry containers. PRG/P00
preserve the load address and PETSCII filename. TAP v0/v1 expose cycle-length
pulses and a saved cursor. A v0 overflow is 2048 cycles; recording a pulse
that v0 cannot represent upgrades the image losslessly to v1. Every format
has a focused create/read/write/state check. Transport and KERNAL integration
belong to the later loading packages.

## VIA and GCR

The 6522 ticks before the drive CPU's phi2 register access. It exposes port
directions, live or edge-latched input, CA/CB handshake and pulse outputs,
independent interrupt inputs, IFR/IER, both timers, PB7 timing output and
PB6 pulse counting. All eight shift modes use CB1/CB2, with the T2 low-byte
reload controlling the divided shift clock. Control outputs report -1 when
released; IRQ reports 1 when asserted. State preserves counters, reload
delays, handshake pulses and an incomplete serial transfer. Reset preserves
the timer and shift-register contents specified as unaffected by RES.

GCR helpers encode and validate the 4-to-5-byte groups, sector headers and
256-byte data blocks, including block identifiers and XOR checksums. A
circular raw-bit buffer retains its exact bit length, position, density,
write protection and modified data in states. Density timing uses integer
16 MHz ticks (52/56/60/64), avoiding drift between the four speed zones.
Sync detection and drive wiring belong to the 1541 package. The GCR check
includes the worked conversion from Inside Commodore DOS.

## Control ports

The board's `pointer` input takes a socket, controller type, X/Y and two
buttons. For the 1351, X/Y are signed host movement; the chip stores six-bit
counters and presents bits 6:1 to POTX/POTY. POTY increases upwards. Its
buttons connect to fire and up. For paddles, X/Y are absolute 0-255 conversion
times and the buttons connect to left/right. The board clocks a 256-cycle
discharge plus 256-cycle conversion through the CIA1 PA6/PA7-controlled
4066 multiplexer. Mouse counters, paddle inputs and ADC phase/results are
serialized in BUS version 2; version 1 states load with disconnected inputs.
The SID adapter overlays external ADC read data and its data-bus lifetime,
leaving the vendored reSID and its analogue history unchanged.

## Datasette and KERNAL tape blocks

`rc_datasette` takes logical motor-on and the electrical CPU write level;
the board maps 6510 P5 (active low) and P3 into them. Its sense is low while
PLAY or RECORD is latched, even with the motor off. The read callback is
an electrical level for CIA1 FLAG: each TAP duration has one falling edge,
with an intermediate rise. Stopping the motor freezes the exact remainder
of a pulse. Recording measures WRITE rising-edge periods (READ falls after
the recorder's inverted signal path) while the motor
runs and marks the mounted TAP image dirty. No device calls another chip.
The deck borrows a mounted `rc_media`; loading a state updates the existing
borrowed media object, or owns the restored image when none was mounted.
Media, transport, waveform phase and partial recording periods are saved.

The portable tape codec recognizes original KERNAL headers, nine-byte
countdowns, LSB-first data and odd parity, the XOR checksum, repeated header
and data blocks, and the long/short end marker. Directory entries keep the
pilot's TAP byte offset for genuine tape seeking. The encoder emits the
same format with 384/528/688-cycle short/medium/long nominal pulses. Pilot
counts are derived from the original ROM: header `$69` plus rollover gives
`$6A00` short periods; data `$14` gives `$1500`; the repeated-copy pilot
includes the end marker's short period. The original SAVE code, not the
encoder, produces recorded waveforms during guest execution.
## Known-ROM loading hooks

The optional LOAD and SAVE hooks are enabled only for the pinned KERNAL
901227-03 SHA-256. They watch F4A5 and F5ED after the ROM entry wrappers
have stored their parameters. The board supplies memory and virtual-file
callbacks, and checks ROM mapping before dispatch. A callback can decline,
leaving the real routine untouched; physical TAP always declines. Disabling
loading hooks clears the watch bits. No ROM byte is changed.

Successful LOAD returns to F5A9, where the original CLC/LDX/LDY/RTS executes;
SAVE returns to F68D. Negative callback results enter the original KERNAL
error table. LOAD supports relocation and VERIFY status. Flags and counters
are serialized, and a saved hook state cannot enable a different ROM hash.

## C64 media wiring

The board exposes tape, virtual program and disk-image slots. The current
virtual-program or tape slot selects one Datasette path. PRG/P00 and T64 entries
have an original-format synthesized tape available even while fast hooks
are enabled. Turning hooks off therefore uses the same data through the
actual KERNAL tape routines. TAP always bypasses the hooks. Type3 absolute
headers preserve their address even with secondary address zero; auto-load
selects RUN for BASIC or SYS for another start address.

CPU P4 receives active-low sense, P5 controls the active-low motor, P3 sends
write transitions, and Datasette read drives CIA1 FLAG. Tape samples are
mixed with SID audio at the board's output rate. The board saves mounted
media and synthetic data, selected entries, recorder phase, loading-hook
state and pending tape audio along with the chips. A separate, fixed board
fetch mask keeps tape-prompt handling available when fast hooks are off.
The board presses PLAY at the known KERNAL prompt, rather than on motor
changes during reset, so an idle auto-load cannot consume the tape first.

## 1541 board and IEC

The drive runs its unmodified 16 KiB DOS on the cycle-stepped NMOS 6502,
with 2 KiB RAM and two independent MOS 6522 VIAs. Its board decodes memory
and wires both VIA interrupts to IRQ. The serial VIA receives inverted
DATA, CLK and ATN, and its PB4 attention acknowledgement combines with ATN
through the schematic's XOR gate. Bus endpoints only pull lines low; the
resolved levels are the AND of their released outputs. Device jumpers select
8 through 11. The drive sees the physical reset line.

The mechanism stores circular GCR tracks with a nominal 200 ms rotation.
The 16 MHz clock divided by 13, 14, 15 or 16 gives the four recording zones.
Serial read bits feed the shift register; ten consecutive ones reset byte
framing and assert active-low SYNC. Byte-ready pulses reach VIA CA1 and,
when CA2 enables them, the CPU's logical SO input. The ROM decodes the GCR
and executes disk jobs itself. CB2 switches reading and writing; Port A
supplies the next serial write byte. Motor, LED, phase changes and head
bumps come from the actual VIA outputs. A head step preserves angular
position across tracks.

D64 sectors are converted to header and data blocks with their checksums,
sync and gaps. Extended-image error codes change the corresponding physical
header/data descriptor, checksum or disk ID while retaining valid GCR.
Completed writes are decoded back into the mounted, borrowed image before
the host flushes it. State includes both processors' interface state, drive
RAM, bus levels, mechanism phase, raw tracks and an unfinished write. No
firmware byte or custom-loader instruction is replaced.

Drive sounds are generated at 48 kHz from measured frequencies, levels,
envelopes and durations of the pinned 1541 recordings. Integer oscillators
and a seeded noise source produce motor spin-up, steady hum and coast-down;
bounded overlapping voices produce alternating head steps and the head
stop bump. No recorded samples are shipped. The board feeds actual motor
levels and monotonically counted mechanical events. Sound state advances
while muted, and its phases, envelopes and pending output are saved exactly.
The original recordings, their hashes and the measurement method are in
`sources.lock.json`.

## Automatic disk speed

Automatic fast loading observes the actual drive motor and the resolved IEC
edge counter after each guest cycle. Motor activity requests full speed
immediately. Serial transitions keep that request alive for
`ceil(clock_hz/50)` cycles (20 ms) after the most recent edge. This is a
host-speed policy that bridges a frame poll; it changes no guest timing.
A bus line held low without further transitions becomes idle after that
interval. Motor stop adds no independent delay. The shell applies the user's
fast-loading switch and returns to 100% when the request ends. The guest
cycle, IEC counter, last-edge cycle and activity flags are saved exactly.

## Cartridge port and game mappers

The board supplies already-decoded ROML, ROMH, I/O1 and I/O2 selects and
the bus value. GAME, EXROM, NMI, IRQ and DMA are physical levels (one is
released); BA is a board input. Optional access observation occurs before
PLA decode, and a drive mask distinguishes floating I/O from driven data
when an expansion shares I/O2. CPU peeks cause no register or bank changes.
VIC ROMH reads in Ultimax use the same file-backed cartridge bytes.

CRT CHIP packets retain their bank, address, type and offset within the
borrowed media. Normal 8K, 16K and Ultimax images follow their physical
header lines. Ocean selects banks with the low six data bits; 256K files
describe the low and high ROM halves using the respective 0-15 and 16-31
CHIP bank numbers. Fun Play preserves literal CRT bank patterns (00, 08,
10 through 39), and 86 disconnects it. Magic Desk uses seven bank bits and
bit 7 disconnects EXROM. System 3 selects by I/O1 write address and resets
the bank on an I/O1 read; Dinamic selects by I/O1 read address. Zaxxon
mirrors its low 4K and latches its high bank from the low-ROM read address.

The current hardware programming manual resolves two errors in the 2007
CRT notes: Simons' BASIC selects 16K on a DE00 write and 8K on a DE00 read,
independent of data; Super Games bit 2 controls the joined GAME/EXROM lines
and bit 3 locks its bank register until reset. No firmware is patched.
Snapshots retain the image bytes, dirty flag, bank, control, lock and BA;
restoring a borrowed image retains its media pointer and write protection.
External flash/freezer mappers bind callbacks to this same port.

`test_cart --fixtures <existing-work-folder>` emits original test CRTs that
display each mapper's name. Their firmware and Ultimax glyphs are original
test material, not real cartridge software; mapper-register and exact-state
checks accompany these startup fixtures.
