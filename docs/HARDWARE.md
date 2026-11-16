# Hardware interfaces

`hw/hw.h` defines the chip operations (`read`, `write`, `event`, `reset`,
`input`, `save`, `load`) and board callbacks. A chip never calls another chip
or the bus. `rc_cpu_cycle` describes the address, byte, read/write direction
and instruction-fetch pin of a single CPU access.

Guest time counts model CPU cycles. `rc_sched_cycles` runs the board's phi1,
phi2 and tick callbacks once per cycle, with deadlines delivered in time order
and registration order for ties. The board performs the address decode and
BA/AEC/RDY arbitration, and advances the CPU only on a granted access.
The 1541's separate clock will be advanced by an integer ratio on the board.

The family registry in `core/machine.c` exposes models, options, slots,
keyboards and the board implementation to the shell. Core and chip files use
portable headers only. Host services live behind `host/host.h`.

`.rcs` uses `REC64ST1`, a little-endian versioned header with family, model
and ROM SHA-256 values, then tagged chunks and an `END ` marker. Unknown
chunks are skipped. Memory is run-length packed. Each chip restores its own
deadlines on load. The host writes the resulting buffer atomically.

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
