#pragma once

#include "data/entity.h"
#include "lmath.h"
#include "physics.h"
#include "render/ui.h"
#include "ai.h"
#include "game.h"

// NOTE: this is also the max number of teams
// if you change this, make sure to allocate more physics categories for each team's containment field
#define MAX_PLAYERS 4

namespace VI
{


struct Camera;
struct Transform;
struct Target;
struct TargetEvent;
struct PlayerManager;


#define PLAYER_SPAWN_DELAY 3.0f
#define GAME_BUY_PERIOD (10.0f + PLAYER_SPAWN_DELAY)
#define CREDITS_INITIAL 60
#define CREDITS_MINION_KILL 10
#define CREDITS_SENSOR_DESTROY 10
#define CREDITS_CONTAINMENT_FIELD_DESTROY 10
#define CREDITS_DEFAULT_INCREMENT 5
#define CREDITS_CONTROL_POINT 5
#define CREDITS_ENERGY_PICKUP 5
#define CREDITS_CAPTURE_CONTROL_POINT 10
#define CREDITS_CAPTURE_ENERGY_PICKUP 10
#define MAX_ABILITIES 3

#define UPGRADE_TIME 1.5f
#define CAPTURE_TIME 3.0f

enum class Ability
{
	Sensor,
	Minion,
	Teleporter,
	Rocket,
	ContainmentField,
	Sniper,
	Decoy,
	count,
	None,
};

struct AbilityInfo
{
	AssetID icon;
	s16 spawn_cost;
	static AbilityInfo list[(s32)Ability::count];
};

enum class Upgrade
{
	Sensor,
	Minion,
	Teleporter,
	Rocket,
	ContainmentField,
	Sniper,
	Decoy,
	count,
	None = count,
};

struct UpgradeInfo
{
	AssetID name;
	AssetID description;
	AssetID icon;
	s16 cost;
	static UpgradeInfo list[(s32)Upgrade::count];
};

struct Team : public ComponentType<Team>
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
		s8 hp;
		s8 hp_max;
		s8 shield;
		s8 shield_max;
	};

	static const Vec4 ui_color_enemy;
	static const Vec4 ui_color_friend;
	static const Vec4 color_enemy;
	static const Vec4 color_friend;
	static r32 control_point_timer;
	static r32 game_over_real_time;
	static b8 game_over;
	static Ref<Team> winner;

	static void awake_all();
	static void extract_history(PlayerManager*, SensorTrackHistory*);
	static void transition_next(Game::MatchResult);
	static s16 containment_field_mask(AI::Team);
	static void update_all(const Update&);
	static s32 teams_with_players();
	static Team* with_most_kills();

	static inline const Vec4& ui_color(AI::Team me, AI::Team them)
	{
		return them == AI::TeamNone ? UI::color_accent : (me == them ? ui_color_friend : ui_color_enemy);
	}

	static inline const Vec4& color(AI::Team me, AI::Team them)
	{
		return me == them ? color_friend : color_enemy;
	}

	SensorTrack player_tracks[MAX_PLAYERS];
	SensorTrackHistory player_track_history[MAX_PLAYERS];
	Ref<Transform> player_spawn;

	Team();
	void awake() {}
	b8 has_player() const;
	void track(PlayerManager*);
	s32 control_point_count() const;
	s16 kills() const;

	inline AI::Team team() const
	{
		return (AI::Team)id();
	}
};

struct ControlPoint;

struct PlayerManager : public ComponentType<PlayerManager>
{
	struct SummaryItem
	{
		AssetID label;
		s32 amount;
	};

	enum State
	{
		Default,
		Upgrading,
		Capturing,
		count,
	};

	static r32 timer;

	static void update_all(const Update&);

	r32 spawn_timer;
	r32 credits_flash_timer;
	r32 particle_accumulator;
	r32 state_timer;
	s32 upgrades;
	StaticArray<SummaryItem, 1> credits_summary;
	Ability abilities[MAX_ABILITIES];
	Upgrade current_upgrade;
	Link spawn;
	LinkArg<Upgrade> upgrade_completed;
	LinkArg<b8> control_point_capture_completed;
	Ref<Team> team;
	Ref<Entity> entity;
	s16 credits;
	s16 kills;
	s16 respawns;
	char username[MAX_USERNAME + 1]; // +1 for null terminator
	b8 score_accepted;

	PlayerManager(Team* = nullptr);
	void awake() {}

	Entity* decoy() const;
	State state() const;
	b8 can_transition_state() const;
	b8 has_upgrade(Upgrade) const;
	b8 is_local() const;
	s32 ability_count() const;
	b8 ability_valid(Ability) const;
	b8 upgrade_start(Upgrade);
	void upgrade_complete();
	b8 capture_start();
	void capture_complete();
	b8 upgrade_available(Upgrade = Upgrade::None) const;
	s16 upgrade_cost(Upgrade) const;
	s32 add_credits(s32);
	void add_kills(s32);
	b8 at_upgrade_point() const;
	ControlPoint* at_control_point() const;
	b8 friendly_control_point(const ControlPoint*) const;
	s16 increment() const;
	void update_server(const Update&);
	void update_client(const Update&);
};


}