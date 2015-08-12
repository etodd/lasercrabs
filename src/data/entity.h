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
	InputState* input;
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
		reinterpret_cast<PinArray<T>*>(&data)->get(id)->~T();
		reinterpret_cast<PinArray<T>*>(&data)->remove(id);
	}
};

struct Entity
{
	ID id;
	ID components[MAX_FAMILIES];
	ComponentMask component_mask;
	Entity(ID id)
		: components(), id(id), component_mask()
	{
	}
	template<typename T, typename... Args> T* create(Args... args);
	template<typename T, typename... Args> T* add(Args... args);
	template<typename T> inline T* get();
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

	template<typename T> static T* component(ID entity)
	{
		Entity* e = &list[entity];
		if (e->component_mask & (1 << family))
		{
			Pool<T>* pool = &component_pools[family];
			return pool->get(e->components[family]);
		}
		else
			return 0;
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
		list.remove(e->id);
		for (ID i = 0; i < World::component_families; i++)
		{
			if (e->component_mask & (1 << i))
			{
				PoolBase* pool = &component_pools[i];
				pool->remove(e->components[i]);
			}
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

template<typename T> inline T* Entity::get()
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

	inline Entity* entity()
	{
		return World::list.get(entity_id);
	}

	template<typename T> inline T* get()
	{
		return World::list.get(entity_id)->get<T>();
	}
};

struct Link
{
	ID entity;

	Link()
		: entity()
	{

	}

	Link(ID entity)
		: entity(entity)
	{

	}

	virtual void fire() { }
};

template<typename T, void (T::*Method)()> struct InstantiatedLink : public Link
{
	InstantiatedLink(ID entity)
		: Link(entity)
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
struct LinkArg
{
	ID entity;

	LinkArg()
		: entity()
	{

	}

	LinkArg(ID entity)
		: entity(entity)
	{

	}

	virtual void fire(T t) { }
};

template<typename T, typename T2, void (T::*Method)(T2)> struct InstantiatedLinkArg : public LinkArg<T2>
{
	InstantiatedLinkArg(ID entity)
		: LinkArg<T2>(entity)
	{

	}

	virtual void fire(T2 arg)
	{
		Entity* e = &World::list[LinkArg<T2>::entity];
		T* t = e->get<T>();
		(t->*Method)(arg);
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

	template<void (Derived::*Method)()> void link(Link* link)
	{
		new (link)InstantiatedLink<Derived, Method>(entity_id);
	}

	template<typename T2, void (Derived::*Method)(T2)> void link_arg(LinkArg<T2>* link)
	{
		new (link)InstantiatedLinkArg<Derived, T2, Method>(entity_id);
	}
};

}
