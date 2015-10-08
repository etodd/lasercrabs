#pragma once

#define _AMD64_

#include <stdint.h>
#include "types.h"
#include <GL/glew.h>
#include <SDL.h>
#include "input.h"
#undef main

namespace VI
{

struct Main
{
	static SDL_Window* window;
	static SDL_GameController* controllers[MAX_GAMEPADS];
	static void resize(SDL_Window*, int, int);
	static void refresh_controllers();
	static int proc();
};

}
