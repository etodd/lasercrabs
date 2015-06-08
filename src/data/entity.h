#pragma once

#include "types.h"
#include "lmath.h"
#include "array.h"
#include "vi_assert.h"
#include "exec.h"
#include <GL/glew.h>

#include <stdio.h>

typedef size_t Family;
typedef size_t ID;
typedef unsigned long ComponentMask;
const Family MAX_FAMILIES = sizeof(ComponentMask) * 8;

struct Entity;

struct ComponentBase
{
	ID id;
	Entity* entity;
	static Family families;
};

struct Entities;

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

struct Entity
{
	static Family families;
	ID id;
	ComponentBase* components[MAX_FAMILIES];
	Entity()
		: components(), id()
	{
	}

	template<typename T> T* get()
	{
		return (T*)components[T::family()];
	}

	virtual Family instance_family() = 0;
};

template<typename Derived>
struct EntityType : Entity
{
	static Family family()
	{
		static Family f = Entity::families++;
		vi_assert(f <= MAX_FAMILIES);
		return f;
	}

	virtual Family instance_family()
	{
		return Derived::family();
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
		static Family f = ComponentBase::families++;
		vi_assert(f <= MAX_FAMILIES);
		return f;
	}
};

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

	virtual void awake(size_t) {}
	virtual void remove(size_t) {}
};

template<typename T>
struct Pool : public PoolBase
{
	Pool()
	{
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

struct Entities : ExecDynamic<Update>
{
	static Entities all;
	PoolBase entity_pools[MAX_FAMILIES];
	PoolBase component_pools[MAX_FAMILIES];
	void* systems[MAX_FAMILIES];
	ExecSystemDynamic<EntityUpdate> update;
	ExecSystemDynamic<RenderParams*> draw;

	Entities()
		: entity_pools(), component_pools(), systems(), update(), draw()
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
		Family f = T::family();
		Pool<T>* pool = (Pool<T>*)&entity_pools[f];
		if (!pool->initialized)
		{
			new (pool) Pool<T>();
			pool->initialized = true;
		}
		
		ID id = pool->add();
		T* e = (T*)pool->get(id);
		new (e) T(args...);
		e->id = id;
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

	template<typename T> ArrayNonRelocating<T>* components()
	{
		return ((Pool<T>*)&component_pools[T::family()])->data;
	}

	template<typename T, typename... Args> T* component(Entity* e, Args... args)
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
		t->entity = e;
		e->components[f] = t;
		return t;
	}

	template<typename T> T* add(Entity* e)
	{
		T* component = create<T>(e);
		PoolBase* pool = &component_pools[T::family()];
		pool->awake(component->id);
	}

	template<typename T> void awake(T* e)
	{
		for (size_t i = 0; i < ComponentBase::families; i++)
		{
			if (e->components[i])
			{
				PoolBase* pool = &component_pools[i];
				pool->awake(e->components[i]->id);
			}
		}
		e->awake();
	}

	void remove(Entity* e)
	{
		PoolBase* pool = &entity_pools[e->instance_family()];
		pool->remove(e->id);
		for (size_t i = 0; i < ComponentBase::families; i++)
		{
			if (e->components[i])
			{
				PoolBase* pool = &component_pools[i];
				pool->remove(e->components[i]->id);
			}
		}
	}
};
