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

## REU and GeoRAM

The REU presents the original 8726 register file throughout I/O2, mirrored
at 32-byte intervals. The 1700, 1764 and 1750 use the original three-bit
bank register, jumper-dependent size flag and address-counter behavior.
Sizes above 512 KiB select the modern contiguous address extension, with
an eight-bit readable bank register, through 16 MiB. The unpopulated upper
DRAM bank of a 1764 does not alias its installed memory; its floating data
is represented by the retained REU data latch. The original hardware's
analogue value in that unpopulated bank is not specified.

The board grants a requested DMA access only after pending CPU writes and
when VIC BA/AEC permit it. The REU exposes address, direction, data and DMA
pins and consumes only accepted accesses. Store, fetch and verify consume
one accepted cycle per byte; swap consumes a read and a write cycle.
Registers are hidden from the REU's own DMA engine. DMA uses the board's
normal PLA mapping, including I/O and ROM, while addresses zero and one
access the RAM behind the suspended 6510's internal port. The deferred
FF00 trigger works independently of whether I/O is mapped. End-of-block,
verify and interrupt status acknowledge only on an ordinary status read;
debug reads preserve all state. Shadow register pairs preserve the 8726's
half-autoload effect. States retain RAM, selectors, shadows and an unfinished
swap, so a VIC stall can be saved and resumed exactly.

GeoRAM exposes a 256-byte window at DE00-DEFF, selected by a six-bit page
and a bank of 16 KiB. The original board decodes its write-only page/bank
registers at every even/odd address in DF80-DFFF. Undriven register reads
retain the board's bus value. Sizes of 512 KiB, 1, 2 and 4 MiB mask the bank
to the installed memory; the 4 MiB option extends the bank latch to eight
bits. Reset preserves RAM and the original non-resettable page/bank latches.
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
## Freezer cartridges

`hw/cart_freezer` models CRT 1 (Action Replay 4.1/4.2/5/6), 30 (earlier
MK4 without cartridge RAM), 36 (Retro Replay), and 3 (Final Cartridge III).
Original hardware documents and unmodified verification firmware are pinned
in `sources.lock.json`; test firmware stays under ignored `work/`.

Action Replay 5/6 decoding follows the physical 82S129 PROM dump, including
its empty ROM windows and cartridge RAM accesses when ROML is not selected.
PROM A0..A7 are control D5, D1, D0, IO2, address A13, A15, A14, and the
inverted freeze latch. O1..O4 are ROM enable, RAM enable, GAME, and EXROM.
The first two outputs are active low. IO1 accesses clock the control latch
on reads as well as writes; read data comes from the preceding VIC half-cycle.
The positive edge of the physical freeze button clocks the first flip-flop.
The counter loads fifteen while BA is low, then its Qc output clocks the
second flip-flop after five consecutive available PHI2 cycles. This releases
IRQ/NMI and enables the frozen memory map. Control bit 6 releases that map.

MK4 uses its different five-bit register, no RAM, and an initially disabled
cartridge. Its normal power-up is the computer's BASIC screen; pressing
freeze enters cartridge firmware. Register bit 2 disables the cartridge and
rearms freezing. A five-cycle counter likewise restarts when BA is low.

Retro Replay provides eight 8 KiB ROM banks and four 8 KiB RAM banks.
The flash jumper is unset, which physically inhibits flash writes. Its
write-once AllowBank, NoFreeze and REU-compatible bits retain their original
semantics; enabling the unpopulated accessory connector leaves those
register addresses floating. The button must remain down for more than two
microseconds before release. Freeze asserts IRQ/NMI and waits for three
consecutive CPU writes before selecting bank zero and its vectors. VIC reads
and observation-only debugger reads do not advance the CPU write detector.

FC3 has four 16 KiB ROM banks and no RAM. IO1/IO2 always expose the lower
ROM's final 512 bytes, including when bit 7 hides its write register. Its
button asserts NMI immediately and GAME after seven BA-enabled PHI2 cycles;
release removes the button contribution while the register can keep both
lines asserted. Reset clears the register, including its active-low NMI
output. ROMH is also available on VIC accesses for the firmware's freezer
menu font and sprite data.

All RAM, register visibility, freeze state, button level, debounce age,
counter position and BA state are serialized. The board supplies real bus
cycles, combines IRQ/NMI lines, and presents a qualified freeze action as a
physical button pulse. Cartridge code never accesses the board or CPU.

`tools/verify_freezers.py` uses `work/p7/freezers/fixtures/{ar4,ar41,ar5,ar6,rr,fc3}.crt`;
`--case` selects one firmware. The four Action Replay files are unchanged
members of the pinned archive. FC3 wraps the pinned 64 KiB binary in CRT type
3 with four 16 KiB CHIP packets. RR wraps the manufacturer's `RR38QROM` file
from `Flashtool.zip` / `flashutil.d64` in type 36 with eight 8 KiB packets;
that file is exactly 64 KiB with no two-byte PRG prefix. The script checks
the original file or concatenated ROM payload hash before execution. No
fixture is included in releases. FC3 enters BASIC with RUN/STOP held during
startup, as specified in its original manual; its freezer menu uses ROM
sprites and must be inspected in the captured image rather than extracted
screen text. AR4/4.1 use an overlay strip and F5 horizontal option selector.

## EasyFlash

EasyFlash has two independent Am29F040B arrays, each 512 KiB, and 256 bytes
of RAM at I/O2. DE00 selects their shared 8K bank; DE02 controls the physical
GAME/EXROM lines and LED. Registers are write-only. The enabled boot jumper
starts bank zero in Ultimax; software can override it. Reset changes the
board latches while preserving RAM and active flash operations: the flash
chips have no C64 RESET input. ROMH uses the same flash chip in the CPU's
A000/E000 windows and VIC Ultimax accesses.

The flash engines execute the AMD unlock, autoselect, byte-program, sector
erase and chip-erase command sequences. Programming only clears bits;
attempted zero-to-one programming reports DQ5 failure until a reset command.
Busy status implements DQ7 data polling, DQ6 toggling, selected-sector DQ2
and the DQ3 erase timer. Peeks leave the toggles unchanged. Sector erase can
select multiple sectors within its 50 microsecond command window, suspend,
allow programming outside the selected sectors and resume. The documented
typical durations (7 microseconds program, 1 second sector erase and 8
seconds chip erase) are rounded up to guest cycles. Suspend is immediate,
within the data sheet's 20 microsecond maximum. Unspecified status bits are
deterministic; their values are not a hardware guarantee.

Missing CRT banks begin erased (FF). Completed operations update the borrowed
CRT bytes and dirty flag; the first write to an omitted bank appends a valid
flash CHIP packet containing that bank. The host flushes that same image.
Read-only media inhibit flash writes while retaining banking and RAM writes.
Snapshots retain both flash arrays, command phases, polling bits, pending
timers, selected sectors, RAM, bank, control and jumper. The image's original
firmware, including EAPI, changes only through guest flash operations.

## SID files and player

SID debugger peeks replay the access journal into an isolated reSID instance.
The public `read_state()` API performs register reads and changes the live
bus, so it is never used on the running chip for a peek or a POT lifetime
query. Model bus lifetimes are obtained once from isolated public-API probes.
Actual guest reads remain journaled on the live chip.

The HVSC PSID v1-v4 and RSID v2-v4 reader keeps the original payload separate
from its header and optional little-endian load prefix. It checks the 64 KiB
load boundary, song range, RSID reserved fields and init address, and driver
relocation pages. Clock and chip hints select the corresponding C64 model.
Multi-SID files and Compute! MUS files requiring an external player are refused.

PSID runs an original small 6510 program in a free, permitted RAM page. The
song accumulator is zero-based, each init/play call applies the specified
6510 bank value, and the selected speed bit enables raster or CIA1 timer-A
interrupts. CIA1 timer A runs at the documented 60 Hz startup latch for the
chosen clock. A zero play address leaves playback to the tune's installed
interrupt vector. The firmware and loaded payload are never patched.

RSID waits for the unchanged firmware to reach READY, loads physical RAM,
and calls init on the real CPU stack. A returning init resumes the firmware's
input loop; a non-returning init continues its own code. RSID BASIC sets the
BASIC text/end pointers and zero-based song byte at $030C, then types RUN
through the original keyboard buffer. Interrupt timing remains guest code
and actual hardware. Pause stops guest cycles; the displayed elapsed time is
guest time. The SIDP state chunk retains the launch phase, song, pause,
elapsed/boot cycles and returning-init register context; chip/CPU/RAM chunks
retain the actual running driver and tune state. The player is a transient
view rebuilt from the mounted file and saved machine status.

## Live status panel

The status panel takes a read-only board status and composites four 8-pixel
rows onto a copy of the indexed guest frame, retaining its palette, aspect
and dimensions. Track display uses halftrack / 2 with odd halftracks shown
as .5 (halftrack 2 is track 1). The LED is VIA2 PB3, independent of the
motor at PB2. `TAPE POS` is the TAP data byte offset excluding its 20-byte
header, rather than a mechanical reel counter; motor and PLAY/RECORD are
independent transport values. The joystick field names the active keyboard
joystick port, or OFF. The overlay owns no guest state and never advances,
pauses or changes a device.

## On-screen keyboard

The on-screen keyboard maps 66 physical keycaps to the 64 CIA matrix
contacts and RESTORE. SHIFT LOCK shares the left SHIFT contact. The
quarter-key layout also supplies family keyboard geometry. Independent
pointer IDs hold counted contacts; sliding releases the previous cap and
cancel releases without latching. SHIFT, C= and CTRL latch on release and
toggle off on the next tap; closing or focus loss releases every contact.
Controller accept holds the selected cap until release. Callbacks feed the
keymap's virtual contacts, combined with physical host input. Snapshots
retain visibility, selection and latched modifiers, release transient
fingers and accept, and bind the restored keyboard only after the snapshot
machine is accepted. The 320 by 96 indexed overlay preserves the running
guest and can reserve a lower margin for the status panel.

## Experimental own SID

`hw/sid_own.*` is an original, standalone candidate driven once per SID clock.
It implements 24-bit phase accumulators, triangle/saw/pulse/noise selection,
pulse and HMOS waveform latches, the 23-bit noise register, TEST, sync and ring
modulation, ADSR rate and exponential counters, delayed counter events, sustain
comparison, hold-at-zero and rate-change delay. HMOS register writes enter a
one-cycle latch. Read registers expose OSC3, ENV3 and board-supplied POT values;
other reads expose the retained data bus. Chip RESET clears writable registers
and envelope dividers while retaining phase, envelope level and analog charge;
it releases the noise shift latch. Init alone establishes a deterministic nominal
cold-start state. Neither operation asserts a measured power-up bit pattern.
Nominal charge retention timings are model parameters, not a claim of identical leakage on every physical chip.

The 8580 candidate uses the documented state-variable integrator circuit and
resonance resistor ratios. Its cutoff conductance is linear, using the MOS
nominal 12 kHz range and the die's code-zero half-LSB path. Audio uses a fixed
centered waveform-times-envelope scale (one PCM unit per 64 product units at
maximum volume); it has not been gain-fitted to oracle output. The motherboard
10 kohm/1 nF lowpass and 10 uF/1 kohm AC coupling run at the chip clock. The
comparison interface can bypass that external stage. Rational weighted-area
resampling splits each guest cycle at output sample boundaries, with no clock
drift. This resampler is not a high-order bandlimited reconstruction filter.

The 6581 path solves its resistor ladders by nodal elimination. Waveform and
multiplying-envelope ladders use the published R/2.02R geometry estimate; the
cutoff ladder uses R/1.879R with its twelfth MSB tied high. These values are the
hardware analyst's estimates, including the stated uncertainty of the cutoff
ratio. Integrator currents follow the first-order MOS channel equation, with
pinch-off and signal-dependent gate voltage through the documented dividers.
The two 470 pF capacitors, approximately 1 Mohm leakage paths, resonance
conductances and switched volume resistances come from the published 6581R2
circuit drawing. Four integration substeps run per guest cycle. The finite
inverter/output rails bound large signals. DAC offset and volume conductance
changes produce DC steps before motherboard AC coupling.

Combined waveforms use a dynamic nodal network of the selected waveform
transistors and their shared bitlines. Triangle and saw selection couples
adjacent lines from the same XOR drivers; pulse/noise join those lines through
their own selector switches. The DAC buffer threshold turns solved voltages
back into twelve bits. The selected noise lines can discharge their LFSR
storage bits. On 6581, loading the unbuffered saw MSB can pull the phase MSB
low; the 8580's independent latch prevents this feedback. No sampled waveform
or fitted emulator table is used.

Parameters not established by the available hardware documents are explicit
nominal assumptions. They were chosen before this candidate's comparison and
are not estimates fitted to the reference engine:

| Assumption | Candidate value |
|---|---|
| 6581 MOS threshold and effective channel beta | 1 V; 20 microampere/V squared |
| 6581 inverter operating center and usable output displacement | 5 V; -4 to +6 V |
| Voice buffer DC displacement from the envelope's +5 V reference | +1 V |
| 6581 output PCM normalization | 3276.8 units per volt; 10 V maps to magnitude 32768 |
| Selector low/high driver conductance ratio | 2:1 |
| Shared triangle/saw neighbor conductance; other adjacent coupling | 1.0; 0.08, relative to a high driver |
| Per-cycle bitline charge coefficient; digital threshold | 0.25; half the 5 V rail |
| Narrower 8580 upper triangle selector conductance | 0.8 relative to other selector switches |

These circuit-based approximations do not establish chip-specific analog or
combined-waveform equality. Admission remains governed by the unchanged
comparison contract in [SID-COMPARISON.md](SID-COMPARISON.md).

The candidate has no dependency on reSID, SDL, an operating system or another
chip. Save/load stores all counter latches, pending writes, analog integrators,
fractional resampling position, per-bit selector charge, DAC coefficients and
queued PCM explicitly (candidate payload version 2), with transactional
validation on load. `test_sid_own` checks both chip models for exact continuation
with active combined waveforms, nonlinear filter history and a pending HMOS write. Its fidelity and selectable-engine status are
controlled by the phase-10 comparison gate; snapshot equality alone is not
proof of SID hardware fidelity.
