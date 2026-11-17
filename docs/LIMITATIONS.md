# SID engine availability

reSID remains the default engine for both 6581 and 8580. The Sound page also
offers **Built-in (experimental)** by explicit opt-in. It is not accuracy-qualified
and can sound different, especially with combined waveforms, envelope retrigger
tricks and filtered or volume-sampled music. Changing engines restarts the machine;
snapshots retain and automatically restore their engine.

The strict qualification criteria in [SID-COMPARISON.md](SID-COMPARISON.md)
remain unchanged. Experimental availability is separate from passing those gates.

The permitted reSID interface exposes eight waveform bits through OSC3,
not the complete twelve-bit word. Public raw debug output is mixed PCM.
An isolated-wave calibration also maps distinct waveform codes to the same
PCM value. These observations cannot establish exact twelve-bit equality;
the comparison reports that requirement as unverified, never as passed.

The comparison implementation and original register logs remain available.
Existing reSID snapshot payloads remain supported. Built-in snapshots include
all digital latches, analog history, resampling position and queued PCM.

## P10 result

The completed comparison of the digital, nonlinear-filter and coupled-waveform
candidate uses contract 1 and fixture SHA-256
`64dc216cd5d7b80d0bd2fd9c20f70be9e4233432446acac1b4bfe08602ed3c8c`.
No thresholds, gains or time alignment were adjusted to obtain these results.

| Required check | 6581 | 8580 |
|---|---:|---:|
| Logs with every observable digital comparison exact | 3 / 5 | 1 / 5 |
| Exact snapshot continuation | 5 / 5 | 5 / 5 |
| Audio windows within both fixed limits | 0 / 40 | 1 / 40 |
| Complete twelve-bit waveform equality | Unverified | Unverified |

The worst filter-mode relative RMS errors were 2.119 for 6581 and 1.350 for
8580, against limits of 0.25 and 0.10. Envelope/retrigger and combined-waveform
differences also remain. These independent failures prevent accuracy qualification even
apart from the unavailable twelve-bit observation. Model assumptions and their
hardware sources are recorded in [HARDWARE.md](HARDWARE.md).

A subsequent envelope repair preserves a pending direction transition across
consecutive GATE writes. In the fixed retrigger log, 6581 envelope mismatches
fell from 62,217 to 2,168, with guest reads now exact. The 8580 log still has one
envelope timing mismatch. The all-gates qualification result remains failed.

Both runtime engines have exact snapshot continuation. Native built-in checks
play a PSID on 6581 and an RSID on 8580, preserve SID state during debugger peeks,
produce identical captured PCM after restoration, and restore the snapshot's
engine after switching. These are functionality checks, not accuracy qualification.
