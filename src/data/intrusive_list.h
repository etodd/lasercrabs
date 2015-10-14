#pragma once

#include "vi_assert.h"

// Don't throw this into a field in your struct.
// You have to derive from it.
template<typename Derived>
struct IntrusiveLinkedList
{
	Derived* object;
	IntrusiveLinkedList<Derived>* previous;
	IntrusiveLinkedList<Derived>* next;

	IntrusiveLinkedList()
		: previous(nullptr), next(nullptr), object(nullptr)
	{
	}

	IntrusiveLinkedList(Derived* o)
		: previous(nullptr), next(nullptr), object(o)
	{
	}

	void remove()
	{
		if (next)
			next->previous = previous;
		if (previous)
			previous->next = next;
		next = previous = nullptr;
	}

	void insert_after(IntrusiveLinkedList<Derived>* i)
	{
		vi_assert(!next && !previous);
		previous = i;
		next = i->next;
		previous->next = this;
		if (next)
			next->previous = this;
	}

	void insert_before(IntrusiveLinkedList<Derived>* i)
	{
		vi_assert(!next && !previous);
		previous = i->previous;
		next = i;
		if (previous)
			previous->next = this;
		next->previous = this;
	}
};