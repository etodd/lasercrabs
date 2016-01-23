#pragma once
#include "types.h"
#include "pin_array.h"
#include "ease.h"

namespace VI
{


struct Behavior
{
	Behavior* parent;
	virtual void run() {}
	virtual void child_done(Behavior*) {}
	virtual ~Behavior() {}
};

#define MAX_BEHAVIORS 1024

template<typename Derived> struct BehaviorBase : public Behavior
{
	static PinArray<Derived, MAX_BEHAVIORS> list;
	static Array<Derived*> active_list;

	bool active;
	bool managed; // true if this instance is allocated in "list"

	template<typename... Args> static Derived* create(Args... args)
	{
		Derived* d = list.add();
		new (d) Derived(args...);
		d->managed = true;
		return d;
	}

	static void update_all(const Update& u)
	{
		for (s32 i = 0; i < active_list.length; i++)
		{
			// TODO: if behaviors are removed during update, some behaviors will not be updated this frame
			active_list[i]->update(u);
		}
	}

	void activate()
	{
		vi_assert(!active);
		active = true;
		active_list.add((Derived*)this);
	}

	void deactivate()
	{
		vi_assert(active);
		active = false;
		for (s32 i = 0; i < active_list.length; i++)
		{
			if (active_list[i] == (Derived*)this)
			{
				active_list.remove(i);
				break;
			}
		}
	}

	virtual ~BehaviorBase()
	{
		if (active)
			deactivate();
		if (managed)
		{
			s32 id = (Derived*)this - (Derived*)&list[0];
			list.remove(id);
		}
	}
};

template<typename T> PinArray<T, MAX_BEHAVIORS> BehaviorBase<T>::list = PinArray<T, MAX_BEHAVIORS>();
template<typename T> Array<T*> BehaviorBase<T>::active_list = Array<T*>();

#define MAX_COMPOSITE 8

template<typename Derived> struct BehaviorComposite : public BehaviorBase<Derived>
{
	Behavior* children[MAX_COMPOSITE];
	s32 num_children;

	template<typename... Args> BehaviorComposite(Args... args)
		: children{ args... }, num_children(sizeof...(Args))
	{
		for (s32 i = 0; i < num_children; i++)
			children[i]->parent = this;
	}

	virtual ~BehaviorComposite()
	{
		for (s32 i = 0; i < num_children; i++)
			children[i]->~Behavior();
		BehaviorBase::~BehaviorBase();
	}
};

struct Sequence : public BehaviorComposite<Sequence>
{
	s32 index;

	template<typename... Args> Sequence(Args... args)
		: BehaviorComposite(args...), index()
	{
	}

	void run();
	void child_done(Behavior*);
};

struct Parallel : public BehaviorComposite<Parallel>
{
	s32 done;

	template<typename... Args> Parallel(Args... args)
		: BehaviorComposite(args...), done()
	{
	}

	void run();
	void child_done(Behavior*);
};

struct Repeat : public BehaviorComposite<Repeat>
{
	s32 index;
	s32 repeat_count;
	s32 repeat_index;

	template<typename... Args> Repeat(s32 repeat, Args... args)
		: BehaviorComposite(args...), repeat_count(repeat), index()
	{
	}

	void run();
	void child_done(Behavior*);
};

template<typename T> struct Set : public BehaviorBase<Set<T>>
{
	T value;
	T* target;

	Set(T* target, const T& value)
		: target(target),
		value(value)
	{
	}

	void run()
	{
		*target = value;
		if (parent)
			parent->child_done(this);
	}
};

template<typename T> struct LerpTo : public BehaviorBase<LerpTo<T>>
{
	r32 duration;
	r32 time;
	T start;
	T end;
	T* target;
	Ease::Type ease;

	LerpTo(T* target, const T& end, r32 duration, Ease::Type ease = Ease::Type::Linear)
		: target(target),
		end(end),
		duration(duration),
		ease(ease)
	{
	}

	void run()
	{
		start = *target;
		time = 0.0f;
		activate();
	}

	void update(const Update& u)
	{
		time += u.time.delta;
		if (time > duration)
		{
			*target = end;
			deactivate();
			if (parent)
				parent->child_done(this);
		}
		else
			*target = Ease::ease<T>(ease, time / duration, start, end);
	}
};


}