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

		double time()
		{
			return (r64)std::chrono::high_resolution_clock::now().time_since_epoch().count() / 1000000000.0;
		}

		void sleep(float time)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds((s64)(time * 1000.0f)));
		}

	}

	s32 proc()
	{
		// Launch threads

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

		r64 last_time = platform::time();

		while (true)
		{
			r64 time = platform::time();
			sync->time.total = (r32)time;
			sync->time.delta = vi_min((r32)(time - last_time), 0.25f);
			last_time = time;

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
	return VI::proc();
}
