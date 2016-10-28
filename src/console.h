#pragma once

#include "render/ui.h"

namespace VI
{

struct Update;

struct Console
{
	struct Log
	{
		r32 timer;
		s32 length;
		char string[255];
	};
	static Array<char> command;
	static Array<char> debug_buffer;
	static Array<Log> logs;
	static b8 visible;
	static UIText text;
	static char fps_text[255];
	static UIText debug_text;
	static UIText log_text;
	static s32 fps_count;
	static r32 fps_accumulator;
	static r32 longest_frame_time;
	static b8 fps_visible;
	static char shift_map[127];
	static char normal_map[127];
	static r32 repeat_start_time;
	static r32 repeat_last_time;

	static void init();
	static void update(const Update&);
	static void draw(const RenderParams&);
	static void log(const char*, ...);
	static void update_log();
	static void debug(const char*, ...);
};

}
