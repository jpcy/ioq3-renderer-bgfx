A renderer for [ioquake3](https://github.com/ioquake/ioq3) written in C++ and using [bgfx](https://github.com/bkaradzic/bgfx) to support multiple rendering APIs.

## Compiling - Prerequisites
Checkout [bgfx](https://github.com/bkaradzic/bgfx) and [bx](https://github.com/bkaradzic/bx) to the same parent directory as ioq3-renderer-bgfx.

## Compiling - Visual Studio
Use [ioq3-premake-msvc](https://github.com/jpcy/ioq3-premake-msvc).

## Compiling - MinGW

1. Checkout [ioquake3](https://github.com/ioquake/ioq3) to the same parent directory as ioq3-renderer-bgfx.
2. Run `./premake5.exe gmake`.
3. Enter the `build` directory and run `make help` for build options.

## Usage

Copy the renderer binaries from `build\bin_*` to where you have a [ioquake3 test build](http://ioquake3.org/get-it/test-builds/) installed.

Select the renderer in-game with `cl_renderer bgfx` followed by `vid_restart`.

Console Variable | Description
-----------------| ----------------------------------------------
r_backend        | Rendering backend - OpenGL, Direct3D 9 etc.
r_maxAnisotropy  | Enable [anisotropic filtering](https://en.wikipedia.org/wiki/Anisotropic_filtering).
r_msaa           | Multisample anti-aliasing. 2x, 4x, 8x, or 16x.
r_bgfx_stats     | Show bgfx statistics.
