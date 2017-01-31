#define _AMD64_

#include "game/master.h"

#include <thread>
#if _WIN32
#include <Windows.h>
#endif
#include <time.h>
#include <chrono>
#include "sock.h"
#include "net_serialize.h"

namespace VI
{

	namespace platform
	{

		u64 timestamp()
		{
			time_t t;
			::time(&t);
			return u64(t);
		}

		r64 time()
		{
			return r64(std::chrono::high_resolution_clock::now().time_since_epoch().count()) / 1000000000.0;
		}

		void sleep(r32 time)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(s64(time * 1000.0f)));
		}

	}

	s32 proc()
	{
		return 0;
	}

}

int main(int argc, char** argv)
{
	return VI::proc();
}
