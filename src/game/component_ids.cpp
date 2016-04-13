#include "entities.h"
#include "common.h"
#include "ai.h"
#include "render/views.h"
#include "render/skinned_model.h"
#include "data/animator.h"
#include "awk.h"
#include "physics.h"
#include "walker.h"
#include "audio.h"
#include "player.h"
#include "data/ragdoll.h"
#include "minion.h"
#include "ai_player.h"
#include "parkour.h"

namespace VI
{

#define COMPONENTS() \
COMPONENT_TYPE(0,  Transform) \
COMPONENT_TYPE(1,  RigidBody) \
COMPONENT_TYPE(2,  AIAgent) \
COMPONENT_TYPE(3,  Animator) \
COMPONENT_TYPE(4,  Audio) \
COMPONENT_TYPE(5,  Awk) \
COMPONENT_TYPE(6,  NoclipControl) \
COMPONENT_TYPE(7,  PointLight) \
COMPONENT_TYPE(8,  SpotLight) \
COMPONENT_TYPE(9,  DirectionalLight) \
COMPONENT_TYPE(10, AIPlayerControl) \
COMPONENT_TYPE(11, Health) \
COMPONENT_TYPE(12, MinionAI) \
COMPONENT_TYPE(13, PlayerCommon) \
COMPONENT_TYPE(14, Shockwave) \
COMPONENT_TYPE(15, Mover) \
COMPONENT_TYPE(16, Rope) \
COMPONENT_TYPE(17, MinionCommon) \
COMPONENT_TYPE(18, SkyDecal) \
COMPONENT_TYPE(19, Walker) \
COMPONENT_TYPE(20, Target) \
COMPONENT_TYPE(21, PlayerTrigger) \
COMPONENT_TYPE(22, LocalPlayerControl) \
COMPONENT_TYPE(23, Ragdoll) \
COMPONENT_TYPE(24, SkinnedModel) \
COMPONENT_TYPE(25, View) \
COMPONENT_TYPE(26, TurretControl) \
COMPONENT_TYPE(27, Projectile) \
COMPONENT_TYPE(28, Parkour) \
COMPONENT_TYPE(29, Tile) \
COMPONENT_TYPE(30, HealthPickup) \
COMPONENT_TYPE(31, Sensor) \
COMPONENT_TYPE(32, MinionSpawn) \
COMPONENT_TYPE(33, ControlPoint) \

Family World::families = 34;

#define COMPONENT_TYPE(INDEX, TYPE) \
template<> Family ComponentType<TYPE>::family = (INDEX); \
template<> ComponentMask ComponentType<TYPE>::component_mask = (ComponentMask)1 << (INDEX); \
template<> PinArray<TYPE, MAX_ENTITIES> ComponentType<TYPE>::list; \
template<> ComponentPool<TYPE> ComponentType<TYPE>::pool;

	COMPONENTS()

#undef COMPONENT_TYPE

#define COMPONENT_TYPE(INDEX, TYPE) component_pools[INDEX] = &TYPE::pool;

void World::init()
{
	COMPONENTS()
}

}
