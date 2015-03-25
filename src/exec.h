#pragma once

#include "data/array.h"
#include <climits>

template<typename Derived, typename T>
struct ExecStatic
{
	int order;
	Derived* previous;
	Derived* next;

	ExecStatic(int o = 0)
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

	void insert_after_exec(Derived* i)
	{
		if (next)
			next->previous = i;
		i->previous = (Derived*)this;
		i->next = next;
		next = i;
	}
};

template<typename T>
struct ExecDynamic : public ExecStatic<ExecDynamic<T>, T>
{
	virtual void exec(T t) { }
};

template <typename T, typename T2>
struct ExecSystemStatic : ExecDynamic<T2>
{
	T head;

	ExecSystemStatic()
		: head()
	{
		head.order = INT_MIN;
	}

	void reorder(T* e)
	{
		T* i = e;
		while (i->next && i->next->order < e->order)
			i = i->next;
		while (i->previous && i->previous->order > e->order)
			i = i->previous;
		if (i != e)
		{
			e->remove_exec();
			i->insert_after_exec(e);
		}
	}

	void add(T* exec)
	{
		T* i = &head;
		while (i->next && i->next->order <= exec->order)
			i = i->next;
		i->insert_after_exec(exec);
	}

	void remove(T* e)
	{
		e->remove_exec();
	}

	void exec(T2 t)
	{
		T* exec = head.next;
		while (exec)
		{
			T* n = exec->next;
			exec->exec(t);
			exec = n;
		}
	}
};

template<typename T>
struct ExecSystemDynamic : ExecSystemStatic<ExecDynamic<T>, T>
{
};
