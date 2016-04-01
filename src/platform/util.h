#pragma once
#include <sdl/include/SDL.h>

namespace VI
{


namespace platform
{
inline double time()
{
	return (SDL_GetTicks() / 1000.0);
}

void sleep(float time)
{
	SDL_Delay(time * 1000.0f);
}
}


}