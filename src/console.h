#pragma once

#include "data/entity.h"
#include "data/components.h"
#include "load.h"
#include "physics.h"
#include "render/ui.h"
#include "input.h"

namespace VI
{

struct Console
{
	struct Log
	{
		float timer;
		int length;
		char string[255];
	};
	static Array<char> command;
	static Array<char> debug_buffer;
	static Array<Log> logs;
	static bool visible;
	static UIText text;
	static UIText fps_text;
	static UIText debug_text;
	static UIText log_text;
	static int fps_count;
	static float fps_accumulator;
	static bool fps_visible;
	static char shift_map[127];
	static char normal_map[127];
	static float repeat_start_time;
	static float repeat_last_time;

	static void init();
	static void update(const Update&);
	static void draw(const RenderParams&);
	static void log(const char*, ...);
	static void update_log();
	static void debug(const char*, ...);
};

}
