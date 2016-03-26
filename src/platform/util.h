#pragma once
#include <sdl/include/SDL.h>

inline double platform_time()
{
	return (SDL_GetTicks() / 1000.0);
}

void platform_sleep(float time)
{
	SDL_Delay(time * 1000.0f);
}