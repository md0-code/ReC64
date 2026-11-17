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
