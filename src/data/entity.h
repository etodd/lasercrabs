#pragma once

#include "types.h"
#include "vi_assert.h"
#include "pin_array.h"
#include "input.h"

namespace VI
{

typedef unsigned int Family;
typedef unsigned int ID;
typedef unsigned long ComponentMask;

const Family MAX_FAMILIES = sizeof(ComponentMask) * 8;
const int MAX_ENTITIES = 4096;

struct Update
{
	const InputState* input;
	GameTime time;
};

struct ComponentBase;

struct PoolBase
{
	bool initialized;

	// This gets reinterpreted as an PinArray<T> in ComponentPool.
	// Embrace the madness.
	PinArray<char> data;

	PoolBase()
		: initialized(), data()
	{
	}

	virtual ComponentBase* virtual_get(int) { return 0; }
	virtual void awake(int) {}
	virtual void remove(int) {}
};

template<typename T> struct Ref
{
private:
	T* target;
public:
	int revision;

	Ref()
		: target(), revision()
	{
	}

	Ref(T* t)
		: target(t), revision(t ? t->revision : 0)
	{
	}

	inline Ref<T>& operator= (T* t)
	{
		target = t;
		revision = t ? t->revision : 0;
		return *this;
	}

	inline T* ref()
	{
		if (target && target->revision != revision)
			target = 0;
		return target;
	}
};

template<typename T>
struct Pool : public PoolBase
{
	Pool(int size)
		: PoolBase()
	{
		new ((PinArray<T>*)&data) PinArray<T>(size);
		initialized = true;
	}

	virtual ComponentBase* virtual_get(int id)
	{
		return reinterpret_cast<PinArray<T>*>(&data)->get(id);
	}

	typename PinArray<T>::Entry add()
	{
		return reinterpret_cast<PinArray<T>*>(&data)->add();
	}

	T* get(int id)
	{
		return reinterpret_cast<PinArray<T>*>(&data)->get(id);
	}

	void awake(int id)
	{
		reinterpret_cast<PinArray<T>*>(&data)->get(id)->awake();
	}

	void remove(int id)
	{
		T* item = reinterpret_cast<PinArray<T>*>(&data)->get(id);
		item->~T();
		item->revision++;
		reinterpret_cast<PinArray<T>*>(&data)->remove(id);
	}
};

struct Entity
{
	ID id;
	ID components[MAX_FAMILIES];
	int revision;
	ComponentMask component_mask;
	Entity(const ID id)
		: components(), id(id), component_mask()
	{
	}
	template<typename T, typename... Args> T* create(Args... args);
	template<typename T, typename... Args> T* add(Args... args);
	template<typename T> inline bool has() const;
	template<typename T> inline T* get() const;
};

struct World
{
	static Family component_families;
	static PinArray<Entity> list;
	static PoolBase component_pools[MAX_FAMILIES];

	template<typename T, typename... Args> static T* create(Args... args)
	{
		PinArray<Entity>::Entry entry = list.add();
		new (entry.item) T(entry.index, args...);
		awake((T*)entry.item);
		return (T*)entry.item;
	}

	template<typename T> static PinArray<T>& components()
	{
		Pool<T>* pool = (Pool<T>*)&component_pools[T::family()];
		return *(reinterpret_cast<PinArray<T>*>(&pool->data));
	}
	
	static Entity* get(ID entity)
	{
		return &list[entity];
	}

	template<typename T, typename... Args> static T* create_component(Entity* e, Args... args)
	{
		Family f = T::family();
		Pool<T>* pool = (Pool<T>*)&component_pools[f];
		if (!pool->initialized)
			new (pool)Pool<T>(MAX_ENTITIES);
		typename PinArray<T>::Entry entry = pool->add();
		new(entry.item) T(args...);
		entry.item->entity_id = e->id;
		entry.item->id = entry.index;
		e->components[f] = entry.index;
		e->component_mask |= 1 << f;
		return entry.item;
	}

	template<typename T, typename... Args> static T* add_component(Entity* e, Args... args)
	{
		T* component = create_component<T>(e, args...);
		PoolBase* pool = &component_pools[T::family()];
		pool->awake(component->id);
		return component;
	}

	template<typename T> static void awake(T* e)
	{
		for (ID i = 0; i < World::component_families; i++)
		{
			if (e->component_mask & (1 << i))
			{
				PoolBase* pool = &component_pools[i];
				pool->awake(e->components[i]);
			}
		}
		e->awake();
	}

	static void remove(Entity* e)
	{
		if (list.data[e->id].active)
		{
			list.remove(e->id);
			for (ID i = 0; i < World::component_families; i++)
			{
				if (e->component_mask & (1 << i))
				{
					PoolBase* pool = &component_pools[i];
					pool->remove(e->components[i]);
				}
			}
			e->component_mask = 0;
			e->revision++;
		}
	}
};

template<typename T, typename... Args> T* Entity::create(Args... args)
{
	return World::create_component<T>(this, args...);
}

template<typename T, typename... Args> T* Entity::add(Args... args)
{
	return World::add_component<T>(this, args...);
}

template<typename T> inline bool Entity::has() const
{
	return component_mask & (1 << T::family());
}

template<typename T> inline T* Entity::get() const
{
	if (component_mask & (1 << T::family()))
		return &World::components<T>()[components[T::family()]];
	else
		return 0;
}

struct ComponentBase
{
	ID id;
	ID entity_id;
	int revision;

	inline Entity* entity() const
	{
		return World::list.get(entity_id);
	}

	template<typename T> inline bool has() const
	{
		return World::list.get(entity_id)->has<T>();
	}

	template<typename T> inline T* get() const
	{
		return World::list.get(entity_id)->get<T>();
	}
};

struct LinkEntry
{
	ID entity;

	LinkEntry()
		: entity()
	{

	}

	LinkEntry(ID entity)
		: entity(entity)
	{

	}

	virtual void fire() { }
};

template<typename T, void (T::*Method)()> struct InstantiatedLinkEntry : public LinkEntry
{
	InstantiatedLinkEntry(ID entity)
		: LinkEntry(entity)
	{

	}

	virtual void fire()
	{
		Entity* e = &World::list[entity];
		T* t = e->get<T>();
		(t->*Method)();
	}
};

template<typename T>
struct LinkEntryArg
{
	ID entity;
	int revision;

	LinkEntryArg()
		: entity(), revision()
	{

	}

	LinkEntryArg(ID entity)
		: entity(entity), revision(World::list[entity].revision)
	{

	}

	virtual void fire(T t) { }
};

template<typename T, typename T2, void (T::*Method)(T2)> struct InstantiatedLinkEntryArg : public LinkEntryArg<T2>
{
	InstantiatedLinkEntryArg(ID entity)
		: LinkEntryArg<T2>(entity)
	{

	}

	virtual void fire(T2 arg)
	{
		Entity* e = &World::list[LinkEntryArg<T2>::entity];
		if (e->revision == LinkEntryArg<T2>::revision)
		{
			T* t = e->get<T>();
			(t->*Method)(arg);
		}
	}
};

#define MAX_ENTITY_LINKS 4

struct Link
{
	LinkEntry entries[MAX_ENTITY_LINKS];
	int entry_count;
	Link();
	void fire();
};

template<typename T>
struct LinkArg
{
	LinkEntryArg<T> entries[MAX_ENTITY_LINKS];
	int entry_count;
	LinkArg() : entries(), entry_count() {}
	void fire(T t)
	{
		for (int i = 0; i < entry_count; i++)
			(&entries[i])->fire(t);
	}
};

template<typename Derived>
struct ComponentType : public ComponentBase
{
	static Family family()
	{
		static Family f = World::component_families++;
		vi_assert(f <= MAX_FAMILIES);
		return f;
	}

	static ComponentMask mask()
	{
		return 1 << family();
	}

	template<void (Derived::*Method)()> void link(Link& link)
	{
		vi_assert(link.entry_count < MAX_ENTITY_LINKS);
		LinkEntry* entry = &link.entries[link.entry_count];
		link.entry_count++;
		new (entry) InstantiatedLinkEntry<Derived, Method>(entity_id);
	}

	template<typename T2, void (Derived::*Method)(T2)> void link_arg(LinkArg<T2>& link)
	{
		vi_assert(link.entry_count < MAX_ENTITY_LINKS);
		LinkEntryArg<T2>* entry = &link.entries[link.entry_count];
		link.entry_count++;
		new (entry) InstantiatedLinkEntryArg<Derived, T2, Method>(entity_id);
	}
};

}
