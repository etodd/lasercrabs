#include "entity.h"
#include <new>

namespace VI
{

PinArray<Entity, MAX_ENTITIES> Entity::list = PinArray<Entity, MAX_ENTITIES>();
Array<ID> World::remove_buffer = Array<ID>();
ComponentPoolBase* World::component_pools[MAX_FAMILIES];

LinkEntry::Data::Data()
	: id(), revision()
{
}

LinkEntry::Data::Data(ID e, Revision r)
	: id(e), revision(r)
{
}

FunctionPointerLinkEntry::FunctionPointerLinkEntry(void(*fp)())
{
	function_pointer = fp;
}

void FunctionPointerLinkEntry::fire() const
{
	(*function_pointer)();
}

void Link::link(void(*fp)())
{
	LinkEntry* entry = entries.add();
	new (entry) FunctionPointerLinkEntry(fp);
}

void Link::fire() const
{
	for (s32 i = 0; i < entries.length; i++)
		(&entries[i])->fire();
}

void World::awake(Entity* e)
{
	for (Family i = 0; i < World::families; i++)
	{
		if (e->component_mask & ((ComponentMask)1 << i))
			component_pools[i]->awake(e->components[i]);
	}
}

Entity::Entity()
	: component_mask()
{
}

void World::remove(Entity* e)
{
	ID id = e->id();
	vi_assert(Entity::list.active(id));
	for (Family i = 0; i < World::families; i++)
	{
		if (e->component_mask & ((ComponentMask)1 << i))
			component_pools[i]->remove(e->components[i]);
	}
	e->component_mask = 0;
	e->revision++;
	Entity::list.remove(id);
}

void World::remove_deferred(Entity* e)
{
	remove_buffer.add(e->id());
}

void World::flush()
{
	for (s32 i = 0; i < remove_buffer.length; i++)
		World::remove(&Entity::list[remove_buffer[i]]);
	remove_buffer.length = 0;
}

LinkEntry::LinkEntry()
	: data()
{

}

LinkEntry::LinkEntry(ID i, Revision r)
	: data(i, r)
{

}

LinkEntry::LinkEntry(const LinkEntry& other)
	: data(other.data)
{
}

const LinkEntry& LinkEntry::operator=(const LinkEntry& other)
{
	data = other.data;
	return *this;
}


}
