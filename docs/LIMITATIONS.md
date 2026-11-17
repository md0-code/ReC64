# SID engine availability

The release uses the unmodified reSID backend for both 6581 and 8580. The
original SID candidate is a separate comparison target and is not linked
into the emulator or offered as a sound setting.

Admission requires exact per-voice twelve-bit waveform and envelope output,
the fixed filter tolerances, and exact state continuation described in
[SID-COMPARISON.md](SID-COMPARISON.md). All required gates must pass.

The permitted reSID interface exposes eight waveform bits through OSC3,
not the complete twelve-bit word. Public raw debug output is mixed PCM.
An isolated-wave calibration also maps distinct waveform codes to the same
PCM value. These observations cannot establish exact twelve-bit equality;
the comparison reports that requirement as unverified, never as passed.

The comparison implementation and original register logs remain available
for further work. An unqualified candidate does not alter release audio,
saved SID states, or SID-player compatibility.

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
differences also remain. These independent failures exclude the candidate even
apart from the unavailable twelve-bit observation. Model assumptions and their
hardware sources are recorded in [HARDWARE.md](HARDWARE.md).

The release retains reSID and includes the SID-query correction found during
this work: debugger peeks no longer alter the live SID bus. Native PSID and
RSID checks on both chip models preserve machine state and exact PCM snapshot
continuation when these queries are inserted.
