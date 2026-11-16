# Component and firmware notices

| Component | License | Location |
|---|---|---|
| SDL 2.32.10 | zlib | Downloaded at build time; its `LICENSE.txt` is retained in the dependency tree |
| libyaml 0.2.5 | MIT, Copyright (c) 2006-2016 Kirill Simonov | `src/third_party/libyaml/LICENSE` |

The source locks are in `sources.lock.json`. The CRT presenter uses the
project's M-CRT shader. Program source is GPL-3.0-or-later (`LICENSE`).

The five Commodore firmware images fetched into the ignored `roms/` folder
retain their original owners' rights. Distribution through VICE does not
relicense them as ReC64 source. No firmware is committed to this repository.
Each image's source, exact byte count and SHA-256 are recorded in the lock.
The 1541 image contains both the 325302-01 lower half and 901229-05 upper half.
The test screen does not require firmware.

SingleStepTests/65x02 test vectors are MIT-licensed, downloaded only into
`work/cpu/` at the commit in the source lock. Their full notice is downloaded
with them. They are not program source and are not distributed with ReC64.

reSID 1.0-pre2 is vendored unchanged from VICE 3.9.0 at the commit and file
hashes in the source lock. Copyright (C) 1998-2010 Dag Lem and the authors
listed in `src/third_party/resid/AUTHORS`; GPL-2.0-or-later. Its complete
licence is in `licenses/reSID-GPL-2`. Only its public `sid.h` API is used by
ReC64. Its original Perl build generator produces waveform headers in the
build tree; no generated header changes a vendored file.

The RGB palette data in `palettes/` retains its original VPL comments:
Christopher Jam, Colodore and Pepto PAL/NTSC, from the pinned VICE data
files recorded in `sources.lock.json`. The built-in tables use these same
published values. No palette generator or emulator implementation is copied.
