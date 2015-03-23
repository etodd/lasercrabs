#ifndef ARRAY_H
#define ARRAY_H

#include <cstdlib>
#include <cmath>
#include <stdio.h>

#define ARRAY_GROWTH_FACTOR 1.5
#define ARRAY_INITIAL_RESERVATION 4

template <typename T>
class Array
{
	unsigned int _alloced;
public:
	T* d;
	unsigned int length;

	Array(unsigned int size = 0)
		: _alloced(size), length(0)
	{
		if (size > 0)
			d = (T*)malloc(size * sizeof(T));
		else
			d = 0;
	}

	void grow(unsigned int size)
	{
		if (size > _alloced)
		{
			unsigned int nextSize = (unsigned int)pow(ARRAY_GROWTH_FACTOR, (unsigned int)(log(size) / log(ARRAY_GROWTH_FACTOR)) + 1);
			if (!_alloced)
			{
				_alloced = nextSize > ARRAY_INITIAL_RESERVATION ? nextSize : ARRAY_INITIAL_RESERVATION;
				d = (T*)malloc(_alloced * sizeof(T));
			}
			else
			{
				_alloced = nextSize;
				d = (T*)realloc(d, _alloced * sizeof(T));
			}
		}
	}
	
	void remove(unsigned int i)
	{
		d[i] = d[length--];
	}

	void remove_ordered(unsigned int i)
	{
		memmove(&d[i + 1], &d[i]);
		length--;
	}

	void insert(unsigned int i, T& t)
	{
		grow(++length);
		memmove(&d[i], &d[i + 1]);
		d[i] = t;
	}

	void add(T& t)
	{
		grow(++length);
		d[length - 1] = t;
	}

	unsigned int add()
	{
		grow(++length);
		return length - 1;
	}
};

#endif
