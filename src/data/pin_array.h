#pragma once

#include "array.h"

namespace VI
{

namespace BitUtility
{
	static inline u32 popcount(u32 x)
	{
#ifdef __GNUC__
		return __builtin_popcount(x);
#else // #ifdef __GNUC__
		const u32 a = x - ((x >> 1) & 0x55555555);
		const u32 b = (((a >> 2) & 0x33333333) + (a & 0x33333333));
		const u32 c = (((b >> 4) + b) & 0x0f0f0f0f);
		const u32 d = c + (c >> 8);
		const u32 e = d + (d >> 16);
		const u32 result = e & 0x0000003f;
		return result;
#endif // #ifdef __GNUC__
	}
}

template<s16 size> struct Bitmask
{
	u32 data[(size / (sizeof(u32) * 8)) + (size % (sizeof(u32) * 8) == 0 ? 0 : 1)];
	s16 start;
	s16 end;

	Bitmask()
		: start(size), end(), data()
	{
	}

	inline b8 get(s32 i) const
	{
		vi_assert(i >= 0 && i < size);
		s32 index = i / (sizeof(u32) * 8);
		return data[index] & (1 << (i - (index * sizeof(u32) * 8)));
	}

	inline b8 any() const
	{
		return start < end;
	}

	s16 count() const
	{
		if (start < end)
		{
			s32 total = 0;
			s32 start_index = start / (sizeof(u32) * 8);
			s32 end_index = ((end - 1) / (sizeof(u32) * 8)) + 1;
			for (s32 i = start_index; i < end_index; i++)
				total += BitUtility::popcount(data[i]);
			return total;
		}
		else
			return 0;
	}

	inline s32 next(s32 i) const
	{
		i++;
		while (i < end)
		{
			if (get(i))
				break;
			i++;
		}
		return i;
	}

	inline s32 prev(s32 i) const
	{
		i--;
		while (i >= start)
		{
			if (get(i))
				break;
			i--;
		}
		return i;
	}

	void clear()
	{
		start = size;
		end = 0;
		memset(data, 0, sizeof(data));
	}

	void set(s32 i, b8 value)
	{
		vi_assert(i >= 0 && i < size);
		s32 index = i / (sizeof(u32) * 8);
		u32 mask = 1 << (i - (index * sizeof(u32) * 8));
		if (value)
		{
			data[index] |= mask;
			start = start < i ? start : i;
			end = end > i + 1 ? end : s16(i + 1);
		}
		else
		{
			data[index] &= ~mask;

			if (i + 1 == end)
			{
				s32 j;
				for (j = i - 1; j > start; j--)
				{
					if (get(j))
						break;
				}
				end = s16(j + 1);
			}
			if (i == start)
			{
				s32 j;
				for (j = i + 1; j < end; j++)
				{
					if (get(j))
						break;
				}
				start = s16(j);
			}
			if (start >= end)
			{
				start = size;
				end = 0;
			}
		}
	}

	void add(const Bitmask<size>& other)
	{
		start = vi_min(start, other.start);
		end = vi_max(end, other.end);
		s32 start_index = start / (sizeof(u32) * 8);
		s32 end_index = ((end - 1) / (sizeof(u32) * 8)) + 1;
		for (s32 i = start_index; i < end_index; i++)
			data[i] |= other.data[i];
	}

	void subtract(const Bitmask<size>& other)
	{
		for (s32 i = other.start; i < other.end; i++)
		{
			if (other.get(i))
				set(i, false);
		}
	}
};

template<typename T, s16 size>
struct PinArray
{
	StaticArray<T, size> data;
	Bitmask<size> mask;
	StaticArray<ID, size> free_list;

	struct Iterator
	{
		PinArray<T, size>* array;
		ID index;

		inline void next()
		{
			index = array->mask.next(index);
		}

		inline void prev()
		{
			index = array->mask.prev(index);
		}

		inline b8 is_last() const
		{
			return index >= array->mask.end;
		}

		inline b8 is_first() const
		{
			return index < array->mask.start;
		}

		inline T* item() const
		{
			vi_assert(!is_last() && array->active(index));
			return &array->data[index];
		}
	};

	PinArray()
		: data(), mask(), free_list()
	{
		data.length = size;
		free_list.length = size;
		for (s32 i = 0; i < size; i++)
			free_list[i] = ID((size - 1) - i);
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

	inline T operator [] (s32 i) const
	{
		vi_assert(i >= 0 && i < size);
		return data.data[i];
	}

	inline T& operator [] (s32 i)
	{
		vi_assert(i >= 0 && i < size);
		return data.data[i];
	}

	Iterator iterator()
	{
		Iterator i;
		i.index = mask.start;
		i.array = this;
		return i;
	}

	Iterator iterator_end()
	{
		Iterator i;
		i.index = mask.end - 1;
		i.array = this;
		return i;
	}

	void clear()
	{
		mask.clear();
		free_list.length = size;
		for (s32 i = 0; i < size; i++)
			free_list[i] = (size - 1) - i;
	}

	T* add()
	{
		vi_assert(free_list.length > 0);
		s32 index = free_list[free_list.length - 1];
		vi_assert(!active(index));
		free_list.remove(free_list.length - 1);
		active(index, true);
		return &data[index];
	}

	ID add(const T& t)
	{
		T* i = add();
		*i = t;
		return ID(((char*)i - (char*)&data[0]) / sizeof(T));
	}

	void remove(s32 i)
	{
		vi_assert(active(i));
		free_list.add(i);
		active(i, false);
	}
};

}
