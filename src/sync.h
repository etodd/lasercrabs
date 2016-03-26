#pragma once

#include <mutex>
#include <condition_variable>
#include "data/array.h"

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

struct SyncBuffer
{
	Array<u8> queue;
	s32 read_pos;

	SyncBuffer()
		: queue(), read_pos()
	{
	}

	// IMPORTANT: don't do this:
	// T t; write(&t);
	// It will resolve to write<T*> rather than write<T>, so you'll get the wrong size.
	// Use write(t).

	template<typename T>
	void write(const T& data)
	{
		write(&data);
	}

	template<typename T>
	void write(const T* data, const s32 count = 1)
	{
		T* destination = alloc<T>(count);
		memcpy((void*)destination, data, sizeof(T) * count);
	}

	template<typename T>
	T* alloc(const s32 count = 1)
	{
		s32 pos = queue.length;
		queue.resize(pos + sizeof(T) * count);
		return (T*)(queue.data + pos);
	}

	template<typename T>
	const T* read(s32 count = 1)
	{
		T* result = (T*)(queue.data + read_pos);
		read_pos += sizeof(T) * count;
		return result;
	}
};

}