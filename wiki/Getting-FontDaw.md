# Getting FontDaw

There is no download yet. This page will say where to get FontDaw once there is one.

## Downloads

**There are no public builds.** FontDaw is at version 0.1.0 and still in early development. We will not link to a download until one exists. There are no installers yet.

When a release exists it will be listed on the [FontDaw releases page](https://github.com/alfonslm/FontDaw/releases).

## System requirements

We have not tested FontDaw on specific system versions, so we cannot give minimum requirements yet. What we can say is what the build scripts aim at:

- Linux on 64-bit Intel/AMD and on 64-bit ARM.
- Windows, 64-bit.
- macOS, on Apple silicon and Intel.

*Stub: the supported versions of each system, the minimum memory and the audio hardware advice will be listed here once the first release is planned.*

## Building from source

**The source is not public yet. These steps are for when it is, and nobody has verified this exact command.** The steps for Linux, in short:

1. Clone the source with its submodules (`git clone --recurse-submodules`).
2. Install a recent C++ compiler: GCC 14 or newer, or Clang 20 or newer.
3. Install CMake 3.28 or newer.
4. Install Qt 6.11 with the modules qt5compat, qtnetworkauth, qtshadertools and qtwebsockets, and make sure `qmake6` or `qmake` is on your path.
5. Install the OpenGL, EGL and xkbcommon development packages (on Debian or Ubuntu: `libgl-dev`, `libegl-dev`, `libxkbcommon-dev`).
6. From the top folder, run `cmake -P build.cmake -DCMAKE_BUILD_TYPE=Release -DMUSE_MODULE_VST=ON`. The build folder appears under `builds/`. The last option switches on VST3 support, which is off by default.

Plan for about 18 GB of free disk space. The source of each release will be attached to that release on the [releases page](https://github.com/alfonslm/FontDaw/releases).

*Stub: steps for Windows and macOS, the full list of Linux packages, and the public place to get the source.*

## Installing next to MuseScore

FontDaw is designed to keep its own name, settings folder, documents folder and program file (`fontdaw`), so that it does not touch MuseScore Studio's. Having both installed on one computer has not been tested yet. More in [FontDaw and MuseScore](FontDaw-and-MuseScore).
