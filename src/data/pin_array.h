#pragma once

#include "array.h"

namespace VI
{

template<typename T, s32 size>
struct PinArray
{
	StaticArray<T, size> data;
	u32 mask[(size / sizeof(u32)) + 1];
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
				if (array->is_active(index))
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
			vi_assert(!is_last() && array->is_active(index));
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

	inline b8 is_active(s32 i) const
	{
		s32 index = i / sizeof(u32);
		return mask[index] & (1 << (i - index));
	}

	inline void activate(s32 i)
	{
		s32 index = i / sizeof(u32);
		mask[index] |= (1 << (i - index));
	}

	inline void deactivate(s32 i)
	{
		s32 index = i / sizeof(u32);
		mask[index] &= ~(1 << (i - index));
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
		vi_assert(!is_active(index));
		start = start < index ? start : index;
		end = end > index + 1 ? end : index + 1;
		free_list.remove(free_list.length - 1);
		activate(index);
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
		vi_assert(i >= start && i < end && is_active(i));
		free_list.add(i);
		deactivate(i);
		if (i == end)
		{
			s32 j;
			for (j = i - 1; j > start; j--)
			{
				if (is_active(j))
					break;
			}
			end = j;
		}
		if (i == start)
		{
			s32 j;
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
