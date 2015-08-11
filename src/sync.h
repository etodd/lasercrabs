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

template<typename T, int count>
struct Swapper
{
	T* data;
	int current;
	T* get()
	{
		return &data[current];
	}
	template<SwapType swap_type> T* swap()
	{
		{
			std::lock_guard<std::mutex> lock(data[current].mutex);
			data[current].ready[!swap_type] = true;
		}
		data[current].condition.notify_all();

		int next = (current + 1) % count;
		std::unique_lock<std::mutex> lock(data[next].mutex);
		while (!data[next].ready[swap_type])
			data[next].condition.wait(lock);
		data[next].ready[swap_type] = false;
		current = next;
		return &data[next];
	}
};

}
