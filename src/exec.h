#pragma once

#include "data/array.h"
#include <climits>

template<typename T>
struct Exec
{
	int order;
	Exec<T>* previous;
	Exec<T>* next;

	Exec(int o = 0)
	: order(o), previous(0), next(0)
	{
	}

	void remove_exec()
	{
		if (next)
			next->previous = previous;
		if (previous)
			previous->next = next;
		next = previous = 0;
	}

	void insert_after_exec(Exec<T>* i)
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
struct ExecSystem : Exec<T>
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
		i->insert_after_exec(exec);
	}

	void remove(Exec<T>* e)
	{
		e->remove_exec();
	}

	void exec(T t)
	{
		Exec<T>* exec = head.next;
		while (exec)
		{
			Exec<T>* n = exec->next;
			exec->exec(t);
			exec = n;
		}
	}
};
