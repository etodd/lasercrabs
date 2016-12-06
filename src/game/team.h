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
	};
	Type type;
	AssetID icon;
	s16 spawn_cost;
	b8 rapid_fire;
	static AbilityInfo list[(s32)Ability::count];
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

	static const Vec4 ui_color_enemy;
	static const Vec4 ui_color_friend;
	static const Vec4 color_enemy;
	static const Vec4 color_friend;
	static r32 control_point_timer;
	static r32 game_over_real_time;
	static r32 transition_timer;
	static r32 match_time;
	static b8 game_over;
	static Ref<Team> winner;
	static Game::Mode transition_mode_scheduled;

	static void awake_all();
	static void transition_next();
	static s16 containment_field_mask(AI::Team);
	static void update(const Update&);
	static void update_all_server(const Update&);
	static void update_all_client_only(const Update&);
	static s32 teams_with_players();
	static Team* with_most_kills();
	static b8 net_msg(Net::StreamRead*);
	static void transition_mode(Game::Mode);
	static void draw_ui(const RenderParams&);

	static inline const Vec4& ui_color(AI::Team me, AI::Team them)
	{
		return them == AI::TeamNone ? UI::color_accent : (me == them ? ui_color_friend : ui_color_enemy);
	}

	static inline const Vec4& color(AI::Team me, AI::Team them)
	{
		return me == them ? color_friend : color_enemy;
	}

	SensorTrack player_tracks[MAX_PLAYERS];
	Ref<Transform> player_spawn;

	Team();
	void awake() {}
	b8 has_player() const;
	void track(PlayerManager*, Entity*);
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

	static s32 visibility_hash(const PlayerManager*, const PlayerManager*);
	static Ref<Entity> visibility[MAX_PLAYERS * MAX_PLAYERS];

	static void update_all(const Update&);

	static b8 net_msg(Net::StreamRead*, PlayerManager*, Net::MessageSource);

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
	Ref<Entity> instance;
	s16 credits;
	s16 credits_last;
	s16 kills;
	s16 respawns;
	char username[MAX_USERNAME + 1]; // +1 for null terminator
	b8 score_accepted;

	PlayerManager(Team* = nullptr, const char* = nullptr);
	void awake();
	~PlayerManager();

	Entity* decoy() const;
	State state() const;
	b8 can_transition_state() const;
	b8 has_upgrade(Upgrade) const;
	b8 is_local() const;
	s32 ability_count() const;
	b8 ability_valid(Ability) const;
	b8 upgrade_start(Upgrade);
	void upgrade_cancel();
	void upgrade_complete();
	b8 capture_start();
	void capture_cancel();
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
	void score_accept();
};


}