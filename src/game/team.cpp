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


const Vec4 Team::colors[(s32)AI::Team::count] =
{
	Vec4(0.9f, 0.8f, 0.3f, 1),
	Vec4(0.8f, 0.3f, 0.3f, 1),
};

StaticArray<Team, (s32)AI::Team::count> Team::list;

Team::Team()
	: minion_spawn_timer(MINION_SPAWN_INITIAL_DELAY)
{
}

void Team::awake()
{
	for (s32 i = 0; i < targets.length; i++)
	{
		Target* t = targets[i].ref();
		if (t)
		{
			t->hit_this.link<Team, Entity*, &Team::target_hit>(this);
			t->hit_by.link<Team, Entity*, &Team::target_hit_by>(this);
		}
	}
}

void Team::target_hit(Entity* target)
{
	World::remove_deferred(target);
}

void Team::target_hit_by(Entity* enemy)
{
	AI::Team other = enemy->get<AIAgent>()->team;
	Team::list[(s32)other].score++;
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
				pos += rot * Vec3(0, 0, MINION_SPAWN_RADIUS); // spawn it around the edges
				Entity* entity = World::create<Minion>(pos, rot, team());
			}
			minion_spawn_state = (MinionSpawnState)(((s32)minion_spawn_state + 1) % (s32)MinionSpawnState::count);
			minion_spawn_timer = minion_spawn_delay(minion_spawn_state);
		}
	}
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