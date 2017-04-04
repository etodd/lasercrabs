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
COMPONENT_TYPE(10, Turret) \
COMPONENT_TYPE(11, Audio) \
COMPONENT_TYPE(12, Health) \
COMPONENT_TYPE(13, PointLight) \
COMPONENT_TYPE(14, SpotLight) \
COMPONENT_TYPE(15, ControlPoint) \
COMPONENT_TYPE(16, PlayerSpawn) \
COMPONENT_TYPE(17, Rope) \
COMPONENT_TYPE(18, Walker) \
COMPONENT_TYPE(19, Ragdoll) \
COMPONENT_TYPE(20, Target) \
COMPONENT_TYPE(21, PlayerTrigger) \
COMPONENT_TYPE(22, SkinnedModel) \
COMPONENT_TYPE(23, Projectile) \
COMPONENT_TYPE(24, Grenade) \
COMPONENT_TYPE(25, Battery) \
COMPONENT_TYPE(26, Sensor) \
COMPONENT_TYPE(27, Rocket) \
COMPONENT_TYPE(28, Decoy) \
COMPONENT_TYPE(29, ForceField) \
COMPONENT_TYPE(30, AICue) \
COMPONENT_TYPE(31, Water) \
COMPONENT_TYPE(32, DirectionalLight) \
COMPONENT_TYPE(33, SkyDecal) \
COMPONENT_TYPE(34, Team) \
COMPONENT_TYPE(35, PlayerHuman) \
COMPONENT_TYPE(36, PlayerManager) \
COMPONENT_TYPE(37, Parkour) \
COMPONENT_TYPE(38, Interactable) \
COMPONENT_TYPE(39, TramRunner) \
COMPONENT_TYPE(40, Tram) \
COMPONENT_TYPE(41, Collectible) \

Family World::families = 42;

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
