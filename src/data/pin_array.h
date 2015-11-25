#pragma once

#include "array.h"

namespace VI
{

template<typename T> struct PinArrayEntry
{
	bool active;
	T item;
	PinArrayEntry() : active() {}
};

template<typename T, int size>
struct PinArray
{
	StaticArray<PinArrayEntry<T>, size> data;
	StaticArray<int, size> free_list;
	int start;
	int end;

	struct Iterator
	{
		int index;
		PinArray<T, size>* array;
		void next()
		{
			index++;
			while (index < array->end)
			{
				if (array->data[index].active)
					return;
				index++;
			}
		}

		bool is_last()
		{
			return index >= array->end;
		}

		inline T* item()
		{
			vi_assert(!is_last());
			return &array->data[index].item;
		}
	};

	PinArray()
		: data(), start(size), end(0), free_list()
	{
		data.length = size;
		free_list.length = size;
		for (int i = 0; i < size; i++)
			free_list[i] = (size - 1) - i;
	}

	int count() const
	{
		return size - free_list.length;
	}

	inline T operator [] (const int i) const
	{
		vi_assert(i >= 0 && i < size);
		return (data.data + i)->item;
	}

	inline T& operator [] (const int i)
	{
		vi_assert(i >= 0 && i < size);
		return (data.data + i)->item;
	}

	Iterator iterator()
	{
		Iterator i;
		i.index = start;
		i.array = this;
		return i;
	}

	T* add()
	{
		vi_assert(free_list.length > 0);
		int index = free_list[free_list.length - 1];
		vi_assert(!data[index].active);
		start = start < index ? start : index;
		end = end > index + 1 ? end : index + 1;
		free_list.remove(free_list.length - 1);
		data[index].active = true;
		return &data[index].item;
	}

	int add(const T& t)
	{
		T* i = add();
		*i = t;
		return ((char*)i - (char*)&data[0]) / sizeof(PinArrayEntry<T>);
	}

	void remove(int i)
	{
		vi_assert(i >= start && i < end && data[i].active);
		free_list.add(i);
		data[i].active = false;
		if (i == end)
		{
			int j;
			for (j = i - 1; j > start; j--)
			{
				if (data[j].active)
					break;
			}
			end = j;
		}
		if (i == start)
		{
			int j;
			for (j = i + 1; j < end; j++)
			{
				if (data[j].active)
					break;
			}
			start = j;
		}
		if (start >= end)
		{
			start = size;
			end = 0;
		}
	}
};

}