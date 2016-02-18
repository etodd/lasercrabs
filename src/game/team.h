#pragma once

#include "data/entity.h"
#include "lmath.h"
#include "physics.h"
#include "render/ui.h"
#include "ai.h"

#define MAX_PLAYERS 8

namespace VI
{


struct Camera;
struct Transform;

struct Team
{
	enum class MinionSpawnState
	{
		One,
		Two,
		Three,
		count,
	};

	static StaticArray<Team, (s32)AI::Team::count> list;

	Ref<Transform> player_spawn;
	StaticArray<Ref<Transform>, 4> minion_spawns;
	MinionSpawnState minion_spawn_state;
	r32 minion_spawn_timer;
	Revision revision;
	Link lost;

	Team();

	void update(const Update&);

	inline ID id() const
	{
		return this - &list[0];
	}

	inline AI::Team team() const
	{
		return (AI::Team)id();
	}

	void set_spawn_vulnerable();
	void spawn_hit_by(Entity*);
};

struct PlayerManager
{
	static PinArray<PlayerManager, MAX_PLAYERS> list;

	r32 spawn_timer;
	Revision revision;
	char username[255];
	u16 credits;
	Ref<Team> team;
	Ref<Entity> entity;
	Link spawn;

	PlayerManager(Team*);

	inline ID id() const
	{
		return this - &list[0];
	}

	void update(const Update&);
};


}