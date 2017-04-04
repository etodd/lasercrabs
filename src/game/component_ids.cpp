#include "entities.h"
#include "common.h"
#include "ai.h"
#include "render/views.h"
#include "render/skinned_model.h"
#include "data/animator.h"
#include "drone.h"
#include "physics.h"
#include "walker.h"
#include "audio.h"
#include "player.h"
#include "data/ragdoll.h"
#include "minion.h"
#include "ai_player.h"
#include "parkour.h"
#include "team.h"

namespace VI
{

#define COMPONENTS() \
COMPONENT_TYPE(0,  Transform) \
COMPONENT_TYPE(1,  RigidBody) \
COMPONENT_TYPE(2,  View) \
COMPONENT_TYPE(3,  Animator) \
COMPONENT_TYPE(4,  AIAgent) \
COMPONENT_TYPE(5,  Drone) \
COMPONENT_TYPE(6,  PlayerControlAI) \
COMPONENT_TYPE(7,  PlayerControlHuman) \
COMPONENT_TYPE(8,  PlayerCommon) \
COMPONENT_TYPE(9,  Minion) \
COMPONENT_TYPE(10, Audio) \
COMPONENT_TYPE(11, Health) \
COMPONENT_TYPE(12, PointLight) \
COMPONENT_TYPE(13, SpotLight) \
COMPONENT_TYPE(14, ControlPoint) \
COMPONENT_TYPE(15, PlayerSpawn) \
COMPONENT_TYPE(16, Rope) \
COMPONENT_TYPE(17, Walker) \
COMPONENT_TYPE(18, Ragdoll) \
COMPONENT_TYPE(19, Target) \
COMPONENT_TYPE(20, PlayerTrigger) \
COMPONENT_TYPE(21, SkinnedModel) \
COMPONENT_TYPE(22, Projectile) \
COMPONENT_TYPE(23, Grenade) \
COMPONENT_TYPE(24, Battery) \
COMPONENT_TYPE(25, Sensor) \
COMPONENT_TYPE(26, Rocket) \
COMPONENT_TYPE(27, Decoy) \
COMPONENT_TYPE(28, ForceField) \
COMPONENT_TYPE(29, AICue) \
COMPONENT_TYPE(30, Water) \
COMPONENT_TYPE(31, DirectionalLight) \
COMPONENT_TYPE(32, SkyDecal) \
COMPONENT_TYPE(33, Team) \
COMPONENT_TYPE(34, PlayerHuman) \
COMPONENT_TYPE(35, PlayerManager) \
COMPONENT_TYPE(36, Parkour) \
COMPONENT_TYPE(37, Interactable) \
COMPONENT_TYPE(38, TramRunner) \
COMPONENT_TYPE(39, Tram) \
COMPONENT_TYPE(40, Collectible) \

Family World::families = 41;

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
