#pragma once

#include "types.h"
#include "vi_assert.h"
#include "pin_array.h"

namespace VI
{

typedef u8 Family;
typedef u16 Revision;
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
		T* target = &T::list[id];
		return target->revision == revision ? target : nullptr;
	}
};

template<typename T>
struct ComponentPool : public ComponentPoolBase
{
	T* add()
	{
		return T::list.add();
	}

	T* get(ID id)
	{
		return &T::list[id];
	}

	virtual void awake(ID id)
	{
		T::list[id].awake();
	}

	virtual void remove(ID id)
	{
		T* item = &T::list[id];
		item->~T();
		item->revision++;
		T::list.remove(id);
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

	template<typename T, typename... Args> T* create(Args... args);
	template<typename T, typename... Args> T* add(Args... args);
	template<typename T> void attach(T*);
	template<typename T> void detach();
	template<typename T> void remove();
	template<typename T> inline b8 has() const;
	template<typename T> inline T* get() const;
	static PinArray<Entity, MAX_ENTITIES> list;

	inline ID id() const
	{
		return (ID)(this - &list[0]);
	}
};

struct World
{
	static Family families;
	static Array<ID> remove_buffer;
	static ComponentPoolBase* component_pools[MAX_FAMILIES];

	static void init();

	template<typename T, typename... Args> static T* create(Args... args)
	{
		Entity* e = Entity::list.add();
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
	vi_assert(!has<T>());
	T* item = T::pool.add();
	component_mask |= T::component_mask;
	components[T::family] = item->id();
	new (item) T(args...);
	item->entity_id = id();
	return item;
}

template<typename T, typename... Args> T* Entity::add(Args... args)
{
	vi_assert(!has<T>());
	T* component = create<T>(args...);
	component->awake();
	return component;
}

template<typename T> void Entity::attach(T* t)
{
	vi_assert(!has<T>());
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
	return &T::list[components[T::family]];
}

struct ComponentBase
{
	ID entity_id;
	Revision revision;

	inline Entity* entity() const
	{
		return &Entity::list[entity_id];
	}

	template<typename T> inline b8 has() const
	{
		return Entity::list[entity_id].has<T>();
	}

	template<typename T> inline T* get() const
	{
		return Entity::list[entity_id].get<T>();
	}
};

struct LinkEntry
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
		void(*function_pointer)();
	};

	const LinkEntry& operator=(const LinkEntry& other);

	LinkEntry();
	LinkEntry(ID, Revision);
	LinkEntry(const LinkEntry& other);

	virtual void fire() const { }
};

template<typename T, void (T::*Method)()> struct ObjectLinkEntry : public LinkEntry
{
	ObjectLinkEntry(ID id)
		: LinkEntry(id, T::list[id].revision)
	{

	}

	virtual void fire() const
	{
		T* t = &T::list[data.id];
		if (t->revision == data.revision)
			(t->*Method)();
	}
};

template<typename T>
struct LinkEntryArg
{
	struct Data
	{
		ID id;
		Revision revision;
		Data() : id(), revision() {}
		Data(ID i, Revision r) : id(i), revision(r) {}
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

	LinkEntryArg(ID i, Revision r)
		: data(i, r)
	{

	}

	virtual void fire(T t) const { }
};

template<typename T, typename T2, void (T::*Method)(T2)> struct ObjectLinkEntryArg : public LinkEntryArg<T2>
{
	ObjectLinkEntryArg(ID i) : LinkEntryArg<T2>(i, T::list[i].revision) { }

	virtual void fire(T2 arg) const
	{
		T* t = &T::list[LinkEntryArg<T2>::data.id];
		if (t->revision == LinkEntryArg<T2>::data.revision)
			(t->*Method)(arg);
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

	template<typename T, void (T::*Method)()> void link(T* target)
	{
		LinkEntry* entry = entries.add();
		new (entry) ObjectLinkEntry<T, Method>(target->id());
	}
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

	template<typename T, typename T2, void (T::*Method)(T2)> void link(T* target)
	{
		LinkEntryArg<T2>* entry = entries.add();
		new (entry) ObjectLinkEntryArg<T, T2, Method>(target->id());
	}
};

template<typename Derived>
struct ComponentType : public ComponentBase
{
	static Family family;
	static ComponentMask component_mask;
	static PinArray<Derived, MAX_ENTITIES> list;
	static ComponentPool<Derived> pool;

	inline ID id() const
	{
		return (ID)((Derived*)this - (Derived*)&list[0]);
	}

	template<void (Derived::*Method)()> void link(Link& link)
	{
		link.link<Derived, Method>((Derived*)this);
	}

	template<typename T2, void (Derived::*Method)(T2)> void link_arg(LinkArg<T2>& link)
	{
		link.link<Derived, T2, Method>((Derived*)this);
	}
};

#ifndef _MSC_VER
template<typename T> Family ComponentType<T>::family;
template<typename T> ComponentMask ComponentType<T>::component_mask;
template<typename T> PinArray<T, MAX_ENTITIES> ComponentType<T>::list;
template<typename T> ComponentPool<T> ComponentType<T>::pool;
#endif

}
