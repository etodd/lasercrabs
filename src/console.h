#pragma once

#include "render/ui.h"
#include "input.h"

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
	static TextField field;
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

	static void init();
	static void update(const Update&);
	static void draw_ui(const RenderParams&);
	static void log(const char*, ...);
	static void update_log();
	static void debug(const char*, ...);
};


}
