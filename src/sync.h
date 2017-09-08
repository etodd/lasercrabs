#pragma once

#include <mutex>
#include <condition_variable>
#include "data/array.h"
#include <thread>
#include "platform/util.h"

namespace VI
{

template<s32 size> struct SyncRingBuffer
{
	s32 read_pos;
	s32 write_pos;
	mutable std::mutex mutex;
	Array<u8> data;
	std::condition_variable condition;

	SyncRingBuffer() :
		read_pos(),
		write_pos(),
		mutex(),
		data(size, size),
		condition()
	{
	}

	void lock_wait_read()
	{
		while (true)
		{
			mutex.lock();
			b8 can_read = read_pos != write_pos;
			if (can_read)
				break;
			else
			{
				mutex.unlock();
				platform::sleep(1.0f / 60.0f);
			}
		}
	}

	inline b8 can_read() const
	{
		return read_pos != write_pos;
	}

	inline void lock()
	{
		mutex.lock();
	}

	inline void unlock()
	{
		mutex.unlock();
	}

	template<typename T> void write(const T* t, s32 count)
	{
		s32 write_size = sizeof(T) * count;
		s32 write_end = write_pos + write_size;

		if (read_pos < write_pos)
			vi_assert(write_end - data.length < read_pos);
		else if (read_pos > write_pos)
			vi_assert(write_end < read_pos);

#if defined(__clang__)
		// get ready to do gross things
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdynamic-class-memaccess"
#endif
		if (write_end < data.length)
		{
			memcpy(&data[write_pos], t, write_size);
			write_pos = write_end;
		}
		else
		{
			s32 partition = data.length - write_pos;
			memcpy(&data[write_pos], t, partition);
			write_pos = write_end - data.length;
			memcpy(&data[0], ((u8*)t) + partition, write_pos);
		}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
	}

	template<typename T> void write(const T& t)
	{
		write<T>(&t, 1);
	}

	template<typename T> void read(T* t, s32 count = 1)
	{
		s32 read_len = sizeof(T) * count;
		if (read_len == 0)
			return;
		s32 read_end = read_pos + read_len;

#if defined(__clang__)
		// get ready to do gross things
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdynamic-class-memaccess"
#endif
		if (read_end >= data.length)
		{
			vi_assert(write_pos < read_pos);
			s32 read_partition = data.length - read_pos;
			vi_assert(read_len - read_partition <= write_pos);

			memcpy(t, &data[read_pos], read_partition);
			read_pos = read_len - read_partition;
			memcpy(((u8*)t) + read_partition, &data[0], read_pos);
		}
		else
		{
			vi_assert(read_end <= write_pos == read_pos < write_pos);
			memcpy(t, &data[read_pos], read_len);
			read_pos = read_end;
		}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
	}

	s32 length()
	{
		if (read_pos <= write_pos)
			return write_pos - read_pos;
		else
			return write_pos + data.length - read_pos;
	}

	s32 capacity()
	{
		return data.length;
	}
};

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

	template<typename T>
	T* alloc(const s32 count = 1)
	{
		s32 pos = queue.length;
		queue.resize(pos + sizeof(T) * count);
		return (T*)(queue.data + pos);
	}

	template<typename T>
	void write(const T* data, const s32 count)
	{
		T* destination = alloc<T>(count);
		memcpy((void*)destination, data, sizeof(T) * count);
	}

	template<typename T>
	void write(const T& data)
	{
		T* destination = alloc<T>();
		memcpy((void*)destination, &data, sizeof(T));
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
