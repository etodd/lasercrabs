#pragma once

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "vi_assert.h"

namespace VI
{

#define ARRAY_GROWTH_FACTOR 1.5
#define ARRAY_INITIAL_RESERVATION 1

template <typename T, int size>
struct StaticArray
{
	union
	{
		char _nil[size * sizeof(T)];
		T data[size];
	};
	int length;

	StaticArray()
		: _nil(), length()
	{
	}

	StaticArray(int l)
		: _nil(), length(l)
	{
	}

	~StaticArray()
	{
	}

	inline T operator [] (const int i) const
	{
		vi_assert(i >= 0 && i < length);
		return data[i];
	}

	inline T& operator [] (const int i)
	{
		vi_assert(i >= 0 && i < length);
		return data[i];
	}

	void remove(int i)
	{
		vi_assert(i >= 0 && i < length);
		if (i != length - 1)
			data[i] = data[length - 1];
		length--;
	}

	void remove_ordered(int i)
	{
		vi_assert(i >= 0 && i < length);
		memmove(&data[i + 1], &data[i], sizeof(T) * (length - (i + 1)));
		length--;
	}

	T* insert(int i, T& t)
	{
		vi_assert(i >= 0 && i <= length);
		length++;
		vi_assert(length <= size);
		memmove(&data[i], &data[i + 1], sizeof(T) * (length - i));
		data[i] = t;
		return &data[i];
	}

	T* insert(int i)
	{
		vi_assert(i >= 0 && i <= length);
		length++;
		vi_assert(length <= size);
		memmove(&data[i], &data[i + 1], sizeof(T) * (length - i));
		return &data[i];
	}

	T* add()
	{
		length++;
		vi_assert(length <= size);
		return &data[length - 1];
	}

	T* add(const T& t)
	{
		T* p = add();
		*p = t;
		return p;
	}
};

template <typename T>
struct Array
{
	T* data;
	int length;
	int reserved;

	Array(int reserve_count = 0, int length = 0)
		: length(length)
	{
		vi_assert(reserve_count >= 0 && length >= 0 && length <= reserve_count);
		reserved = 0;
		if (reserve_count > 0)
			reserve(reserve_count);
		else
			data = 0;
	}

	~Array()
	{
		if (data)
			free(data);
	}

	inline T operator [] (const int i) const
	{
		vi_assert(i >= 0 && i < length);
		return *(data + i);
	}

	inline T& operator [] (const int i)
	{
		vi_assert(i >= 0 && i < length);
		return *(data + i);
	}

	void reserve(int size)
	{
		vi_assert(size >= 0);
		if (size > reserved)
		{
			int nextSize = (unsigned int)pow(ARRAY_GROWTH_FACTOR, (int)(log(size) / log(ARRAY_GROWTH_FACTOR)) + 1);
			if (!reserved)
			{
				nextSize = nextSize > ARRAY_INITIAL_RESERVATION ? nextSize : ARRAY_INITIAL_RESERVATION;
				data = (T*)malloc(nextSize * sizeof(T));
			}
			else
				data = (T*)realloc(data, nextSize * sizeof(T));

			memset((void*)&data[reserved], 0, (nextSize - reserved) * sizeof(T));
			reserved = nextSize;
		}
	}

	void remove(int i)
	{
		vi_assert(i >= 0 && i < length);
		if (i != length - 1)
			data[i] = data[length - 1];
		length--;
	}

	void remove_ordered(int i)
	{
		vi_assert(i >= 0 && i < length);
		memmove(&data[i + 1], &data[i], sizeof(T) * (length - (i + 1)));
		length--;
	}

	T* insert(int i, T& t)
	{
		vi_assert(i >= 0 && i <= length);
		reserve(++length);
		memmove(&data[i], &data[i + 1], sizeof(T) * (length - i));
		data[i] = t;
		return &data[i];
	}

	T* insert(int i)
	{
		vi_assert(i >= 0 && i <= length);
		reserve(++length);
		memmove(&data[i], &data[i + 1], sizeof(T) * (length - i));
		return &data[i];
	}

	void resize(int i)
	{
		reserve(i);
		length = i;
	}

	T* add()
	{
		reserve(++length);
		return &data[length - 1];
	}

	T* add(const T& t)
	{
		T* p = add();
		*p = t;
		return p;
	}
};

}