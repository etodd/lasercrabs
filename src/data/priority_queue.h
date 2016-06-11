#pragma once

#include "array.h"
#include "vi_assert.h"

namespace VI
{


// adapted from https://github.com/pcioni/PriorityQueue
template<typename T, typename Key> struct PriorityQueue
{
	Array<T> heap;
	Key* key;

	PriorityQueue(Key* key)
		: key(key), heap()
	{
	}
	
private:

	inline void swap(s32 pos_a, s32 pos_b)
	{
		T temp = heap[pos_a];
		heap[pos_a] = heap[pos_b];
		heap[pos_b] = temp;
	}
	
	void percolate_up(s32 position)
	{
		while (position > 0)
		{
			if (key->priority(heap[position]) < key->priority(heap[(position - 1) / 2]))
			{
				swap(position, (position - 1) / 2);
				position = (position - 1) / 2;
			}
			else
				break;
		}
	}
	
	void percolate_down(s32 position)
	{
		while (position * 2 + 1 < heap.length)
		{
			s32 child;
			if (position * 2 + 2 < heap.length && key->priority(heap[position * 2 + 2]) < key->priority(heap[position * 2 + 1]))
				child = position * 2 + 2;
			else 
				child = position * 2 + 1;
			if (key->priority(heap[child]) < key->priority(heap[position]))
			{
				swap(child, position);
				position = child;
			}
			else
				break;
		}
	}

public:

	inline s32 size() const
	{
		return heap.length;
	}

	void clear()
	{
		heap.length = 0;
	}

	void reserve(s32 size)
	{
		heap.reserve(size);
	}

	void push(const T& entry)
	{
		heap.add(entry);
		percolate_up(heap.length - 1);
	}

	void update(s32 index)
	{
		percolate_up(index);
		percolate_down(index);
	}

	void remove(s32 index)
	{
		vi_assert(index < heap.length);
		if (index < heap.length - 1)
		{
			heap[index] = heap[heap.length - 1];
			heap.length--;
			percolate_up(index);
			percolate_down(index);
		}
		else
			heap.length--;
	}

	T pop()
	{
		vi_assert(heap.length > 0);
		T result = heap[0];
		if (heap.length > 1)
		{
			heap[0] = heap[heap.length - 1];
			percolate_down(0);
		}
		heap.length--;
		return result;
	}
};


}