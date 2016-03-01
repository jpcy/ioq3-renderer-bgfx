A renderer for [ioquake3](https://github.com/ioquake/ioq3) written in C++ and using [bgfx](https://github.com/bkaradzic/bgfx).

Goal: Make Quake 3 Arena and derivitives look slightly better on a modern PC without content replacement. It doesn't need to run at 600fps, trade some of that performance for visuals.

Status: Work in Progress. Not widely tested. Q3A and TA more or less look how they should.

### Features
* Anti-aliasing - MSAA, SMAA, FXAA
* Soft sprites
* Real dynamic lights
* Extra dynamic lights for Q3A weapons - BFG, Lightning, Plasma, Railgun

## Compiling

### Prerequisites
Clone [bgfx](https://github.com/bkaradzic/bgfx) and [bx](https://github.com/bkaradzic/bx) to the same parent directory as ioq3-renderer-bgfx.

### Linux

```
./premake5 shaders
./premake5 gmake
cd build
make
```

### MinGW

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
r_brightness            |
r_contrast              |
r_dynamicLightIntensity | Make dynamic lights brighter/dimmer.
r_dynamicLightScale     | Scale the radius of dynamic lights.
r_gamma                 |
r_maxAnisotropy         | Enable [anisotropic filtering](https://en.wikipedia.org/wiki/Anisotropic_filtering).
r_saturation            |

### Console Commands

Command         | Description
----------------|------------
screenshotPNG   |
