#pragma once

#include "types.h"
#include "array.h"
#include "vi_assert.h"
#include "exec.h"
#include "render/render.h"

#include <stdio.h>

typedef unsigned int Family;
typedef size_t ID;
typedef unsigned long ComponentMask;
const Family MAX_FAMILIES = sizeof(ComponentMask) * 8;

struct Entity;

struct ComponentBase
{
	ID id;
	Entity* entity;
};

struct Entities;

struct EntityUpdate
{
	Entities* entities;
	InputState* input;
	GameTime time;
};

// If you inherit this, do not add any data
// Create a component instead
struct Entity : public ExecDynamic<EntityUpdate>
{
	static Family families;
	ID id;
	ComponentBase* components[MAX_FAMILIES];
	Entity()
		: components(), id()
	{
	}
	virtual void exec(EntityUpdate);
	template<typename T> T* get()
	{
		return (T*)components[T::family()];
	}
};

template<typename Derived>
struct Component : public ComponentBase
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
		static Family f = Entity::families++;
		vi_assert(f <= MAX_FAMILIES);
		return f;
	}
};

struct Transform : public Component<Transform>
{
	Transform* parent;
	Vec3 pos;
	Quat rot;
	void awake(Entities* e)
	{
	}
};

struct ComponentPoolBase;

typedef void (*ComponentPoolRemove)(ComponentPoolBase*, size_t);
typedef void (*ComponentPoolAwake)(ComponentPoolBase*, size_t, Entities*);

struct ComponentPoolBase
{
	ComponentPoolRemove remove_function;
	ComponentPoolAwake awake_function;

	// This gets reinterpreted as an ArrayNonRelocating<T> in ComponentPool.
	// Embrace the madness.
	ArrayNonRelocating<char> data;
};

template<typename T>
struct ComponentPool : public ComponentPoolBase
{
	size_t add()
	{
		return reinterpret_cast<ArrayNonRelocating<T>*>(&data)->add();
	}

	T* get(size_t id)
	{
		return reinterpret_cast<ArrayNonRelocating<T>*>(&data)->get(id);
	}

	static void awake(ComponentPoolBase* pool, size_t id, Entities* e)
	{
		reinterpret_cast<ArrayNonRelocating<T>*>(&pool->data)->get(id)->awake(e);
	}

	static void remove(ComponentPoolBase* pool, size_t id)
	{
		reinterpret_cast<ArrayNonRelocating<T>*>(&pool->data)->get(id)->~T();
		reinterpret_cast<ArrayNonRelocating<T>*>(&pool->data)->remove(id);
	}
};

struct Entities : ExecDynamic<Update>
{
	ArrayNonRelocating<Entity> all;
	ComponentPoolBase component_pools[MAX_FAMILIES];
	void* systems[MAX_FAMILIES];
	ExecSystemDynamic<EntityUpdate> update;
	ExecSystemDynamic<RenderParams*> draw;

	Entities()
		: all(), component_pools(), systems(), update(), draw()
	{
	}

	void exec(Update t)
	{
		EntityUpdate up;
		up.entities = this;
		up.input = t.input;
		up.time = t.time;
		update.exec(up);
	}

	~Entities()
	{
		size_t i = 0;
		while ((i = all.next(i)) != -1)
		{
			remove(all.get(i));
			i++;
		}
		// TODO: systems are never cleaned up
	}

	template<typename T> T* create()
	{
		ID id = all.add();
		T* e = (T*)all.get(id);
		new (e) T(this);
		e->id = id;
		awake(e);
		return e;
	}

	template<typename SystemType> SystemType* system()
	{
		Family f = SystemType::family();
		if (!systems[f])
			systems[f] = new SystemType(this);
		return (SystemType*)systems[f];
	}

	template<typename T> ArrayNonRelocating<T>* components()
	{
		return ((ComponentPool<T>*)&component_pools[T::family()])->data;
	}

	template<typename T> T* create(Entity* e)
	{
		Family f = T::family();
		ComponentPool<T>* pool = (ComponentPool<T>*)&component_pools[f];
		if (!pool->remove_function)
		{
			new (pool) ComponentPool<T>();
			pool->remove_function = &ComponentPool<T>::remove;
			pool->awake_function = &ComponentPool<T>::awake;
		}
		size_t id = pool->add();
		T* t = pool->get(id);
		new(t) T();
		t->id = id;
		t->entity = e;
		e->components[f] = t;
		return t;
	}

	template<typename T> T* add(Entity* e)
	{
		T* component = create<T>(e);
		ComponentPoolBase* pool = &component_pools[T::family()];
		pool->awake_function(pool, component->id, this);
	}

	template<typename T> void awake(T* e)
	{
		for (int i = 0; i < Entity::families; i++)
		{
			if (e->components[i])
			{
				ComponentPoolBase* pool = &component_pools[i];
				pool->awake_function(pool, e->components[i]->id, this);
			}
		}
		e->awake(this);
	}

	void remove(Entity* e)
	{
		e->~Entity();
		for (int i = 0; i < Entity::families; i++)
		{
			if (e->components[i])
			{
				ComponentPoolBase* pool = &component_pools[i];
				pool->remove_function(pool, e->components[i]->id);
			}
		}
		all.remove(e->id);
	}
};
