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
COMPONENT_TYPE(0,  Audio) \
COMPONENT_TYPE(1,  Transform) \
COMPONENT_TYPE(2,  RigidBody) \
COMPONENT_TYPE(3,  View) \
COMPONENT_TYPE(4,  Animator) \
COMPONENT_TYPE(5,  AIAgent) \
COMPONENT_TYPE(6,  Drone) \
COMPONENT_TYPE(7,  Shield) \
COMPONENT_TYPE(8,  PlayerControlAI) \
COMPONENT_TYPE(9,  PlayerControlHuman) \
COMPONENT_TYPE(10, PlayerCommon) \
COMPONENT_TYPE(11, Minion) \
COMPONENT_TYPE(12, Turret) \
COMPONENT_TYPE(13, Health) \
COMPONENT_TYPE(14, PointLight) \
COMPONENT_TYPE(15, SpotLight) \
COMPONENT_TYPE(16, CoreModule) \
COMPONENT_TYPE(17, SpawnPoint) \
COMPONENT_TYPE(18, Rope) \
COMPONENT_TYPE(19, Walker) \
COMPONENT_TYPE(20, Ragdoll) \
COMPONENT_TYPE(21, Target) \
COMPONENT_TYPE(22, PlayerTrigger) \
COMPONENT_TYPE(23, SkinnedModel) \
COMPONENT_TYPE(24, Bolt) \
COMPONENT_TYPE(25, Grenade) \
COMPONENT_TYPE(26, Battery) \
COMPONENT_TYPE(27, Sensor) \
COMPONENT_TYPE(28, Rocket) \
COMPONENT_TYPE(29, Decoy) \
COMPONENT_TYPE(30, ForceField) \
COMPONENT_TYPE(31, AICue) \
COMPONENT_TYPE(32, Water) \
COMPONENT_TYPE(33, DirectionalLight) \
COMPONENT_TYPE(34, SkyDecal) \
COMPONENT_TYPE(35, Team) \
COMPONENT_TYPE(36, PlayerHuman) \
COMPONENT_TYPE(37, PlayerManager) \
COMPONENT_TYPE(38, Parkour) \
COMPONENT_TYPE(39, Interactable) \
COMPONENT_TYPE(40, TramRunner) \
COMPONENT_TYPE(41, Tram) \
COMPONENT_TYPE(42, Collectible) \

Family World::families = 43;

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
