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
	enum class Type
	{
		Build,
		Shoot,
		Other,
		count,
	};
	Type type;
	AssetID icon;
	s16 spawn_cost;
	b8 rapid_fire;
	static AbilityInfo list[s32(Ability::count)];
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
	struct SensorTrack
	{
		Ref<Entity> entity;
		r32 timer;
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
	static b8 game_over;
	static Ref<Team> winner;
	static Game::Mode transition_mode_scheduled;

	static void awake_all();
	static void transition_next();
	static s16 force_field_mask(AI::Team);
	static void update(const Update&);
	static void update_all_server(const Update&);
	static void update_all_client_only(const Update&);
	static s32 teams_with_active_players();
	static Team* with_most_kills();
	static b8 net_msg(Net::StreamRead*);
	static void transition_mode(Game::Mode);
	static void draw_ui(const RenderParams&);
	static void launch_rockets();

	static inline const Vec4& ui_color(AI::Team me, AI::Team them)
	{
		return them == AI::TeamNone ? UI::color_accent : (me == them ? ui_color_friend : ui_color_enemy);
	}

	static inline const Vec4& color(AI::Team me, AI::Team them)
	{
		return me == them ? color_friend : color_enemy;
	}

	SensorTrack player_tracks[MAX_PLAYERS];

	Team();
	void awake() {}
	b8 has_active_player() const;
	void track(PlayerManager*, Entity*);
	s32 player_count() const;
	s16 kills() const;
	s16 increment() const;

	inline AI::Team team() const
	{
		return (AI::Team)id();
	}
};

struct PlayerManager : public ComponentType<PlayerManager>
{
	enum State
	{
		Default,
		Upgrading,
		count,
	};

	struct Visibility
	{
		enum class Type
		{
			Direct,
			Indirect,
			count,
		};

		Type type;
		Ref<Entity> entity;
	};

	static s32 visibility_hash(const PlayerManager*, const PlayerManager*);
	static Visibility visibility[MAX_PLAYERS * MAX_PLAYERS];

	static void update_all(const Update&);
	static b8 net_msg(Net::StreamRead*, PlayerManager*, Net::MessageSource);
	static PlayerManager* owner(Entity*);
	static void entity_killed_by(Entity*, Entity*);

	r32 spawn_timer;
	r32 state_timer;
	s32 upgrades;
	Ability abilities[MAX_ABILITIES];
	Upgrade current_upgrade;
	LinkArg<const SpawnPosition&> spawn;
	LinkArg<Upgrade> upgrade_completed;
	Ref<Team> team;
	Ref<Entity> instance;
	s16 energy;
	s16 kills;
	s16 deaths;
	s16 respawns;
	char username[MAX_USERNAME + 1]; // +1 for null terminator
	b8 score_accepted;
	b8 can_spawn;

	PlayerManager(Team* = nullptr, const char* = nullptr);
	void awake();
	~PlayerManager();

	void spawn_select(SpawnPoint*);
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
	b8 at_spawn_point() const;
	void update_server(const Update&);
	void update_client(const Update&);
	void score_accept();
	void set_can_spawn();
};


}