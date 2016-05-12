#pragma once

#include "data/entity.h"
#include "lmath.h"
#include "physics.h"
#include "render/ui.h"
#include "ai.h"

#define MAX_PLAYERS 4

namespace VI
{


struct Camera;
struct Transform;
struct Target;
struct TargetEvent;
struct PlayerManager;

#define GAME_TIME_LIMIT (60.0f * 10.0f)
#define CREDITS_MINION 10
#define CREDITS_DAMAGE 30
#define CREDITS_DETECT 10
#define CREDITS_INITIAL 40
#define CREDITS_SENSOR_DESTROY 10
#define CREDITS_CONTROL_POINT 2

#define ABILITY_UPGRADE_TIME 1.5f

// if the ability cooldown is lower than this, we can use the ability
// we should flash the ability icon during this time to indicate the ability is now usable

enum class Ability
{
	Sensor,
	Teleporter,
	Minion,
	count,
	None = count,
};

#define MAX_ABILITY_LEVELS 2
struct AbilityInfo
{
	AssetID icon;
	r32 spawn_time;
	u16 spawn_cost;
	u16 upgrade_cost[MAX_ABILITY_LEVELS];
	AssetID name;
	AssetID description[MAX_ABILITY_LEVELS];
	static AbilityInfo list[(s32)Ability::count];
};

struct Team
{
	struct SensorTrack
	{
		Ref<Entity> entity;
		r32 timer;
		b8 tracking;
	};

	struct SensorTrackHistory
	{
		Vec3 pos;
		u16 hp;
		u16 hp_max;
	};

	static const Vec4 ui_color_enemy;
	static const Vec4 ui_color_friend;
	static const Vec4 color_enemy;
	static const Vec4 color_friend;
	static r32 control_point_timer;

	static inline const Vec4& ui_color(AI::Team me, AI::Team them)
	{
		return me == them ? ui_color_friend : ui_color_enemy;
	}

	static inline const Vec4& color(AI::Team me, AI::Team them)
	{
		return me == them ? color_friend : color_enemy;
	}

	static StaticArray<Team, (s32)AI::Team::count> list;

	static b8 game_over();
	static b8 is_draw();

	Ref<Transform> player_spawn;
	Revision revision;
	r32 victory_timer;
	SensorTrack player_tracks[MAX_PLAYERS];
	SensorTrackHistory player_track_history[MAX_PLAYERS];
	b8 stealth_enable;

	Team();
	void awake();

	static void extract_history(PlayerManager*, SensorTrackHistory*);
	b8 has_player() const;
	b8 is_local() const;
	void track(PlayerManager*, PlayerManager*);

	static void update_all(const Update&);

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
	u8 ability_level[(s32)Ability::count];
	b8 ready;
	r32 spawn_ability_timer;
	r32 upgrade_timer;
	Ability current_spawn_ability;
	Ability current_upgrade_ability;
	LinkArg<Ability> ability_spawned;

	static b8 all_ready();

	b8 ability_spawn_start(Ability);
	void ability_spawn_stop(Ability);
	void ability_spawn_complete();
	b8 ability_upgrade_start(Ability);
	b8 ability_upgrade_complete();
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