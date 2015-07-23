MK-ZEBRA
========

![MK-ZEBRA](http://i.imgur.com/9hJiC4ql.png)

The engine and most assets for MK-ZEBRA are available under MIT license.
Game code remains closed-source for top secret reasons.

Features
--------

- Simple multi-threaded rendering architecture
- Entity/component system
- Content pipeline supporting GLSL, TTF, PNG, FBX with skinned animation
- Navmesh generation thanks to Recast
- Physics thanks to Bullet
- Complete linear math suite somewhat stolen from Ogre
- Builds on Win/Mac/Linux

Quickstart
----------

First, run `git submodule update --init` to pull down all submodules. Expect
the "game" submodule to fail; it contains private game code. CMake will
generate some template game code for you in the `src/game` folder.

Windows
-------

1. Install [Visual Studio](https://www.visualstudio.com/en-us/downloads/download-visual-studio-vs.aspx)
1. Install [CMake](http://www.cmake.org/download/)
1. Ensure [Blender](http://blender.org) is installed and
   [available on the path](http://www.computerhope.com/issues/ch000549.htm).
1. Run `setup.bat`
1. Open `build/mkzebra.sln` in Visual Studio.
1. Set the `mkzebra` project as the default startup project.
1. Hit F5 to run the game.

Linux
-----
1. Run `./setup`
1. Run `./mkzebra` from the `build` folder.

Mac
---
1. Install [Homebrew](http://brew.sh/)
1. Ensure [Blender](http://blender.org) is installed and
   [available on the path](http://www.computerhope.com/issues/ch000549.htm).
1. Run `./setup-mac`
1. Run `./mkzebra` from the `build` folder.

The MIT License (MIT)
---------------------

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