#pragma once

#include <cstdlib>
#include <cmath>
#include <stdio.h>
#include <cstring>

#define ARRAY_GROWTH_FACTOR 1.5
#define ARRAY_INITIAL_RESERVATION 4

template <typename T>
struct Array
{
	T* data;
	size_t length;
	size_t reserved;

	Array(size_t size = 0)
		: reserved(size), length(0)
	{
		if (size > 0)
			data = (T*)malloc(size * sizeof(T));
		else
			data = 0;
	}

	~Array()
	{
		if (data)
			free(data);
	}

	void reserve(size_t size)
	{
		if (size > reserved)
		{
			size_t nextSize = (unsigned int)pow(ARRAY_GROWTH_FACTOR, (size_t)(log(size) / log(ARRAY_GROWTH_FACTOR)) + 1);
			if (!reserved)
			{
				nextSize = nextSize > ARRAY_INITIAL_RESERVATION ? nextSize : ARRAY_INITIAL_RESERVATION;
				data = (T*)malloc(nextSize * sizeof(T));
			}
			else
				data = (T*)realloc(data, nextSize * sizeof(T));

			memset((void*)(data + reserved * sizeof(T)), 0, (nextSize - reserved) * sizeof(T));
			reserved = nextSize;
		}
	}
	
	void remove(size_t i)
	{
		data[i] = data[length--];
	}

	void remove_ordered(size_t i)
	{
		memmove(&data[i + 1], &data[i]);
		length--;
	}

	void insert(size_t i, T& t)
	{
		reserve(++length);
		memmove(&data[i], &data[i + 1]);
		data[i] = t;
	}

	T* add()
	{
		reserve(++length);
		return &data[length - 1];
	}

	T* add(T& t)
	{
		T* p = add();
		*p = t;
		return p;
	}
};

template<typename T>
struct ArrayNonRelocating
{
	struct Entry
	{
		bool active;
		T data;
	};
	Array<Entry> data;
	Array<size_t> free_list;

	ArrayNonRelocating(size_t size = 0)
		: data(size), free_list()
	{
	}

	size_t next(size_t index)
	{
		for (size_t i = index; i < data.length; i++)
		{
			if (data.data[i].active)
				return i;
		}
		return data.length;
	}

	T* get(size_t i)
	{
		return &data.data[i].data;
	}

	size_t add()
	{
		if (free_list.length > 0)
		{
			size_t index = free_list.data[0];
			free_list.remove(0);
			data.data[index].active = true;
			return index;
		}
		else
		{
			data.add();
			return data.length - 1;
		}
	}

	size_t add(T& t)
	{
		size_t index = add();
		&data.data[index] = t;
		return index;
	}

	void remove(size_t i)
	{
		data.data[i].active = false;
		free_list.add(i);
	}
};
