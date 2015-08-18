#pragma once

#define _AMD64_

#include <stdint.h>
#include "types.h"
#include <GL/glew.h>
#include <SDL.h>
#undef main

namespace VI
{

struct Main
{
	static SDL_Window* window;
	static SDL_GameController* controller;
	static void resize(SDL_Window*, int, int);
	static void get_controller();
	static int proc();
};

}
