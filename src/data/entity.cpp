#include "entity.h"
#include "vi_assert.h"
#include <new>

namespace VI
{

Family World::component_families = 0;

PinArray<Entity> World::list = PinArray<Entity>(MAX_ENTITIES);
PoolBase World::component_pools[MAX_FAMILIES];

Link::Link()
	: entries(), entry_count()
{
}

void Link::link(void(*fp)())
{
	vi_assert(entry_count < MAX_ENTITY_LINKS);
	LinkEntry* entry = &entries[entry_count];
	entry_count++;
	new (entry) FunctionPointerLinkEntry(fp);
}

void Link::fire()
{
	for (int i = 0; i < entry_count; i++)
		(&entries[i])->fire();
}

}