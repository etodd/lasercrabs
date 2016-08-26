#pragma once
#include "types.h"
#include "pin_array.h"
#include "ease.h"
#include "entity.h"

namespace VI
{


struct Behavior
{
	Behavior* parent;
	Revision revision;
	virtual void run() {}
	virtual void child_done(Behavior*, b8) {}
	virtual void abort() {}
	virtual void set_context(void*) {}
	virtual void done(b8 = true) {}
	virtual b8 active() const { return false; }
	virtual Behavior* active_child() const
	{
		return active() ? (Behavior*)this : nullptr;
	}
	virtual ~Behavior() {}
	Behavior* root() const;
};

#define MAX_BEHAVIORS 512

template<typename Derived> struct BehaviorBase : public Behavior
{
	static Bitmask<MAX_BEHAVIORS> active_list;
	static PinArray<Derived, MAX_BEHAVIORS> list;

	template<typename... Args> static Derived* alloc(Args... args)
	{
		Derived* d = list.add();
		new (d) Derived(args...);
		return d;
	}

	static void update_active(const Update& u)
	{
		for (auto i = list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->active())
				i.item()->update(u);
		}
	}

	virtual b8 active() const
	{
		return active_list.get(id());
	}

	inline void active(b8 value)
	{
		active_list.set(id(), value);
	}

	inline ID id() const
	{
		return ((Derived*)this - list.data.data);
	}

	virtual void run()
	{
		active(true);
	}

	virtual void done(b8 success = true)
	{
		if (active())
		{
			active(false);
			if (parent)
				parent->child_done(this, success);
		}
	}

	virtual void abort()
	{
		active(false);
	}

	virtual ~BehaviorBase()
	{
		abort();
		list.remove(id());
	}
};

template<typename T> PinArray<T, MAX_BEHAVIORS> BehaviorBase<T>::list;
template<typename T> Bitmask<MAX_BEHAVIORS> BehaviorBase<T>::active_list;

#define MAX_COMPOSITE 8

template<typename Derived> struct BehaviorComposite : public BehaviorBase<Derived>
{
	Behavior* children[MAX_COMPOSITE];
	s32 num_children;

	template<typename... Args> BehaviorComposite(Args... args)
		: children{ args... }, num_children(sizeof...(Args))
	{
		vi_assert(num_children <= MAX_COMPOSITE);
		for (s32 i = 0; i < num_children; i++)
			children[i]->parent = this;
	}

	virtual void set_context(void* ctx)
	{
		for (s32 i = 0; i < num_children; i++)
			children[i]->set_context(ctx);
	}

	virtual void abort()
	{
		for (s32 i = 0; i < num_children; i++)
		{
			if (children[i]->active())
				children[i]->abort();
		}
		BehaviorBase<Derived>::abort();
	}

	Behavior* active_child() const
	{
		for (s32 i = 0; i < num_children; i++)
		{
			if (children[i]->active())
				return children[i]->active_child();
		}
		return nullptr;
	}

	virtual ~BehaviorComposite()
	{
		for (s32 i = 0; i < num_children; i++)
			children[i]->~Behavior();
	}
};

template<typename Derived> struct BehaviorDecorator : public BehaviorBase<Derived>
{
	Behavior* child;

	BehaviorDecorator(Behavior* c)
		: child(c)
	{
		if (c)
			c->parent = this;
	}

	virtual void set_context(void* ctx)
	{
		if (child)
			child->set_context(ctx);
	}

	virtual void abort()
	{
		if (child && child->active())
			child->abort();
		BehaviorBase<Derived>::abort();
	}

	Behavior* active_child() const
	{
		return child && child->active() ? child->active_child() : nullptr;
	}

	virtual ~BehaviorDecorator()
	{
		if (child)
			child->~Behavior();
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
	void child_done(Behavior*, b8);
};

struct Select : public BehaviorComposite<Select>
{
	s32 index;

	template<typename... Args> Select(Args... args)
		: BehaviorComposite(args...), index()
	{
	}

	void run();
	void child_done(Behavior*, b8);
};

// Fails on first child failure, aborts all children
struct Parallel : public BehaviorComposite<Parallel>
{
	s32 children_done;

	template<typename... Args> Parallel(Args... args)
		: BehaviorComposite(args...), children_done()
	{
	}

	void run();
	void child_done(Behavior*, b8);
};

struct Repeat : public BehaviorDecorator<Repeat>
{
	s32 repeat_count;
	s32 repeat_index;

	Repeat(Behavior* c, s32 repeat = -1);

	void run();
	void child_done(Behavior*, b8);
};

struct Delay : public BehaviorBase<Delay>
{
	r32 duration;
	r32 timer;

	Delay(r32 duration)
		: duration(duration), timer()
	{
	}

	void run();
	void update(const Update&);
};

struct Succeed : public BehaviorDecorator<Succeed>
{
	Succeed(Behavior* = nullptr);
	void run();
	void child_done(Behavior*, b8);
};

struct Invert : public BehaviorDecorator<Invert>
{
	Invert(Behavior*);
	void run();
	void child_done(Behavior*, b8);
};

struct Execute : public BehaviorBase<Execute>
{
	struct Entry
	{
		struct Data
		{
			ID id;
			Revision revision;
			Data();
			Data(ID, Revision);
		};

		union
		{
			Data data;
			b8(*function_pointer)();
		};

		const Entry& operator=(const Entry& other);

		Entry();
		Entry(ID, Revision);
		Entry(const Entry& other);

		virtual b8 fire() const { return false; }
	};

	template<typename T, b8 (T::*Method)()> struct ObjectEntry : public Entry
	{
		ObjectEntry(ID id)
			: Entry(id, T::list[id].revision)
		{

		}

		virtual b8 fire() const
		{
			T* t = &T::list[data.id];
			if (t->revision == data.revision)
				return (t->*Method)();
			return false;
		}
	};

	template<typename T, b8 (T::*Method)() const> struct ObjectConstEntry : public Entry
	{
		ObjectConstEntry(ID id)
			: Entry(id, T::list[id].revision)
		{

		}

		virtual b8 fire() const
		{
			T* t = &T::list[data.id];
			if (t->revision == data.revision)
				return (t->*Method)();
			return false;
		}
	};

	struct FunctionPointerEntry : public Entry
	{
		FunctionPointerEntry(b8(*fp)());
		virtual b8 fire() const;
	};

	Entry link;

	template<typename T, b8 (T::*Method)()>
	Behavior* method(T* t)
	{
		new (&link) ObjectEntry<T, Method>(t->id());
		return this;
	}

	template<typename T, b8 (T::*Method)() const>
	Behavior* method(T* t)
	{
		new (&link) ObjectConstEntry<T, Method>(t->id());
		return this;
	}

	Behavior* function(b8(*fp)());

	void run()
	{
		active(true);
		done((&link)->fire());
	}
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
		BehaviorBase<Set<T>>::active(true);
		*target = value;
		BehaviorBase<Set<T>>::done();
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
		BehaviorBase<LerpTo<T>>::active(true);
	}

	void update(const Update& u)
	{
		time += u.time.delta;
		if (time > duration)
		{
			*target = end;
			BehaviorBase<LerpTo<T>>::done();
		}
		else
			*target = Ease::ease<T>(ease, time / duration, start, end);
	}
};


}
