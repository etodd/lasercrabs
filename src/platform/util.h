#pragma once
#include <sdl/include/SDL.h>
#include <time.h>

namespace VI
{


namespace platform
{

inline u64 timestamp()
{
	time_t t;
	::time(&t);
	return (u64)t;
}

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