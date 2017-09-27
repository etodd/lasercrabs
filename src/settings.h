#pragma once
#include "input.h"
#include "render/render.h"
#include "platform/sock.h"

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

	enum class ShadowQuality : s8 { Off, Medium, High, count };

	// defined in load.cpp
	extern Gamepad gamepads[MAX_GAMEPADS];
	extern s32 framerate_limit;
	extern s32 display_mode_index;
#if SERVER
	extern u64 secret;
	extern u16 port;
	extern char itch_api_key[MAX_AUTH_KEY + 1];
	extern char public_ipv4[NET_MAX_ADDRESS];
	extern char public_ipv6[NET_MAX_ADDRESS];
	extern char gamejolt_api_key[MAX_AUTH_KEY + 1];
#endif
	extern char master_server[MAX_PATH_LENGTH + 1];
	extern char username[MAX_USERNAME + 1];
	extern char gamejolt_username[MAX_PATH_LENGTH + 1];
	extern char gamejolt_token[MAX_AUTH_KEY + 1];
	extern u8 sfx;
	extern u8 music;
	extern ShadowQuality shadow_quality;
	extern Region region;
	extern b8 fullscreen;
	extern b8 vsync;
	extern b8 volumetric_lighting;
	extern b8 antialiasing;
	extern b8 waypoints;
	extern b8 subtitles;
	extern b8 scan_lines;
	extern b8 record;
	extern b8 ssao;
	extern b8 expo;
	extern b8 shell_casings;

	const DisplayMode& display();
};


}
