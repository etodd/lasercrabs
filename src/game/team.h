#pragma once

#include "data/entity.h"
#include "lmath.h"
#include "physics.h"
#include "render/ui.h"
#include "ai.h"
#include "game.h"

namespace VI
{


struct Camera;
struct Transform;
struct Target;
struct TargetEvent;
struct PlayerManager;
struct SpawnPosition;
struct SpawnPoint;
namespace Net
{
	struct StreamRead;
}

struct AbilityInfo
{
	enum class Type : s8
	{
		Build,
		Shoot,
		Other,
		count,
	};

	static AbilityInfo list[s32(Ability::count)];

	AssetID icon;
	s16 spawn_cost;
	Type type;
	b8 rapid_fire;
};

struct UpgradeInfo
{
	AssetID name;
	AssetID description;
	AssetID icon;
	s16 cost;
	static UpgradeInfo list[s32(Upgrade::count)];
};

#define PLAYER_SCORE_SUMMARY_ITEMS 4

struct Team : public ComponentType<Team>
{
	enum class MatchState : s8
	{
		Waiting,
		TeamSelect,
		Active,
		Done,
		count,
	};

	struct SensorTrack
	{
		r32 timer;
		Ref<Entity> entity;
		b8 tracking;
	};

	struct ScoreSummaryItem
	{
		s32 amount;
		Ref<PlayerManager> player;
		AI::Team team;
		char label[512];
	};

	static const Vec4 ui_color_enemy;
	static const Vec4 ui_color_friend;
	static const Vec4 color_enemy;
	static const Vec4 color_friend;
	static StaticArray<ScoreSummaryItem, MAX_PLAYERS * PLAYER_SCORE_SUMMARY_ITEMS> score_summary;
	static r32 control_point_timer;
	static r32 game_over_real_time;
	static r32 transition_timer;
	static r32 match_time;
	static Ref<Team> winner;
	static Game::Mode transition_mode_scheduled;
	static MatchState match_state;

	static void awake_all();
	static void transition_next();
	static s16 force_field_mask(AI::Team);
	static void update(const Update&);
	static void update_all_server(const Update&);
	static void update_all_client_only(const Update&);
	static s32 teams_with_active_players();
	static Team* with_most_kills();
	static Team* with_least_players(s32* = nullptr);
	static b8 net_msg(Net::StreamRead*, Net::MessageSource);
	static void transition_mode(Game::Mode);
	static void draw_ui(const RenderParams&);
	static void match_start();
	static void match_team_select();

	static inline const Vec4& ui_color(AI::Team me, AI::Team them)
	{
		return them == AI::TeamNone ? UI::color_accent() : (me == them ? ui_color_friend : ui_color_enemy);
	}

	static inline const Vec4& color(AI::Team me, AI::Team them)
	{
		return me == them ? color_friend : color_enemy;
	}

	SensorTrack player_tracks[MAX_PLAYERS];
	s16 kills;

	void awake() {}
	b8 has_active_player() const;
	void track(PlayerManager*, Entity*);
	s32 player_count() const;
	s16 increment() const;
	void add_kills(s32);

	inline AI::Team team() const
	{
		return AI::Team(id());
	}
};

struct PlayerManager : public ComponentType<PlayerManager>
{
	enum State : s8
	{
		Default,
		Upgrading,
		count,
	};

	enum class Message : s8
	{
		CanSpawn,
		ScoreAccept,
		SpawnSelect,
		UpgradeCompleted,
		UpdateCounts,
		SetInstance,
		MakeAdmin,
		TeamSchedule,
		TeamSwitch,
		count,
	};

	struct Visibility
	{
		enum class Type : s8
		{
			Direct,
			Indirect,
			count,
		};

		Ref<Entity> entity;
		Type type;
	};

	static s32 visibility_hash(const PlayerManager*, const PlayerManager*);
	static Visibility visibility[MAX_PLAYERS * MAX_PLAYERS];

	static void update_all(const Update&);
	static b8 net_msg(Net::StreamRead*, PlayerManager*, Message, Net::MessageSource);
	static PlayerManager* owner(const Entity*);
	static void entity_killed_by(Entity*, Entity*);

	r32 spawn_timer;
	r32 state_timer;
	r32 ability_purchase_times[MAX_ABILITIES];
	s32 upgrades;
	LinkArg<const SpawnPosition&> spawn;
	LinkArg<Upgrade> upgrade_completed;
	Ref<Team> team;
	Ref<Entity> instance;
	s16 energy;
	s16 kills;
	s16 deaths;
	s16 respawns;
	char username[MAX_USERNAME + 1]; // +1 for null terminator
	Ability abilities[MAX_ABILITIES];
	Upgrade current_upgrade;
	AI::Team team_scheduled;
	b8 score_accepted;
	b8 can_spawn;
	b8 is_admin;

	PlayerManager(Team* = nullptr, const char* = nullptr);
	void awake();
	~PlayerManager();

	void make_admin();
	void set_instance(Entity*);
	void spawn_select(SpawnPoint*);
	void clear_ownership();
	State state() const;
	b8 can_transition_state() const;
	b8 has_upgrade(Upgrade) const;
	b8 is_local() const;
	s32 ability_count() const;
	b8 ability_valid(Ability) const;
	b8 upgrade_start(Upgrade);
	void upgrade_complete();
	Upgrade upgrade_highest_owned_or_available() const;
	b8 upgrade_available(Upgrade = Upgrade::None) const;
	s16 upgrade_cost(Upgrade) const;
	void add_energy(s32);
	void add_energy_and_notify(s32);
	void add_kills(s32);
	void add_deaths(s32);
	void update_server(const Update&);
	void score_accept();
	void set_can_spawn(b8 = true);
	void team_schedule(AI::Team);
};


}