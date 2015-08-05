#pragma once

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "vi_assert.h"

namespace VI
{

#define ARRAY_GROWTH_FACTOR 1.5
#define ARRAY_INITIAL_RESERVATION 4

template <typename T>
struct Array
{
	T* data;
	int length;
	int reserved;

	Array(int size = 0)
		: length(0)
	{
		vi_assert(size >= 0);
		if (size > 0)
		{
			if (size < ARRAY_INITIAL_RESERVATION)
				size = ARRAY_INITIAL_RESERVATION;
			data = (T*)malloc(size * sizeof(T));
		}
		else
			data = 0;

		reserved = size;
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
		data[i] = data[length--];
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

// Don't throw this into a field in your struct.
// You have to derive from it.
template<typename Derived>
struct IntrusiveLinkedList
{
	Derived* previous;
	Derived* next;

	IntrusiveLinkedList()
		: previous(0), next(0)
	{
	}

	void remove()
	{
		if (next)
			next->previous = previous;
		if (previous)
			previous->next = next;
		next = previous = 0;
	}

	void insert_after(Derived* i)
	{
		if (next)
			next->previous = i;
		i->previous = (Derived*)this;
		i->next = next;
		next = i;
	}
};

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

	PinArray(int size = 0)
		: data(), free_list()
	{
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
		return &(data.data + i)->item;
	}

	Entry add()
	{
		Entry e;
		if (free_list.length > 0)
		{
			e.index = free_list.data[0];
			free_list.remove(0);
		}
		else
		{
			data.add();
			e.index = data.length - 1;
		}
		data[e.index].active = true;
		e.item = &data[e.index].item;
		return e;
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
		vi_assert(i >= 0 && i < data.length);
		free_list.add(i);
		data[i].active = false;
	}
};

template<typename T>
struct Queue
{
	struct Entry
	{
		T data;
		Entry* next;
		int index;
	};
	PinArray<T> array;
	Entry* head;
	Entry* tail;

	Queue()
		: array(), head(), tail()
	{
	}

	bool empty()
	{
		return head == 0;
	}

	void enqueue(T& t)
	{
		int index = array.add(t);

		Entry* entry = array.get(index);
		entry->data = t;
		entry->index = index;
		entry->next = 0;

		if (!head)
			head = entry;

		if (tail)
			tail->next = entry;

		tail = entry;
	}

	T dequeue()
	{
		Entry* result = head;
		head = head->next;
		array.remove(result->index);
		return result->data;
	}
};

}
