#pragma once
#include "input.h"

namespace VI
{

namespace Settings
{
	struct Gamepad
	{
		InputBinding bindings[(s32)Controls::count];
		u8 sensitivity;
		b8 invert_y;
		b8 zoom_toggle;
		b8 rumble;
		r32 effective_sensitivity() const
		{
			return r32(sensitivity) * 0.01f;
		}
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
	extern b8 antialiasing;
	extern b8 waypoints;
	extern b8 subtitles;
	extern b8 scan_lines;
	extern b8 record;
	extern b8 ssao;
	extern char master_server[MAX_PATH_LENGTH];
	extern s32 secret;
};


}
