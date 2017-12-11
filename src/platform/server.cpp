#define _AMD64_

#include "types.h"
#include "load.h"

#include <thread>
#include "physics.h"
#include "loop.h"
#include "settings.h"
#if _WIN32
#include <Windows.h>
#endif
#include <time.h>
#include <chrono>
#include <signal.h>

namespace VI
{

	namespace platform
	{

		u64 timestamp()
		{
			time_t t;
			::time(&t);
			return (u64)t;
		}

		r64 time()
		{
			return r64(std::chrono::high_resolution_clock::now().time_since_epoch().count()) / 1000000000.0;
		}

		void sleep(r32 time)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds((s64)(time * 1000.0f)));
		}

		void display_mode(s32 width, s32 height, b8 fullscreen, b8 vsync)
		{
		}

		b8 quit = false;
		void signal_handle(s32)
		{
			quit = true;
		}

	}

	s32 proc(u16 port)
	{
		signal(SIGINT, platform::signal_handle);
		signal(SIGTERM, platform::signal_handle);

		Loader::data_directory = ""; // todo
		{
			Array<DisplayMode> modes;
			modes.add({ 0, 0 });
			Loader::settings_load(modes, { 0, 0 });
		}

		Settings::port = port;

		// launch threads

		Sync<LoopSync> render_sync;

		LoopSwapper update_swapper = render_sync.swapper(0);
		LoopSwapper render_swapper = render_sync.swapper(1);

		Sync<PhysicsSync, 1> physics_sync;

		PhysicsSwapper physics_swapper = physics_sync.swapper();
		PhysicsSwapper physics_update_swapper = physics_sync.swapper();

		std::thread physics_thread(Physics::loop, &physics_swapper);

		std::thread update_thread(Loop::loop, &update_swapper, &physics_update_swapper);

		std::thread ai_thread(AI::loop);

		LoopSync* sync = render_swapper.get();

		while (true)
		{
			sync->quit |= platform::quit;
			b8 quit = sync->quit;

			sync = render_swapper.swap<SwapType_Read>();

			if (quit || sync->quit)
				break;
		}

		AI::quit();

		update_thread.join();
		physics_thread.join();
		ai_thread.join();

		return 0;
	}

}

int main(int argc, char** argv)
{
	int port;

	if (argc >= 2)
		port = atoi(argv[1]);
	else
		port = 21365;

	if (port <= 0 || port > 65535)
	{
		fprintf(stderr, "%s\n", "Invalid port number specified.");
		return -1;
	}

	return VI::proc(port);
}
