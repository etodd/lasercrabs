#pragma once

#include <mutex>
#include <condition_variable>

namespace VI
{

enum SwapType
{
	SwapType_Read,
	SwapType_Write,
	SwapType_count,
};

template<typename T, s32 count = 2>
struct Sync
{
	struct Swapper
	{
		s32 current;
		Sync<T, count>* common;

		Swapper()
			: current(), common()
		{

		}

		T* get()
		{
			return &common->data[current];
		}

		template<SwapType swap_type> void done()
		{
			{
				std::lock_guard<std::mutex> lock(common->mutex[current]);
				common->ready[current][!swap_type] = true;
			}
			common->condition[current].notify_all();
		}

		template<SwapType swap_type> T* next()
		{
			s32 next = (current + 1) % count;
			std::unique_lock<std::mutex> lock(common->mutex[next]);
			while (!common->ready[next][swap_type])
				common->condition[next].wait(lock);
			common->ready[next][swap_type] = false;
			current = next;
			return &common->data[next];
		}

		template<SwapType swap_type> T* swap()
		{
			done<swap_type>();
			return next<swap_type>();
		}
	};

	T data[count];
	b8 ready[count][SwapType_count];
	mutable std::mutex mutex[count];
	std::condition_variable condition[count];

	Sync()
		: data(), ready(), mutex(), condition()
	{
	}

	Swapper swapper(s32 index = 0)
	{
		Swapper swapper;
		swapper.current = index;
		swapper.common = this;
		return swapper;
	}
};

}
