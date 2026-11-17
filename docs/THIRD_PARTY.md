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

Built-in RGB tables use Christopher Jam's published colors for the
color-corrected profile and Pepto's published PAL/NTSC colors for the
automatic default profile. Their pinned data references remain in
`sources.lock.json`. No external VPL files or palette generator are shipped.
