# SID limitations

ReC64's SID is its own engine for both the 6581 and the 8580. It was built
from the chip documents and brought into line with reSID, the engine of the
releases up to 0.16.2, before that library was removed. Two parts are
approximations:

- **Combined waveforms** come from a selector-line model, not from sampled
  chips, so their OSC3 readback and the 6581 waveform feedback differ on part
  of the cycles.
- **The 6581 filter** only approximates how the cutoff moves with signal
  level: a single quiet filtered voice and the highest resonance settings
  deviate most.

Oscillators, noise, pulse, ring, sync, TEST and the envelopes matched reSID
cycle for cycle on the comparison set and on randomised register logs.

## Last comparison against reSID (0.16.2)

| Check | 6581 | 8580 |
|---|---:|---:|
| Register logs with every observable digital state exact | 4 / 5 | 4 / 5 |
| Exact snapshot continuation | 5 / 5 | 5 / 5 |
| Audio windows within the fixed limits | 39 / 40 | 40 / 40 |

The log that differed was the combined-waveform one. The failing 6581 audio
window was band-pass at resonance 8 (relative RMS error 0.295 against a limit
of 0.25). Two real tunes rendered through the emulator at 48 kHz correlated
0.989 (6581) and 0.998 (8580) with the reSID render, at the same level. The
model is described in the [design notes](DESIGN.md).

Snapshots saved with the reSID engine still load: the SID's registers are
restored and its notes start again. Snapshots of the experimental built-in
engine of 0.16.1 do not load.
