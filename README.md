Deceiver
========

Features
--------

- [Simple multi-threaded architecture](http://etodd.io/2016/01/12/poor-mans-threading-architecture/) -
separate threads for physics, rendering, AI, and game logic
- Entity/component system
- Linear math suite somewhat stolen from Ogre
- Content pipeline supporting .blend, .glsl, .otf, .png
- Create levels, models, animations, and ragdoll rigs in Blender
- Deferred rendering, cascaded shadow maps, edge detection, bloom, SSAO, clouds
- Geometry-based UI system
- UDP client-server network architecture with master server
- Localization support
- Keyboard/mouse/gamepad support via SDL
- Automatic navmesh generation with realtime mutations via Recast
- Audio via Wwise
- Physics via Bullet
- Almost all dependencies are open source GitHub submodules
- Builds and runs on Win/Mac/Linux

Is this good code?
------------------

It's alright. It uses templates too often. Otherwise not too bad.

Windows quickstart
------------------

1. Clone the repository and pull down the submodules. Don't worry about the `src/platforms/ps` submodule. You will need [Git LFS](https://git-lfs.github.com/).
2. Install [Visual Studio](https://www.visualstudio.com/en-us/downloads/download-visual-studio-vs.aspx)
3. Install the [DirectX SDK](https://www.microsoft.com/en-us/download/confirmation.aspx?id=6812)
4. Install [CMake](http://www.cmake.org/download/)
5. Ensure [Blender](http://blender.org) is installed and
   [available on the path](http://www.computerhope.com/issues/ch000549.htm)
6. Ensure [Wwise](https://www.audiokinetic.com/) is installed and `WwiseCLI`
is available on the path
7. Run `setup-win.bat`
8. Open `build/deceiver.sln` in Visual Studio
9. Set the `deceiver` project as the default startup project
10. Hit F5 to run the game

Linux quickstart
----------------

1. Clone the repository and pull down the submodules. Don't worry about the `src/platforms/ps` submodule. You will need [Git LFS](https://git-lfs.github.com/).
2. The Wwise authoring tool doesn't run on Linux. Use Wwise on another platform
to build the Linux soundbanks.
3. If necessary, copy the generated `Wwise_IDs.h` file into `src/asset`
4. Copy the soundbanks into `assets/audio/GeneratedSoundBanks/Linux`
5. If you are on Debian/Ubuntu, run `./setup-debian-deps`. Otherwise you'll need to install these dependencies yourself.
6. Run `./setup-linux`
7. Run `./deceiver` from the `build` folder

Mac quickstart
--------------

1. Clone the repository and pull down the submodules. Don't worry about the `src/platforms/ps` submodule. You will need [Git LFS](https://git-lfs.github.com/).
2. Install [Homebrew](http://brew.sh/)
3. Ensure [Blender](http://blender.org) is installed and
   [available on the path](http://www.computerhope.com/issues/ch000549.htm)
4. Ensure [Wwise](https://www.audiokinetic.com/) is installed and `WwiseCLI.sh`
is available on the path
5. Wwise might have trouble generating soundbanks the first time. You might
need to delete any cache files and open the project manually in Wwise first.
6. Run `./setup-mac`
7. Run `./deceiver` from the `build` folder

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
