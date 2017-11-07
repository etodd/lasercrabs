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
		InputBinding bindings[s32(Controls::count)];
		u16 sensitivity_gamepad;
		u16 sensitivity_mouse;
		b8 invert_y;
		b8 zoom_toggle;
		b8 rumble;

		r32 effective_sensitivity_gamepad() const
		{
			return r32(sensitivity_gamepad) * 0.01f;
		}

		r32 effective_sensitivity_mouse() const
		{
			return r32(sensitivity_mouse) * 0.01f;
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
	extern char public_ipv4[NET_MAX_ADDRESS];
	extern char public_ipv6[NET_MAX_ADDRESS];
	extern char gamejolt_api_key[MAX_AUTH_KEY + 1];
#endif
	extern char itch_api_key[MAX_AUTH_KEY + 1];
	extern char master_server[MAX_PATH_LENGTH + 1];
	extern char username[MAX_USERNAME + 1];
	extern char gamejolt_username[MAX_PATH_LENGTH + 1];
	extern char gamejolt_token[MAX_AUTH_KEY + 1];
	extern u8 sfx;
	extern u8 music;
	extern u8 fov;

	inline r32 effective_fov()
	{
		return r32(Settings::fov) * PI * 0.5f / 180.0f;
	}

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
	extern b8 god_mode;

	const DisplayMode& display();
};


}
