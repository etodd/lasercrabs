#pragma once

#include "array.h"

namespace VI
{

template<typename T, int size>
struct PinArray
{
	StaticArray<T, size> data;
	unsigned int mask[size / sizeof(unsigned int)];
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
				if (array->is_active(index))
					return;
				index++;
			}
		}

		inline bool is_last() const
		{
			return index >= array->end;
		}

		inline T* item() const
		{
			vi_assert(!is_last() && array->is_active(index));
			return &array->data[index];
		}
	};

	PinArray()
		: data(), mask(), start(size), end(0), free_list()
	{
		data.length = size;
		free_list.length = size;
		for (int i = 0; i < size; i++)
			free_list[i] = (size - 1) - i;
	}

	inline bool is_active(int i) const
	{
		int index = i / sizeof(unsigned int);
		return mask[index] & (1 << (i - index));
	}

	inline void activate(int i)
	{
		int index = i / sizeof(unsigned int);
		mask[index] |= (1 << (i - index));
	}

	inline void deactivate(int i)
	{
		int index = i / sizeof(unsigned int);
		mask[index] &= ~(1 << (i - index));
	}

	inline int count() const
	{
		return size - free_list.length;
	}

	inline T operator [] (const int i) const
	{
		vi_assert(i >= 0 && i < size);
		return data.data[i];
	}

	inline T& operator [] (const int i)
	{
		vi_assert(i >= 0 && i < size);
		return data.data[i];
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
		vi_assert(!is_active(index));
		start = start < index ? start : index;
		end = end > index + 1 ? end : index + 1;
		free_list.remove(free_list.length - 1);
		activate(index);
		return &data[index];
	}

	int add(const T& t)
	{
		T* i = add();
		*i = t;
		return ((char*)i - (char*)&data[0]) / sizeof(T);
	}

	void remove(int i)
	{
		vi_assert(i >= start && i < end && is_active(i));
		free_list.add(i);
		deactivate(i);
		if (i == end)
		{
			int j;
			for (j = i - 1; j > start; j--)
			{
				if (is_active(j))
					break;
			}
			end = j;
		}
		if (i == start)
		{
			int j;
			for (j = i + 1; j < end; j++)
			{
				if (is_active(j))
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