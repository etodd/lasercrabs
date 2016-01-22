MK-ZEBRA (codename)
===================

Features
--------

- Simple multi-threaded architecture - one thread for physics, one for rendering,
one for game logic
- Entity/component system
- Linear math suite somewhat stolen from Ogre
- Content pipeline supporting .blend, .glsl, .ttf, .png
- Create levels, models, animations, and ragdoll rigs in Blender
- Deferred rendering, cascaded shadow maps, edge detection, film grain, bloom,
SSAO
- Geometry-based UI system
- Keyboard/mouse/gamepad support thanks to SDL
- Automatic navmesh generation thanks to Recast
- Physics thanks to Bullet
- Almost all dependencies are GitHub submodules
- Builds and runs on Win/Mac/Linux

Windows quickstart
------------------

1. Install [Visual Studio](https://www.visualstudio.com/en-us/downloads/download-visual-studio-vs.aspx)
1. Install the [DirectX SDK](https://www.microsoft.com/en-us/download/confirmation.aspx?id=6812)
1. Install [CMake](http://www.cmake.org/download/)
1. Ensure [Blender](http://blender.org) is installed and
   [available on the path](http://www.computerhope.com/issues/ch000549.htm).
1. Ensure [Wwise](https://www.audiokinetic.com/) is installed and `WwiseCLI`
is available on the path.
1. Run `setup.bat`
1. Open `build/mkzebra.sln` in Visual Studio.
1. Set the `mkzebra` project as the default startup project.
1. Hit F5 to run the game.

Linux quickstart
----------------

1. The Wwise authoring tool doesn't run on Linux. Use Wwise on another platform
to build the Linux soundbanks.
1. If necessary, copy the generated `Wwise_IDs.h` file into `src/asset`.
1. Copy the soundbanks into `assets/audio/GeneratedSoundBanks/Linux`.
1. Run `./setup`
1. Run `./mkzebra` from the `build` folder.

Mac quickstart
--------------

1. Install [Homebrew](http://brew.sh/)
1. Ensure [Blender](http://blender.org) is installed and
   [available on the path](http://www.computerhope.com/issues/ch000549.htm).
1. Ensure [Wwise](https://www.audiokinetic.com/) is installed and `WwiseCLI.sh`
is available on the path.
1. Run `./setup-mac`
1. Run `./mkzebra` from the `build` folder.

Asset license
-------------

All rights reserved for images, sounds, music, models, animations, fonts,
levels, dialogue files, and all other game data. They are available for
download here for convenience, but may not be redistributed without express
permission.

All code not including the `external` directory is available under the
following license:

The MIT License
---------------

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Wwise SDK
---------

This repository includes a copy of the Wwise SDK. Use of the SDK is subject to
the [Wwise EULA](external/wwise/LICENSE.txt). Wwise is free for educational and
non-commercial purposes. Otherwise, acquire a license key from
[Audiokinetic](https://www.audiokinetic.com/).
