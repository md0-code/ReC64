# Third-party components

| Component | Use | License |
| --- | --- | --- |
| [SDL 2.32.10](https://www.libsdl.org/) | Window, input and sound; fetched at build time and linked statically | zlib |
| [libyaml 0.2.5](https://github.com/yaml/libyaml/tree/0.2.5) | YAML syntax of `rec64.yaml` (`src/third_party/libyaml`); linked statically | MIT, below |

ReC64 itself is licensed under GPL-3.0-or-later ([LICENSE](../LICENSE)).

ReC64 includes no firmware: none is committed to this repository and none is
part of a build. The Commodore images a user supplies retain their owners'
rights; [the notes](NOTES.md#firmware) list their sizes and SHA-256
checksums.

The SingleStepTests/65x02 test vectors used by the checks are MIT-licensed,
downloaded only into `work/cpu/` at a pinned commit with their notice. They
are not program source and are not distributed with ReC64.

## libyaml

```
Copyright (c) 2017-2020 Ingy döt Net
Copyright (c) 2006-2016 Kirill Simonov

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Libretro API

The [Libretro API header](https://github.com/libretro/RetroArch/blob/v1.20.0/libretro-common/include/libretro.h) is from RetroArch v1.20.0.

```
Copyright (C) 2010-2024 The RetroArch team

Permission is hereby granted, free of charge,
to any person obtaining a copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```
