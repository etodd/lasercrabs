#pragma once

#include "array.h"

namespace VI
{

template<typename T> struct PinArrayEntry
{
	bool active;
	T item;
};

template<typename T>
struct PinArray
{
	Array<PinArrayEntry<T>> data;
	Array<int> free_list;
	int start;
	int end;

	struct Entry
	{
		int index;
		T* item;
	};

	struct Iterator
	{
		int index;
		PinArray<T>* array;
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

		T* item()
		{
			if (is_last())
				return 0;
			else
				return &array->data[index].item;
		}
	};

	PinArray()
		: data(), free_list(), start(), end()
	{

	}

	PinArray(int size)
		: data(size, size), free_list(size, size), start(size), end(0)
	{
		for (int i = 0; i < size; i++)
			free_list[i] = (size - 1) - i;
	}

	void resize(int size)
	{
		int old_size = data.length;
		data.resize(size);
		free_list.reserve(size);
		for (int i = old_size; i < size; i++)
			free_list.add((size - 1) - i);
	}

	inline T operator [] (const int i) const
	{
		vi_assert(i >= 0 && i < data.length);
		return (data.data + i)->item;
	}

	inline T& operator [] (const int i)
	{
		vi_assert(i >= 0 && i < data.length);
		return (data.data + i)->item;
	}

	T* get(const int i)
	{
		vi_assert(i >= 0 && i < data.length);
		PinArrayEntry<T>& e = *(data.data + i);
		if (e.active)
			return &e.item;
		else
			return 0;
	}

	Entry add()
	{
		vi_assert(free_list.length > 0);
		Entry e;
		e.index = free_list[free_list.length - 1];
		start = start < e.index ? start : e.index;
		end = end > e.index + 1 ? end : e.index + 1;
		free_list.remove(free_list.length - 1);
		data[e.index].active = true;
		e.item = &data[e.index].item;
		return e;
	}

	int count()
	{
		return data.length - free_list.length;
	}

	Entry add(T& t)
	{
		Entry i = add();
		*(i.item) = t;
		return i;
	}

	Iterator iterator()
	{
		Iterator i;
		i.index = start;
		i.array = this;
		return i;
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
			start = data.length;
			end = 0;
		}
	}
};

}