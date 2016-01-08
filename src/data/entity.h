#pragma once

#include "types.h"
#include "vi_assert.h"
#include "pin_array.h"

namespace VI
{

typedef u8 Family;
typedef u16 ID;
typedef u16 Revision;
const ID IDNull = (ID)-1;
typedef u64 ComponentMask;

const Family MAX_FAMILIES = sizeof(ComponentMask) * 8;
#define MAX_ENTITIES 4096

struct ComponentPoolBase
{
	void* global_state;
	s32 global_state_size;

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
	ID components[MAX_FAMILIES];
	Revision revision;
	ComponentMask component_mask;
	Entity();

	inline ID id() const;

	template<typename T, typename... Args> T* create(Args... args);
	template<typename T, typename... Args> T* add(Args... args);
	template<typename T> void attach(T*);
	template<typename T> void detach();
	template<typename T> void remove();
	template<typename T> inline b8 has() const;
	template<typename T> inline T* get() const;
	static inline PinArray<Entity, MAX_ENTITIES>& list();
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

	static void remove(Entity*);
	static void remove_deferred(Entity*);
	static void awake(Entity*);
	static void flush();
};

template<typename T, typename... Args> T* Entity::create(Args... args)
{
	T* item = T::pool.add();
	component_mask |= T::component_mask;
	components[T::family] = item->id();
	new (item) T(args...);
	item->entity_id = id();
	return item;
}

template<typename T, typename... Args> T* Entity::add(Args... args)
{
	T* component = create<T>(args...);
	component->awake();
	return component;
}

template<typename T> void Entity::attach(T* t)
{
	component_mask |= T::component_mask;
	components[T::family] = t->id();
	t->entity_id = id();
}

template<typename T> void Entity::detach()
{
	get<T>()->entity_id = IDNull;
	component_mask &= ~T::component_mask;
}

template<typename T> void Entity::remove()
{
	vi_assert(has<T>());
	T::pool.remove(components[T::family]);
	component_mask &= ~T::component_mask;
}

template<typename T> inline b8 Entity::has() const
{
	return component_mask & T::component_mask;
}

template<typename T> inline T* Entity::get() const
{
	vi_assert(has<T>());
	return &T::list()[components[T::family]];
}

inline PinArray<Entity, MAX_ENTITIES>& Entity::list()
{
	return World::entities;
}

struct ComponentBase
{
	ID entity_id;
	Revision revision;

	inline Entity* entity() const
	{
		return &World::entities[entity_id];
	}

	template<typename T> inline b8 has() const
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
		Data();
		Data(ID e, Revision r);
	};

	union
	{
		Data data;
		void(*function_pointer)();
	};

	const LinkEntry& operator=(const LinkEntry& other);

	LinkEntry();
	LinkEntry(ID entity);
	LinkEntry(const LinkEntry& other);

	virtual void fire() const { }
};

template<typename T, void (T::*Method)()> struct EntityLinkEntry : public LinkEntry
{
	EntityLinkEntry(ID entity)
		: LinkEntry(entity)
	{

	}

	virtual void fire() const
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

	virtual void fire(T t) const { }
};

template<typename T, typename T2, void (T::*Method)(T2)> struct EntityLinkEntryArg : public LinkEntryArg<T2>
{
	EntityLinkEntryArg(ID entity) : LinkEntryArg<T2>(entity) { }

	virtual void fire(T2 arg) const
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
	FunctionPointerLinkEntry(void(*fp)());
	virtual void fire() const;
};

template<typename T>
struct FunctionPointerLinkEntryArg : public LinkEntryArg<T>
{
	FunctionPointerLinkEntryArg(void(*fp)(T))
	{
		FunctionPointerLinkEntryArg::function_pointer = fp;
	}

	virtual void fire(T arg) const
	{
		(*FunctionPointerLinkEntryArg::function_pointer)(arg);
	}
};

#define MAX_ENTITY_LINKS 4

struct Link
{
	StaticArray<LinkEntry, MAX_ENTITY_LINKS> entries;
	void fire() const;
	void link(void(*)());
};

template<typename T>
struct LinkArg
{
	StaticArray<LinkEntryArg<T>, MAX_ENTITY_LINKS> entries;
	LinkArg() : entries() {}
	void fire(T t) const
	{
		for (s32 i = 0; i < entries.length; i++)
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
	static const ComponentMask component_mask;
	static ComponentPool<Derived> pool;

	static inline PinArray<Derived, MAX_ENTITIES>& list()
	{
		return pool.data;
	}

	inline ID id() const
	{
		return (ID)(((char*)this - (char*)&pool.data[0]) / sizeof(Derived));
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
