# VapourSynth-AreaResize

[![Build Status](https://github.com/Kiyamou/VapourSynth-AreaResize/workflows/CI/badge.svg)](https://github.com/Kiyamou/VapourSynth-AreaResize/actions/workflows/CI.yml)

## Description

AreaResize is an area average downscale resizer plugin for VapourSynth. Support 8-16 bit and 32 bit sample type. Support Gray, YUV and RGB color family.

Downscaling in 8-16 bit RGB has additional gamma corrected.

Ported from AviSynth plugins https://github.com/chikuzen/AreaResize and https://github.com/Aktanusa/AreaResize.

## Usage

```python
core.area.AreaResize(clip clip, int width, int height[, float gamma=2.2])
```

* ***clip***
    * Required parameter.
    * Clip to process.
    * Integer sample type of 8-16 bit depth and float sample type of 32 bit depth are supported.
    * Gray, YUV and RGB color family are supported.
* ***width***
    * Required parameter.
    * The width of output.
    * Must be smaller than the width of input.
* ***height***
    * Required parameter.
    * The height of output.
    * Must be smaller than the height of input.
* ***gamma***
    * Optional parameter. *Default: 2.2*
    * Gamma corrected. Only valid for 8-16 bit RGB.
    * For 32 bit RGB, the accuracy is high enough, no gamma correction is needed.

## Features

* Add parameter for gamma corrected.

## TODO List

* Fix bug for special target size
  * For Gray or YUV 8-16 bit, if the target width can't be divisible by 32, output is abnormal
    * I haven't found the reason, so add an exception handling
  * For RGB 8bit, 1920x1080 -> 1200x700, crash

## Compilation

```
x86_64-w64-mingw32-g++ -shared -o AreaResize.dll -O2 -static AreaResize.cpp
```

`VapourSynth.h` and `VSHelper.h` need be in the same folder. You can get them from [here](https://github.com/vapoursynth/vapoursynth/tree/master/include) or your VapourSynth installation directory (`VapourSynth/sdk/include/vapoursynth`).

Make sure the header files used during compilation are the same as those of your VapourSynth installation directory.
