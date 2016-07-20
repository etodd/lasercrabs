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


#define PLAYER_SPAWN_DELAY 5.0f
#define GAME_TIME_LIMIT ((60.0f * 10.0f) + PLAYER_SPAWN_DELAY)
#define GAME_BUY_PERIOD (10.0f + PLAYER_SPAWN_DELAY)
#define CREDITS_INITIAL 60
#define CREDITS_MINION 10
#define CREDITS_SENSOR_DESTROY 10
#define CREDITS_CONTAINMENT_FIELD_DESTROY 10
#define CREDITS_DAMAGE 15
#define CREDITS_DETECT 10
#define CREDITS_CONTROL_POINT 5
#define CREDITS_DEFAULT_INCREMENT 5
#define MAX_ABILITIES 3

#define UPGRADE_TIME 1.5f

// if the ability cooldown is lower than this, we can use the ability
// we should flash the ability icon during this time to indicate the ability is now usable

enum class Ability
{
	Sensor,
	Rocket,
	Minion,
	ContainmentField,
	count,
	None = count,
};

struct AbilityInfo
{
	AssetID icon;
	r32 spawn_time;
	u16 spawn_cost;
	static AbilityInfo list[(s32)Ability::count];
};

enum class Upgrade
{
	Sensor,
	Rocket,
	Minion,
	ContainmentField,
	HealthSteal,
	HealthBuff,
	count,
	None = count,
};

struct UpgradeInfo
{
	AssetID name;
	AssetID description;
	AssetID icon;
	u16 cost;
	static UpgradeInfo list[(s32)Upgrade::count];
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
	static r32 game_over_real_time;

	static StaticArray<Team, (s32)AI::Team::count> list;

	static b8 game_over;
	static Ref<Team> winner;

	static inline const Vec4& ui_color(AI::Team me, AI::Team them)
	{
		return me == them ? ui_color_friend : ui_color_enemy;
	}

	static inline const Vec4& color(AI::Team me, AI::Team them)
	{
		return me == them ? color_friend : color_enemy;
	}

	static void awake_all();

	static void extract_history(PlayerManager*, SensorTrackHistory*);

	static void level_retry();
	static void level_next();

	static s32 containment_field_mask(AI::Team);

	Ref<Transform> player_spawn;
	Revision revision;
	SensorTrack player_tracks[MAX_PLAYERS];
	SensorTrackHistory player_track_history[MAX_PLAYERS];

	Team();
	b8 has_player() const;
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
	struct RatingItem
	{
		AssetID label;
		s32 amount;
	};

	static PinArray<PlayerManager, MAX_PLAYERS> list;

	u16 hp_start;
	r32 spawn_timer;
	Revision revision;
	char username[255];
	u16 credits;
	r32 credits_flash_timer;
	b8 score_accepted;
	Ref<Team> team;
	Ref<Entity> entity;
	Link spawn;
	u32 upgrades;
	Ability abilities[MAX_ABILITIES];
	b8 has_upgrade(Upgrade) const;
	r32 spawn_ability_timer;
	r32 upgrade_timer;
	Ability current_spawn_ability;
	Upgrade current_upgrade;
	LinkArg<Ability> ability_spawned;
	LinkArg<Ability> ability_spawn_canceled;
	LinkArg<Upgrade> upgrade_completed;
	StaticArray<RatingItem, 8> rating_summary;

	b8 is_local() const;
	s32 ability_count() const;
	b8 ability_spawn_start(Ability);
	void ability_spawn_stop(Ability);
	void ability_spawn_complete();
	b8 upgrade_start(Upgrade);
	void upgrade_complete();
	b8 upgrade_available(Upgrade = Upgrade::None) const;
	u16 upgrade_cost(Upgrade) const;

	s32 add_credits(s32);

	PlayerManager(Team*, u16);

	inline ID id() const
	{
		return this - &list[0];
	}

	b8 at_spawn() const;

	void update(const Update&);
};


}