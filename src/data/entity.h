#pragma once

#include "types.h"
#include "vi_assert.h"
#include "pin_array.h"
#include "input.h"

namespace VI
{

typedef unsigned char Family;
typedef unsigned short ID;
typedef unsigned short Revision;
const ID IDNull = (ID)-1;
typedef unsigned long long ComponentMask;

const Family MAX_FAMILIES = sizeof(ComponentMask) * 8;
#define MAX_ENTITIES 4096

struct Update
{
	const InputState* input;
	const InputState* last_input;
	GameTime time;
};

struct ComponentBase;

struct ComponentPoolBase
{
	void* global_state;
	int global_state_size;

	virtual void awake(ID) {}
	virtual void remove(ID) {}
};

template<typename T> struct Ref
{
	ID id;
	Revision revision;

	Ref()
		: id(IDNull), revision()
	{
	}

	Ref(T* t)
	{
		operator=(t);
	}

	inline Ref<T>& operator= (T* t)
	{
		if (t)
		{
			id = t->id();
			revision = t->revision;
		}
		else
			id = IDNull;
		return *this;
	}

	inline T* ref() const
	{
		if (id == IDNull)
			return nullptr;
		T* target = &T::list()[id];
		return target->revision == revision ? target : nullptr;
	}
};

template<typename T>
struct ComponentPool : public ComponentPoolBase
{
	PinArray<T, MAX_ENTITIES> data;

	typename T* add()
	{
		return data.add();
	}

	T* get(ID id)
	{
		return &data[id];
	}

	virtual void awake(ID id)
	{
		data[id].awake();
	}

	virtual void remove(ID id)
	{
		T* item = &data[id];
		item->~T();
		item->revision++;
		data.remove(id);
	}

	template<typename T2> T2* global()
	{
		vi_assert(global_state == nullptr);
		global_state = new T2();
		global_state_size = sizeof(T2);
		return (T2*)global_state;
	}

	template<typename T2> void global(T2* p)
	{
		global_state = p;
		global_state_size = sizeof(T2);
	}
};

struct Entity
{
	ID id() const;

	ID components[MAX_FAMILIES];
	Revision revision;
	ComponentMask component_mask;
	Entity()
		: component_mask()
	{
	}
	template<typename T, typename... Args> T* create(Args... args);
	template<typename T, typename... Args> T* add(Args... args);
	template<typename T> inline bool has() const;
	template<typename T> inline T* get() const;
	static PinArray<Entity, MAX_ENTITIES>& list();
};

struct World
{
	static Family families;
	static PinArray<Entity, MAX_ENTITIES> entities;
	static Array<ID> remove_buffer;
	static ComponentPoolBase* component_pools[MAX_FAMILIES];

	static void init();

	template<typename T, typename... Args> static T* create(Args... args)
	{
		Entity* e = entities.add();
		new (e) T(args...);
		awake(e);
		return (T*)e;
	}

	template<typename T, typename... Args> static T* create_component(Entity* e, Args... args)
	{
		T* item = T::pool.add();
		e->component_mask |= T::mask;
		e->components[T::family] = item->id();
		new (item) T(args...);
		item->entity_id = e->id();
		return item;
	}

	template<typename T, typename... Args> static T* add_component(Entity* e, Args... args)
	{
		T* component = create_component<T>(e, args...);
		component->awake();
		return component;
	}

	static void awake(Entity* e)
	{
		for (Family i = 0; i < World::families; i++)
		{
			if (e->component_mask & ((ComponentMask)1 << i))
				component_pools[i]->awake(e->components[i]);
		}
	}

	static void remove(Entity* e)
	{
		ID id = e->id();
		vi_assert(entities.data[id].active);
		for (Family i = 0; i < World::families; i++)
		{
			if (e->component_mask & ((ComponentMask)1 << i))
				component_pools[i]->remove(e->components[i]);
		}
		e->component_mask = 0;
		e->revision++;
		entities.remove(id);
	}

	static void remove_deferred(Entity* e)
	{
		remove_buffer.add(e->id());
	}

	static void flush()
	{
		for (int i = 0; i < remove_buffer.length; i++)
			World::remove(&World::entities[remove_buffer[i]]);
		remove_buffer.length = 0;
	}
};

inline PinArray<Entity, MAX_ENTITIES>& Entity::list()
{
	return World::entities;
}

inline ID Entity::id() const
{
	return (ID)(((char*)this - (char*)&World::entities[0]) / sizeof(PinArrayEntry<Entity>));
}

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
	return component_mask & ((ComponentMask)1 << T::family);
}

template<typename T> inline T* Entity::get() const
{
	vi_assert(has<T>());
	return &T::list()[components[T::family]];
}

struct ComponentBase
{
	ID entity_id;
	Revision revision;

	inline Entity* entity() const
	{
		return &World::entities[entity_id];
	}

	template<typename T> inline bool has() const
	{
		return World::entities[entity_id].has<T>();
	}

	template<typename T> inline T* get() const
	{
		return World::entities[entity_id].get<T>();
	}
};

struct LinkEntry
{
	struct Data
	{
		ID entity;
		Revision revision;
		Data() : entity(), revision() {}
		Data(ID e, Revision r) : entity(e), revision(r) { }
	};

	union
	{
		Data data;
		void(*function_pointer)();
	};

	LinkEntry()
		: data()
	{

	}

	LinkEntry(ID entity)
		: data(entity, World::entities[entity].revision)
	{

	}

	virtual void fire() { }
};

template<typename T, void (T::*Method)()> struct EntityLinkEntry : public LinkEntry
{
	EntityLinkEntry(ID entity)
		: LinkEntry(entity)
	{

	}

	virtual void fire()
	{
		Entity* e = &World::entities[data.entity];
		if (e->revision == data.revision)
		{
			T* t = e->get<T>();
			(t->*Method)();
		}
	}
};

template<typename T>
struct LinkEntryArg
{
	struct Data
	{
		ID entity;
		Revision revision;
		Data() : entity(), revision() {}
		Data(ID e, Revision r) : entity(e), revision(r) {}
	};
	
	union
	{
		Data data;
		void (*function_pointer)(T);
	};

	LinkEntryArg()
		: data()
	{

	}

	LinkEntryArg(ID entity)
		: data(entity, World::entities[entity].revision)
	{

	}

	virtual void fire(T t) { }
};

template<typename T, typename T2, void (T::*Method)(T2)> struct EntityLinkEntryArg : public LinkEntryArg<T2>
{
	EntityLinkEntryArg(ID entity) : LinkEntryArg<T2>(entity) { }

	virtual void fire(T2 arg)
	{
		Entity* e = &World::entities[LinkEntryArg<T2>::data.entity];
		if (e->revision == LinkEntryArg<T2>::data.revision)
		{
			T* t = e->get<T>();
			(t->*Method)(arg);
		}
	}
};

struct FunctionPointerLinkEntry : public LinkEntry
{
	FunctionPointerLinkEntry(void(*fp)())
	{
		function_pointer = fp;
	}

	virtual void fire()
	{
		(*function_pointer)();
	}
};

template<typename T>
struct FunctionPointerLinkEntryArg : public LinkEntryArg<T>
{
	FunctionPointerLinkEntryArg(void(*fp)(T))
	{
		FunctionPointerLinkEntryArg::function_pointer = fp;
	}

	virtual void fire(T arg)
	{
		(*FunctionPointerLinkEntryArg::function_pointer)(arg);
	}
};

#define MAX_ENTITY_LINKS 4

struct Link
{
	StaticArray<LinkEntry, MAX_ENTITY_LINKS> entries;
	Link();
	void fire();
	void link(void(*)());
};

template<typename T>
struct LinkArg
{
	StaticArray<LinkEntryArg<T>, MAX_ENTITY_LINKS> entries;
	LinkArg() : entries() {}
	void fire(T t)
	{
		for (int i = 0; i < entries.length; i++)
			(&entries[i])->fire(t);
	}

	void link(void(*fp)(T))
	{
		LinkEntryArg<T>* entry = entries.add();
		new (entry) FunctionPointerLinkEntryArg<T>(fp);
	}
};

template<typename Derived>
struct ComponentType : public ComponentBase
{
	static const Family family;
	static const ComponentMask mask;
	static ComponentPool<Derived> pool;

	static inline PinArray<Derived, MAX_ENTITIES>& list()
	{
		return pool.data;
	}

	inline ID id() const
	{
		return (ID)(((char*)this - (char*)&pool.data[0]) / sizeof(PinArrayEntry<Derived>));
	}

	template<void (Derived::*Method)()> void link(Link& link)
	{
		LinkEntry* entry = link.entries.add();
		new (entry) EntityLinkEntry<Derived, Method>(entity_id);
	}

	template<typename T2, void (Derived::*Method)(T2)> void link_arg(LinkArg<T2>& link)
	{
		LinkEntryArg<T2>* entry = link.entries.add();
		new (entry) EntityLinkEntryArg<Derived, T2, Method>(entity_id);
	}
};

}
