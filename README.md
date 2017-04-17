## ioq3-renderer-bgfx

[![Appveyor CI Build Status](https://ci.appveyor.com/api/projects/status/github/jpcy/ioq3-renderer-bgfx?branch=master&svg=true)](https://ci.appveyor.com/project/jpcy/ioq3-renderer-bgfx)
[![Travis CI Build Status](https://travis-ci.org/jpcy/ioq3-renderer-bgfx.svg?branch=master)](https://travis-ci.org/jpcy/ioq3-renderer-bgfx)

This is a renderer for [ioquake3](https://github.com/ioquake/ioq3) written in C++ and using [bgfx](https://github.com/bkaradzic/bgfx).

Goal: Make Quake 3 Arena and derivitives look slightly better on a modern PC without content replacement. It doesn't need to run at 600fps, trade some of that performance for visuals.

Status: Work in Progress. Not widely tested. Q3A and TA more or less look how they should.

## Features
* Anti-aliasing - MSAA, SMAA
* Soft sprites
* Real dynamic lights
* Extra dynamic lights for Q3A weapons - BFG, Lightning, Plasma, Railgun
* Bloom

## Screenshots

[![](http://i.imgur.com/86x8FN2.png)](http://i.imgur.com/WHYjbF0.jpg)
[![](http://i.imgur.com/eA2ydm8.png)](http://i.imgur.com/vPhQbMc.jpg)

[![](http://i.imgur.com/1QPNbzr.png)](http://i.imgur.com/LvMyLgB.jpg)
[![](http://i.imgur.com/KkGO5Hc.png)](http://i.imgur.com/ShxFR3o.jpg)

## Binaries

[Windows (x86)](https://dl.bintray.com/jpcy/ioq3-renderer-bgfx/renderer_bgfx_x86.zip)

[Linux (x86_64)](https://dl.bintray.com/jpcy/ioq3-renderer-bgfx/renderer_bgfx_x86_64.tar.gz)

These are updated after every commit.

## Compiling

### Linux

Required packages: libgl1-mesa-dev libsdl2-dev

```
./premake5 shaders
./premake5 gmake
cd build
make
```

### Cygwin/MinGW-w64/MSYS2

Clone [ioquake3](https://github.com/ioquake/ioq3) to the same parent directory as ioq3-renderer-bgfx.

```
./premake5.exe shaders
./premake5.exe gmake
cd build
make
```

### Visual Studio
1. Run `CompileShaders.bat`
2. Use [ioq3-premake-msvc](https://github.com/jpcy/ioq3-premake-msvc).

## Usage

Copy the renderer binaries from `build\bin_*` to where you have a [ioquake3 test build](http://ioquake3.org/get-it/test-builds/) installed.

Select the renderer in-game with `cl_renderer bgfx` followed by `vid_restart`.

### Console Variables

Run the following console variables without any arguments to see a list of possible values.

Variable                | Description
------------------------|------------
r_aa                    | Anti-aliasing.
r_aa_hud                | Anti-aliasing for 3D HUD elements.
r_backend               | Rendering backend - OpenGL, Direct3D 9 etc.
r_bgfx_stats            | Show bgfx statistics.
r_bloom                 | Enable bloom.
r_bloomScale            | Scale the bloom effect.
r_dynamicLightIntensity | Make dynamic lights brighter/dimmer.
r_dynamicLightScale     | Scale the radius of dynamic lights.
r_lerpTextureAnimation  | Use linear interpolation on texture animation - flames, explosions.
r_maxAnisotropy         | Enable [anisotropic filtering](https://en.wikipedia.org/wiki/Anisotropic_filtering).
r_waterReflections      | Show planar water reflections. Only enabled on q3dm2 for now.

### Console Commands

Command         | Description
----------------|------------
screenshotPNG   |
