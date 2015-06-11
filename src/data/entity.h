#pragma once

#include "types.h"
#include "lmath.h"
#include "array.h"
#include "vi_assert.h"
#include "exec.h"
#include <GL/glew.h>

#include <stdio.h>

typedef unsigned int Family;
typedef unsigned int ID;
typedef unsigned long ComponentMask;
const Family MAX_FAMILIES = sizeof(ComponentMask) * 8;

struct EntityUpdate
{
	InputState* input;
	GameTime time;
};

enum RenderTechnique
{
	RenderTechnique_Default,
};

struct SyncData;
struct RenderParams
{
	Mat4 view;
	Mat4 projection;
	GLbitfield clear;
	RenderTechnique technique;
	SyncData* sync;
};

struct ComponentBase;

struct PoolBase
{
	bool initialized;

	// This gets reinterpreted as an ArrayNonRelocating<T> in ComponentPool.
	// Embrace the madness.
	ArrayNonRelocating<char> data;

	PoolBase()
		: initialized(), data()
	{
	}

	virtual ComponentBase* virtual_get(size_t) { return 0; }
	virtual void awake(size_t) {}
	virtual void remove(size_t) {}
};

template<typename T>
struct Pool : public PoolBase
{
	Pool()
	{
	}

	virtual ComponentBase* virtual_get(size_t id)
	{
		return reinterpret_cast<ArrayNonRelocating<T>*>(&data)->get(id);
	}

	size_t add()
	{
		return reinterpret_cast<ArrayNonRelocating<T>*>(&data)->add();
	}

	T* get(size_t id)
	{
		return reinterpret_cast<ArrayNonRelocating<T>*>(&data)->get(id);
	}

	void awake(size_t id)
	{
		reinterpret_cast<ArrayNonRelocating<T>*>(&data)->get(id)->awake();
	}

	void remove(size_t id)
	{
		reinterpret_cast<ArrayNonRelocating<T>*>(&data)->get(id)->~T();
		reinterpret_cast<ArrayNonRelocating<T>*>(&data)->remove(id);
	}
};

struct Entity
{
	ID id;
	ID components[MAX_FAMILIES];
	ComponentMask component_mask;
	Entity(ID id)
		: components(), id(id)
	{
	}
	template<typename T, typename... Args> T* create(Args... args);
	template<typename T, typename... Args> T* add(Args... args);
	template<typename T> inline T* get();
};

struct Entities : ExecDynamic<Update>
{
	static Family component_families;
	static Entities main;
	ArrayNonRelocating<Entity> list;
	PoolBase component_pools[MAX_FAMILIES];
	void* systems[MAX_FAMILIES];
	ExecSystemDynamic<EntityUpdate> update;
	ExecSystemDynamic<RenderParams*> draw;

	Entities()
		: list(), component_pools(), systems(), update(), draw()
	{
	}

	void exec(Update t)
	{
		EntityUpdate up;
		up.input = t.input;
		up.time = t.time;
		update.exec(up);
	}

	~Entities()
	{
		// TODO: entities and systems are never cleaned up
	}

	template<typename T, typename... Args> T* create(Args... args)
	{
		ID id = list.add();
		T* e = (T*)&list[id];
		new (e) T(id, args...);
		awake(e);
		return e;
	}

	template<typename SystemType> SystemType* system()
	{
		Family f = SystemType::family();
		if (!systems[f])
			systems[f] = new SystemType();
		return (SystemType*)systems[f];
	}

	template<typename T> ArrayNonRelocating<T>& component_list()
	{
		Pool<T>* pool = (Pool<T>*)&component_pools[T::family()];
		return *(reinterpret_cast<ArrayNonRelocating<T>*>(&pool->data));
	}

	ComponentBase* get_component(ID entity, Family family)
	{
		Entity* e = &list[entity];
		if (e->component_mask & (1 << family))
		{
			PoolBase* pool = &component_pools[family];
			return pool->virtual_get(e->components[family]);
		}
		else
			return 0;
	}

	template<typename T, typename... Args> T* create_component(Entity* e, Args... args)
	{
		Family f = T::family();
		Pool<T>* pool = (Pool<T>*)&component_pools[f];
		if (!pool->initialized)
		{
			new (pool) Pool<T>();
			pool->initialized = true;
		}
		size_t id = pool->add();
		T* t = pool->get(id);
		new(t) T(args...);
		t->id = id;
		t->entity_id = e->id;
		e->components[f] = id;
		e->component_mask |= 1 << f;
		return t;
	}

	template<typename T, typename... Args> T* add_component(Entity* e, Args... args)
	{
		T* component = create_component<T>(e, args...);
		PoolBase* pool = &component_pools[T::family()];
		pool->awake(component->id);
	}

	template<typename T> void awake(T* e)
	{
		for (ID i = 0; i < Entities::component_families; i++)
		{
			if (e->component_mask & (1 << i))
			{
				PoolBase* pool = &component_pools[i];
				pool->awake(e->components[i]);
			}
		}
		e->awake();
	}

	void remove(Entity* e)
	{
		list.remove(e->id);
		for (ID i = 0; i < Entities::component_families; i++)
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
	return Entities::main.create_component<T>(this, args...);
}

template<typename T, typename... Args> T* Entity::add(Args... args)
{
	return Entities::main.add_component<T>(this, args...);
}

template<typename T> inline T* Entity::get()
{
	if (component_mask & (1 << T::family()))
		return &(Entities::main.component_list<T>())[components[T::family()]];
	else
		return 0;
}

struct ComponentBase
{
	ID id;
	ID entity_id;

	inline Entity* entity()
	{
		return Entities::main.list.get(entity_id);
	}

	template<typename T> inline T* get()
	{
		return Entities::main.list.get(entity_id)->get<T>();
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
		Entity* e = &Entities::main.list[entity];
		T* t = e->get<T>();
		(t->*Method)();
	}
};

template<typename Derived>
struct ComponentType : public ComponentBase
{
	struct System
	{
		static Family family()
		{
			return Derived::family();
		}
	};

	static Family family()
	{
		static Family f = Entities::component_families++;
		vi_assert(f <= MAX_FAMILIES);
		return f;
	}

	template<void (Derived::*Method)()> void link(Link* link)
	{
		new (link) InstantiatedLink<Derived, Method>(entity_id);
	}
};
