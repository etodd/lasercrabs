#pragma once

#include "array.h"

namespace VI
{

template<s32 size> struct Bitmask
{
	u32 data[(size / sizeof(u32)) + 1];

	inline b8 get(s32 i) const
	{
		s32 index = i / sizeof(u32);
		return data[index] & (1 << (i - index));
	}

	inline void set(s32 i, b8 value)
	{
		s32 index = i / sizeof(u32);
		if (value)
			data[index] |= (1 << (i - index));
		else
			data[index] &= ~(1 << (i - index));
	}
};

template<typename T, s32 size>
struct PinArray
{
	StaticArray<T, size> data;
	Bitmask<size> mask;
	StaticArray<s32, size> free_list;
	s32 start;
	s32 end;

	struct Iterator
	{
		s32 index;
		PinArray<T, size>* array;
		void next()
		{
			index++;
			while (index < array->end)
			{
				if (array->active(index))
					return;
				index++;
			}
		}

		inline b8 is_last() const
		{
			return index >= array->end;
		}

		inline T* item() const
		{
			vi_assert(!is_last() && array->active(index));
			return &array->data[index];
		}
	};

	PinArray()
		: data(), mask(), start(size), end(0), free_list()
	{
		data.length = size;
		free_list.length = size;
		for (s32 i = 0; i < size; i++)
			free_list[i] = (size - 1) - i;
	}

	inline b8 active(s32 i) const
	{
		return mask.get(i);
	}

	inline void active(s32 i, b8 value)
	{
		mask.set(i, value);
	}

	inline s32 count() const
	{
		return size - free_list.length;
	}

	inline T operator [] (const s32 i) const
	{
		vi_assert(i >= 0 && i < size);
		return data.data[i];
	}

	inline T& operator [] (const s32 i)
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
		s32 index = free_list[free_list.length - 1];
		vi_assert(!active(index));
		start = start < index ? start : index;
		end = end > index + 1 ? end : index + 1;
		free_list.remove(free_list.length - 1);
		active(index, true);
		return &data[index];
	}

	s32 add(const T& t)
	{
		T* i = add();
		*i = t;
		return ((char*)i - (char*)&data[0]) / sizeof(T);
	}

	void remove(s32 i)
	{
		vi_assert(i >= start && i < end && active(i));
		free_list.add(i);
		active(i, false);
		if (i == end)
		{
			s32 j;
			for (j = i - 1; j > start; j--)
			{
				if (active(j))
					break;
			}
			end = j;
		}
		if (i == start)
		{
			s32 j;
			for (j = i + 1; j < end; j++)
			{
				if (active(j))
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
