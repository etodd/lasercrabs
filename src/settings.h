#pragma once
#include "input.h"

namespace VI
{

namespace Settings
{
	struct Gamepad
	{
		InputBinding bindings[(s32)Controls::count];
		r32 sensitivity;
		b8 invert;
	};

	enum ShadowQuality { Off, Medium, High, count };

	// defined in load.cpp
	extern Gamepad gamepads[MAX_GAMEPADS];
	extern s32 width;
	extern s32 height;
	extern b8 fullscreen;
	extern b8 vsync;
	extern u8 sfx;
	extern u8 music;
	extern s32 framerate_limit;
	extern ShadowQuality shadow_quality;
	extern b8 volumetric_lighting;
};


}
