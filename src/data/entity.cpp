#include "entity.h"
#include "vi_assert.h"

Family World::component_families = 0;

PinArray<Entity> World::list = PinArray<Entity>();
PoolBase World::component_pools[MAX_FAMILIES];
void* World::systems[MAX_FAMILIES];