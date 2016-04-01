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
struct PlayerManager;

#define GAME_TIME_LIMIT 420.0f
#define CREDITS_MINION 10
#define CREDITS_DAMAGE 20
#define CREDITS_DETECT 10
#define CREDITS_INITIAL 0

enum class Ability
{
	Sensor,
	Stealth,
	SkipCooldown,
	count,
	None = count,
};

#define MAX_ABILITY_LEVELS 3
struct AbilityInfo
{
	AssetID icon;
	AssetID name;
	r32 cooldown;
	s32 max_level;
	u16 upgrade_cost[MAX_ABILITY_LEVELS];
	static AbilityInfo list[(s32)Ability::count];
};

struct Team
{
	struct SensorTrack
	{
		Ref<Entity> entity;
		r32 timer;
		b8 tracking;
		b8 visible;
	};

	struct SensorTrackHistory
	{
		u16 hp;
		u16 hp_max;
		u16 credits;
		Ability ability;
		u8 ability_level;
	};

	static const Vec4 colors[];
	static const Vec4 ui_colors[];

	static StaticArray<Team, (s32)AI::Team::count> list;

	static b8 abilities_enabled;
	static b8 game_over();
	static b8 is_draw();

	static void update_player_visibility();
	Ref<Transform> player_spawn;
	Revision revision;
	r32 victory_timer;
	r32 sensor_time;
	b8 sensor_explode;
	SensorTrack player_tracks[MAX_PLAYERS];
	SensorTrackHistory player_track_history[MAX_PLAYERS];

	Team();
	void awake();

	static void extract_history(PlayerManager*, SensorTrackHistory*);
	b8 has_player() const;

	static void update(const Update&);

	inline ID id() const
	{
		return this - &list[0];
	}

	inline AI::Team team() const
	{
		return (AI::Team)id();
	}
};

struct PlayerManager
{
	static PinArray<PlayerManager, MAX_PLAYERS> list;

	r32 spawn_timer;
	Revision revision;
	char username[255];
	u16 credits;
	r32 credits_flash_timer;
	Ref<Team> team;
	Ref<Entity> entity;
	Link spawn;
	Ability ability;
	u8 ability_level[(s32)Ability::count];
	r32 ability_cooldown;

	b8 ability_use();
	void ability_switch(Ability);
	void ability_upgrade(Ability);
	b8 ability_upgrade_available(Ability = Ability::None) const;
	u16 ability_upgrade_cost(Ability) const;

	void add_credits(u16);

	PlayerManager(Team*);

	inline ID id() const
	{
		return this - &list[0];
	}

	b8 at_spawn() const;

	void update(const Update&);
};


}