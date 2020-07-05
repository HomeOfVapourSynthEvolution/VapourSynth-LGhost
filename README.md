Description
===========

Ghost reduction filter. Can be used for removing luminance ghost or edge ghost (ringing).


Usage
=====

    lghost.LGhost(clip clip, int[] mode, int[] shift, int[] intensity[, int planes, int opt=0])

* clip: Clip to process. Any planar format with either integer sample type of 8-16 bit depth or float sample type of 32 bit depth is supported.

* mode: Ghost removal mode.
  * 1 = edge
  * 2 = luminance
  * 3 = rising edge
  * 4 = falling edge

* shift: Width to shift. Must be between `-width` and `width` (exclusive).

* intensity: Strength. Must not be 0 and must be between -128 and 127 (inclusive).

* planes: Sets which planes will be processed. Any unprocessed planes will be simply copied. By default only luma plane is processed for non-RGB formats.

* opt: Sets which cpu optimizations to use.
  * 0 = auto detect
  * 1 = use c
  * 2 = use sse2
  * 3 = use avx2
  * 4 = use avx512

Each ghost consists of individual value from `mode`, `shift` and `intensity`. For example, `lghost.LGhost(mode=[2, 2, 1, 1], shift=[4, 7, -4, -7], intensity=[20, 10, -15, -5])` corresponds to four ghosts. The first ghost is (mode=2, shift=4, intensity=20), the second ghost is (mode=2, shift=7, intensity=10), and so on.


Compilation
===========

```
meson build
ninja -C build
ninja -C build install
```
