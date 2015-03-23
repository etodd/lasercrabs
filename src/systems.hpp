#ifndef SYSTEMS_H
#define SYSTEMS_H

#include "array.hpp"
#include <climits>

typedef void (*Update)(float);

struct UpdateEntry
{
	Update update;
	int order;
	unsigned int index;
	UpdateEntry* previous;
	UpdateEntry* next;

	UpdateEntry(Update u = 0, int o = 0)
	: update(u), order(o), previous(0), next(0)
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

	void insertAfter(UpdateEntry* i)
	{
		if (next)
			next->previous = i;
		i->previous = this;
		i->next = next;
		next = i;
	}
};

struct UpdateSystem
{
	UpdateEntry head;
	Array<UpdateEntry> array;

	UpdateSystem()
		: head(), array()
	{
		head.order = INT_MIN;
	}

	void add(UpdateEntry* e)
	{
		UpdateEntry* i = &head;
		while (i->next && i->order > e->order)
			i = i->next;
		i->insertAfter(e);
	}

	void remove(UpdateEntry* e)
	{
		e->remove();
		array.remove(e->index);
	}

	void reorder(UpdateEntry* e)
	{
		e->remove();
		add(e);
	}

	UpdateEntry* add(Update update, int order = 0)
	{
		unsigned int index = array.add();
		UpdateEntry* e = &array.d[index];
		*e = UpdateEntry(update, order);
		e->index = index;
		add(e);
		return e;
	}

	void go(float dt)
	{
		UpdateEntry* update = head.next;
		while (update)
		{
			UpdateEntry* n = update->next;
			update->update(dt);
			update = n;
		}
	}
};

#endif
