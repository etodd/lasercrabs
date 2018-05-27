LASERCRABS
==========

Features
--------

- [Simple multi-threaded architecture](http://etodd.io/2016/01/12/poor-mans-threading-architecture/) -
separate threads for physics, rendering, AI, and game logic
- Entity/component system
- Linear math suite somewhat stolen from Ogre
- Content pipeline supporting .blend, .glsl, .otf, .png
- Create levels, models, animations, and ragdoll rigs in Blender
- Deferred rendering, cascaded shadow maps, edge detection, bloom, SSAO, weather effects
- Geometry-based font rendering and UI system
- [Online multiplayer](http://etodd.io/2018/02/20/poor-mans-netcode/)
- Online user profile system via [sqlite](https://sqlite.org/)
- Authentication via [Steam](http://store.steampowered.com/), [itch.io](https://itch.io), or [GameJolt](https://gamejolt.com)
- Localization support
- Simple Windows crash report system
- Keyboard/mouse/gamepad support via SDL
- Automatic navmesh generation with realtime mutations via Recast
- Audio via Wwise
- Physics via Bullet
- Almost all dependencies are open source GitHub submodules
- Builds and runs on Win/Mac/Linux

Windows quickstart
------------------

1. Clone the repository and pull down the submodules. You will need [Git LFS](https://git-lfs.github.com/).
2. Install [Visual Studio](https://www.visualstudio.com/en-us/downloads/download-visual-studio-vs.aspx) 2017.
3. Install the [DirectX SDK](https://www.microsoft.com/en-us/download/confirmation.aspx?id=6812)
4. Install [CMake](http://www.cmake.org/download/)
5. Ensure [Blender](http://blender.org) 2.79b is installed and
   [available on the path](http://www.computerhope.com/issues/ch000549.htm)
6. Ensure [Wwise](https://www.audiokinetic.com/) 2017.1.0 build 6302 is installed and `WwiseCLI`
is available on the path
7. Run `setup-win.bat`
8. Open `build/lasercrabs.sln` in Visual Studio
9. Set the `lasercrabs` project to start up by default
10. Hit F5 to run the game

Linux quickstart
----------------

1. Clone the repository and pull down the submodules. You will need [Git LFS](https://git-lfs.github.com/).
2. The Wwise authoring tool doesn't run on Linux. Use Wwise on another platform
to build the Linux soundbanks.
3. If necessary, copy the generated `Wwise_IDs.h` file into `src/asset`
4. Copy the soundbanks into `assets/audio/GeneratedSoundBanks/Linux`
5. If you are on Debian/Ubuntu, run `./setup-debian-deps`. Otherwise you'll need to install these dependencies yourself.
6. Run `./setup-linux`
7. Run `./lasercrabs` from the `build` folder

Mac quickstart
--------------

1. Clone the repository and pull down the submodules. You will need [Git LFS](https://git-lfs.github.com/).
2. Install [Homebrew](http://brew.sh/)
3. Ensure [Blender](http://blender.org) 2.79b is installed and
   [available on the path](http://www.computerhope.com/issues/ch000549.htm)
4. Ensure [Wwise](https://www.audiokinetic.com/) 2017.1.0 build 6302 is installed and `WwiseCLI.sh`
is available on the path
5. Wwise might have trouble generating soundbanks the first time. You might
need to delete any cache files and open the project manually in Wwise first.
6. Run `./setup-mac`
7. Run `./lasercrabs` from the `build` folder

Credits
-------

- Evan Todd - code, design, art
- Jack Menhorn - sound design
- Logan Hayes - music
- Ian Cuslidge - level design for "Plaza"
- Bobpoblo - level design for "Crossing"

License
-------

Everything excluding the `external` directory is public domain.

Wwise SDK
---------

This repository includes a copy of the Wwise SDK. Use of the SDK is subject to
the [Wwise EULA](external/wwise/LICENSE.txt). Wwise is free for educational and
non-commercial purposes. Otherwise, acquire a license key from
[Audiokinetic](https://www.audiokinetic.com/).
