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
