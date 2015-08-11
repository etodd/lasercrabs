#include "entity.h"
#include "vi_assert.h"

namespace VI
{

Family World::component_families = 0;

PinArray<Entity> World::list = PinArray<Entity>(MAX_ENTITIES);
PoolBase World::component_pools[MAX_FAMILIES];

}