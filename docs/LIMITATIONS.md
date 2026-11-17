# SID engine availability

reSID remains the default engine for both 6581 and 8580. The Sound page also
offers **Built-in (experimental)** by explicit opt-in. It plays at the same
level as reSID and follows it closely on both chip models, but it is not
accuracy-qualified: combined waveforms are approximated, and the 6581 filter
only approximates how the cutoff moves with signal level. Changing engines
restarts the machine; snapshots retain and automatically restore their engine.
Built-in snapshots saved by 0.16.1 no longer load; reSID snapshots are unaffected.

The strict qualification criteria in [SID-COMPARISON.md](SID-COMPARISON.md)
remain unchanged. Experimental availability is separate from passing those gates.

The permitted reSID interface exposes eight waveform bits through OSC3,
not the complete twelve-bit word. Public raw debug output is mixed PCM.
An isolated-wave calibration also maps distinct waveform codes to the same
PCM value. These observations cannot establish exact twelve-bit equality;
the comparison reports that requirement as unverified, never as passed.

## Comparison result

Contract 1, fixture SHA-256
`64dc216cd5d7b80d0bd2fd9c20f70be9e4233432446acac1b4bfe08602ed3c8c`, with
thresholds, gains and time alignment untouched:

| Required check | 6581 | 8580 |
|---|---:|---:|
| Logs with every observable digital comparison exact | 4 / 5 | 4 / 5 |
| Exact snapshot continuation | 5 / 5 | 5 / 5 |
| Audio windows within both fixed limits | 39 / 40 | 40 / 40 |
| Complete twelve-bit waveform equality | Unverified | Unverified |

The log that still differs is the combined-waveform one: OSC3 readback
differs on 393 (6581) and 1,362 (8580) of 73,728 voice cycles, and on the
6581 the waveform feedback then moves the phase on 13 and the noise register
on 8 cycles. Oscillators, noise, pulse, ring, sync, TEST and the envelopes
are exact on the fixture and on randomised register logs. The failing 6581
audio window is band-pass at resonance 8: relative RMS error 0.295 against
0.25 and correlation 0.965 against 0.97. The worst 8580 window is 0.059.

Two real tunes rendered through the emulator at 48 kHz correlate 0.989
(6581) and 0.998 (8580) with the reSID render, at the same level. The model
and how its constants were obtained are described in [HARDWARE.md](HARDWARE.md).

Both engines have exact snapshot continuation. Native built-in checks play a
PSID on 6581 and an RSID on 8580, preserve SID state during debugger peeks,
produce identical captured PCM after restoration, and restore the snapshot's
engine after switching.
