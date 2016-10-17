#include "entity.h"
#include <new>
#include "net.h"
#include "game/game.h"

namespace VI
{

PinArray<Entity, MAX_ENTITIES> Entity::list;
Array<ID> World::remove_buffer;
ComponentPoolBase* World::component_pools[MAX_FAMILIES];
#if SERVER && DEBUG
Array<Ref<Entity> > World::create_queue;
#endif

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
#if SERVER && DEBUG
	finalized = false;
#endif
}

void remove_components(Entity* e)
{
	for (Family i = 0; i < World::families; i++)
	{
		if (e->component_mask & ((ComponentMask)1 << i))
			World::component_pools[i]->remove(e->components[i]);
	}
	e->component_mask = 0;
}

// if the entity is active, remove it
// returns true if the entity was actually active and removed
// World::remove_deferred WILL NOT crash when it removes an entity multiple times
// World::remove WILL crash if you try to remove an inactive entity
b8 internal_remove(Entity* e)
{
	ID id = e->id();
	if (Entity::list.active(id))
	{
		Net::remove(e);
		remove_components(e);
		e->revision++;
		Entity::list.remove(id);
		return true;
	}
	return false;
}

// add an entity on the client because the server told us to
Entity* World::net_add(ID id)
{
	vi_assert(!Entity::list.active(id));
	vi_assert(Entity::list.count() < MAX_ENTITIES);
	Entity::list.active(id, true);
	Entity::list.free_list.length--;
	return &Entity::list[id];
}

// remove an entity on the client because the server told us to
void World::net_remove(Entity* e)
{
	vi_assert(Entity::list.active(e->id()));
	vi_assert(Entity::list.count() > 0);
	remove_components(e);
	Entity::list.active(e->id(), false);
	e->revision++;
	Entity::list.free_list.length++;
}

void World::remove(Entity* e)
{
	if (Game::session.local) // if we're a client, all entity removals are handled by the server
	{
		b8 actually_removed = internal_remove(e);
		vi_assert(actually_removed);
	}
}

void World::remove_deferred(Entity* e)
{
	remove_buffer.add(e->id());
}

void World::flush()
{
	for (s32 i = 0; i < remove_buffer.length; i++)
		internal_remove(&Entity::list[remove_buffer[i]]);
	remove_buffer.length = 0;
}

void World::clear()
{
	for (auto i = Entity::list.iterator(); !i.is_last(); i.next())
		remove(i.item());

	Entity::list.clear();

	remove_buffer.length = 0; // any deferred requests to remove entities should be ignored; they're all gone
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
