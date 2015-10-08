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
			while (index < array->data.length)
			{
				if (array->data[index].active)
					return;
				index++;
			}
			index = -1;
		}

		bool is_last()
		{
			return index == -1;
		}

		T* item()
		{
			if (index == -1)
				return 0;
			else
				return &array->data[index].item;
		}
	};

	PinArray()
		: data(), free_list()
	{

	}

	PinArray(int size)
		: data(size, size), free_list(size, size)
	{
		for (int i = 0; i < size; i++)
			free_list[i] = (size - 1) - i;
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
		i.index = -1;
		i.array = this;
		i.next();
		return i;
	}

	void remove(int i)
	{
		vi_assert(i >= 0 && i < data.length && data[i].active);
		free_list.add(i);
		data[i].active = false;
	}
};

}