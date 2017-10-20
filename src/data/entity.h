#pragma once

#include "types.h"
#include "vi_assert.h"
#include "pin_array.h"

namespace VI
{

#define MAX_FAMILIES (sizeof(ComponentMask) * 8)

struct ComponentPoolBase
{
	virtual void awake(ID) = 0;
	virtual void net_add(ID, ID, Revision) = 0;
	virtual void remove(ID) = 0;
	virtual Revision revision(ID) = 0;
	virtual void clear() = 0;
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
		return T::list.active(id) && target->revision == revision ? target : nullptr;
	}

	inline b8 equals(const Ref<T>& other) const
	{
		return id == other.id && revision == other.revision;
	}
};

template<typename T>
struct ComponentPool : public ComponentPoolBase
{
	s32 _; // force compiler to keep this thing in memory even though it doesn't store any data

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

	virtual void net_add(ID id, ID entity_id, Revision rev)
	{
		T::list.active(id, true);
		T* t = &T::list[id];
		new (t) T();
		t->entity_id = entity_id;
		t->revision = rev;
		T::list.free_list.length--; // so count() returns the right value
	}

	virtual void remove(ID id)
	{
		vi_assert(T::list.active(id));
		T* item = &T::list[id];
		item->~T();
		item->revision++;
		T::list.remove(id);
	}
	
	virtual void clear()
	{
		T::list.clear();
		for (s32 i = 0; i < T::list.data.length; i++)
			T::list.data[i].revision = 0;
	}

	virtual Revision revision(ID id)
	{
		return T::list[id].revision;
	}
};

struct Entity
{
	ComponentMask component_mask;
	ID components[MAX_FAMILIES];
	Revision revision;
	Entity();

	template<typename T, typename... Args> T* create(Args... args);
	template<typename T, typename... Args> T* add(Args... args);
	template<typename T> void remove();
	template<typename T> inline b8 has() const;
	template<typename T> inline T* get() const;
	static PinArray<Entity, MAX_ENTITIES> list;

	struct Iterator
	{
		ComponentMask mask;
		ID index;

		inline b8 is_last() const
		{
			return index >= Entity::list.mask.end;
		}

		inline void next()
		{
			while (true)
			{
				index = Entity::list.mask.next(index);
				if (is_last() || (Entity::list[index].component_mask & mask))
					break;
			}
		}

		inline Entity* item() const
		{
			vi_assert(!is_last() && Entity::list.active(index));
			return &Entity::list[index];
		}
	};

	static Iterator iterator(ComponentMask);

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

	template<typename T, typename... Args> static T* alloc(Args... args)
	{
		Entity* e = Entity::list.add();
		e->revision++;
		new (e) T(args...);
		return (T*)e;
	}

	template<typename T, typename... Args> static T* create(Args... args)
	{
		Entity* e = Entity::list.add();
		e->revision++;
		new (e) T(args...);
		awake(e);
		return (T*)e;
	}

	static Entity* net_add(ID);
	static void remove(Entity*);
	static void net_remove(Entity*);
	static void remove_deferred(Entity*);
	static void awake(Entity*);
	static void flush();
	static void clear();
};

template<typename T, typename... Args> T* Entity::create(Args... args)
{
	vi_assert(!has<T>());
	T* item = T::pool.add();
	component_mask |= T::component_mask;
	components[T::family] = item->id();
	item->revision++;
	Revision r = item->revision;
	new (item) T(args...);
	item->revision = r;
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

#define MAX_ENTITY_LINKS 8

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

	template<typename T2, typename T3, void (T2::*Method)(T3)> void link(T2* target)
	{
		LinkEntryArg<T3>* entry = entries.add();
		new (entry) ObjectLinkEntryArg<T2, T3, Method>(target->id());
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

	template<typename T2, void (Derived::*Method)(T2)> void link_arg(LinkArg<T2>& l)
	{
		l.template link<Derived, T2, Method>((Derived*)this);
	}
};

#ifndef _MSC_VER
template<typename T> Family ComponentType<T>::family;
template<typename T> ComponentMask ComponentType<T>::component_mask;
template<typename T> PinArray<T, MAX_ENTITIES> ComponentType<T>::list;
template<typename T> ComponentPool<T> ComponentType<T>::pool;
#endif

}
