# Component and firmware notices

| Component | License | Location |
|---|---|---|
| SDL 2.32.10 | zlib | Downloaded at build time; its `LICENSE.txt` is retained in the dependency tree |
| libyaml 0.2.5 | MIT, Copyright (c) 2006-2016 Kirill Simonov | `src/third_party/libyaml/LICENSE` |

The source locks are in `sources.lock.json`. The CRT presenter uses the
project's M-CRT shader. Program source is GPL-3.0-or-later (`LICENSE`).

ReC64 includes no firmware: none is committed to this repository and none is
part of a build. The Commodore images a user supplies retain their owners'
rights. Their exact byte counts and SHA-256 checksums are listed in the
README; the 1541 image is joined from the 325302-01 lower half and the
901229-05 upper half.
The test screen does not require firmware.

SingleStepTests/65x02 test vectors are MIT-licensed, downloaded only into
`work/cpu/` at the commit in the source lock. Their full notice is downloaded
with them. They are not program source and are not distributed with ReC64.

Built-in RGB tables use Christopher Jam's published colors for the
color-corrected profile and Pepto's published PAL/NTSC colors for the
automatic default profile. Their pinned data references remain in
`sources.lock.json`. No external VPL files or palette generator are shipped.
