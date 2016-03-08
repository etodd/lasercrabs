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
struct Target;
struct TargetEvent;

struct Team
{
	static const Vec4 colors[];
	static const Vec4 ui_colors[];

	static StaticArray<Team, (s32)AI::Team::count> list;

	static b8 game_over();

	Ref<Transform> player_spawn;
	Revision revision;
	r32 victory_timer;

	Team();
	void awake();

	b8 has_player() const;

	void update(const Update&);

	inline ID id() const
	{
		return this - &list[0];
	}

	inline AI::Team team() const
	{
		return (AI::Team)id();
	}
};

enum class Ability
{
	Sensor,
	Stealth,
	count,
	None = count,
};

#define ABILITY_LEVELS 3
struct AbilitySlot
{
	struct Info
	{
		AssetID icon;
		AssetID name;
		r32 cooldown;
		u16 upgrade_cost[ABILITY_LEVELS];
	};
	static Info info[(s32)Ability::count];
	Ability ability;
	u8 level;
	r32 cooldown;
	b8 can_upgrade() const;
	u16 upgrade_cost() const;
	b8 use(Entity*);
};

#define ABILITY_COUNT 1
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
	AbilitySlot abilities[ABILITY_COUNT];

	void add_credits(u16);
	void upgrade(Ability);

	PlayerManager(Team*);

	inline ID id() const
	{
		return this - &list[0];
	}

	b8 upgrade_available() const;

	void update(const Update&);
};


}