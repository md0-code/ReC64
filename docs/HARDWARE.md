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
