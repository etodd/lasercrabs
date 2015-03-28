#pragma once

#include "data/array.h"
#include <climits>

template<typename Derived, typename T>
struct ExecStatic : public IntrusiveLinkedList<Derived>
{
	int order;

	ExecStatic(int o = 0)
	: order(o)
	{
	}

	void reorder_exec(int o)
	{
		order = o;
		T* i = this;
		while (i->next && i->next->order < o)
			i = i->next;
		while (i->previous && i->previous->order > o)
			i = i->previous;
		if (i != this)
		{
			this->remove();
			i->insert_after(this);
		}
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

	void add(T* exec)
	{
		T* i = &head;
		while (i->next && i->next->order < exec->order)
			i = i->next;
		i->insert_after(exec);
	}

	void remove(T* e)
	{
		e->remove();
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
