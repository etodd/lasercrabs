#include "entity.h"
#include "vi_assert.h"
#include <new>

namespace VI
{

PinArray<Entity, MAX_ENTITIES> World::entities = PinArray<Entity, MAX_ENTITIES>();
Array<ID> World::remove_buffer = Array<ID>();
ComponentPoolBase* World::component_pools[MAX_FAMILIES];

Link::Link()
	: entries()
{
}

void Link::link(void(*fp)())
{
	LinkEntry* entry = entries.add();
	new (entry) FunctionPointerLinkEntry(fp);
}

void Link::fire()
{
	for (int i = 0; i < entries.length; i++)
		(&entries[i])->fire();
}

}