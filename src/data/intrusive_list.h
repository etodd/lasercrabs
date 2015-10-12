#pragma once

#include "vi_assert.h"

// Don't throw this into a field in your struct.
// You have to derive from it.
template<typename Derived>
struct IntrusiveLinkedList
{
	Derived* previous;
	Derived* next;

	IntrusiveLinkedList()
		: previous(nullptr), next(nullptr)
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

	void insert_after(Derived* i)
	{
		vi_assert(!next && !previous);
		previous = i;
		next = i->next;
		previous->next = (Derived*)this;
		if (next)
			next->previous = (Derived*)this;
	}

	void insert_before(Derived* i)
	{
		vi_assert(!next && !previous);
		previous = i->previous;
		next = i;
		if (previous)
			previous->next = (Derived*)this;
		next->previous = (Derived*)this;
	}
};