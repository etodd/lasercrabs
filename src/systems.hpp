#ifndef SYSTEMS_H
#define SYSTEMS_H

#include "array.hpp"
#include <climits>

template<typename T>
class Exec
{
public:
	int order;
	Exec<T>* previous;
	Exec<T>* next;

	Exec(int o = 0)
	: order(o), previous(0), next(0)
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

	void insert_after(Exec<T>* i)
	{
		if (next)
			next->previous = i;
		i->previous = this;
		i->next = next;
		next = i;
	}

	virtual void exec(T t) { }
};

template <typename T>
struct ExecSystem
{
	Exec<T> head;

	ExecSystem()
		: head()
	{
		head.order = INT_MIN;
	}

	void add(Exec<T>* exec)
	{
		Exec<T>* i = &head;
		while (i->next && i->order <= exec->order)
			i = i->next;
		i->insert_after(exec);
	}

	void remove(Exec<T>* e)
	{
		e->remove();
	}

	void go(float dt)
	{
		Exec<T>* exec = head.next;
		while (exec)
		{
			Exec<T>* n = exec->next;
			exec->exec(dt);
			exec = n;
		}
	}
};

#endif
