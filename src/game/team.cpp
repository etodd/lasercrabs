#include "team.h"
#include "game.h"
#include "minion.h"
#include "data/components.h"
#include "entities.h"
#include "data/animator.h"
#include "asset/animation.h"

#if DEBUG
#define PLAYER_SPAWN_DELAY 1.0f
#define MINION_SPAWN_INITIAL_DELAY 2.0f
#else
#define PLAYER_SPAWN_DELAY 5.0f
#define MINION_SPAWN_INITIAL_DELAY 10.0f
#endif
#define MINION_SPAWN_INTERVAL 30.0f
#define MINION_SPAWN_GROUP_INTERVAL 2.0f

#define CREDITS_INITIAL 50

namespace VI
{


StaticArray<Team, (s32)AI::Team::count> Team::list;

Team::Team()
	: minion_spawn_timer(MINION_SPAWN_INITIAL_DELAY)
{
}

r32 minion_spawn_delay(Team::MinionSpawnState state)
{
	if (state == Team::MinionSpawnState::One)
		return MINION_SPAWN_INTERVAL;
	else
		return MINION_SPAWN_GROUP_INTERVAL;
}

void Team::update(const Update& u)
{
	if (minion_spawns.length > 0)
	{
		minion_spawn_timer -= u.time.delta;
		if (minion_spawn_timer < 0)
		{
			for (int i = 0; i < minion_spawns.length; i++)
			{
				Vec3 pos;
				Quat rot;
				minion_spawns[i].ref()->absolute(&pos, &rot);
				pos += rot * Vec3(0, 0, 3); // spawn it around the edges
				Entity* entity = World::create<Minion>(pos, rot, team());
			}
			minion_spawn_state = (MinionSpawnState)(((s32)minion_spawn_state + 1) % (s32)MinionSpawnState::count);
			minion_spawn_timer = minion_spawn_delay(minion_spawn_state);
		}
	}
}

void Team::set_spawn_vulnerable()
{
	Entity* spawn = player_spawn.ref()->entity();

	RigidBody* body = spawn->get<RigidBody>();
	body->type = RigidBody::Type::Sphere;
	body->size = Vec3(0.5f);
	body->collision_group = CollisionTarget;
	body->collision_filter = btBroadphaseProxy::AllFilter;
	body->rebuild();

	Animator::Layer* layer = &spawn->get<Animator>()->layers[0];
	layer->blend_time = 2.0f;
	layer->loop = true;
	layer->play(Asset::Animation::spawn_vulnerable);

	spawn->add<Target>()->hit_by.link<Team, Entity*, &Team::spawn_hit_by>(this);
}

void Team::spawn_hit_by(Entity*)
{
	// we lost
	lost.fire();
}

PinArray<PlayerManager, MAX_PLAYERS> PlayerManager::list;

PlayerManager::PlayerManager(Team* team)
	: spawn_timer(PLAYER_SPAWN_DELAY),
	team(team),
	credits(CREDITS_INITIAL)
{
}

void PlayerManager::update(const Update& u)
{
	if (team.ref()->player_spawn.ref() && !entity.ref())
	{
		spawn_timer -= u.time.delta;
		if (spawn_timer < 0 || Game::data.mode == Game::Mode::Parkour)
		{
			spawn.fire();
			spawn_timer = PLAYER_SPAWN_DELAY;
		}
	}
}


}
