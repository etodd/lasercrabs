#pragma once
#include "data/array.h"
#include "ai.h"
#include "input.h"
#include "render/render.h"
#include "render/views.h"
#include <unordered_map>

namespace VI
{

struct RenderParams;

typedef void(*UpdateFunction)(const Update&);
typedef void(*DrawFunction)(const RenderParams&);
typedef void(*CleanupFunction)();

struct EntityFinder
{
	struct NameEntry
	{
		const char* name;
		Ref<Entity> entity;
	};

	Array<NameEntry> map;
	void add(const char*, Entity*);
	Entity* find(const char*) const;
};

#if DEBUG
#define DEBUG_AI_CONTROL 1
#endif

struct Game
{
	enum class Mode
	{
		Special,
		Pvp,
	};

	enum class FeatureLevel
	{
		Base,
		HealthPickups,
		Abilities,
		All,
		count = All,
	};

	enum class NetworkQuality
	{
		Perfect,
		Okay,
		Bad,
	};

	enum class NetworkState
	{
		Normal,
		Lag,
		Recover,
	};

	enum class Forfeit
	{
		None,
		NetworkError,
		OpponentQuit,
	};

	struct State
	{
		Mode mode;
		NetworkQuality network_quality;
		NetworkState network_state;
		Forfeit forfeit;
		r32 network_time;
		r32 network_timer;
		AI::Team local_player_config[MAX_GAMEPADS];
		b8 third_person;
		b8 local_multiplayer;
		r32 time_scale;
		r32 effective_time_scale() const;
		b8 allow_double_jump;
		AI::Team teams;
		AssetID level;
		void reset();
		State();
	};

	struct Save
	{
		s32 level_index;
		s32 round;
		s32 rating;
		b8 last_round_loss;
		const char* username = "etodd";
		std::unordered_map<AssetID, AssetID> variables; // todo: kill STL
		void reset(AssetID);
	};

	struct Level
	{
		FeatureLevel feature_level;
		r32 min_y;
		Skybox::Config skybox;
		b8 lock_teams;
		b8 continue_match_after_death;

		b8 has_feature(FeatureLevel) const;
	};

	static State state;
	static Save save;
	static Level level;

	static const s32 levels[];
	static const s32 tutorial_levels = 1;

	static b8 quit;
	static GameTime time;
	static GameTime real_time;
	static r32 physics_timestep;
	static Vec2 cursor;
	static b8 cursor_updated;
	static b8 cursor_active;
	static AssetID scheduled_load_level;
	static Mode scheduled_mode;
	static Array<UpdateFunction> updates;
	static Array<DrawFunction> draws;
	static Array<CleanupFunction> cleanups;
	static b8 cancel_event_eaten[MAX_GAMEPADS];

	static b8 init(LoopSync*);
	static void execute(const Update&, const char*);
	static void draw_cursor(const RenderParams&);
	static void update_cursor(const Update&);
	static void update(const Update&);
	static void schedule_load_level(AssetID, Mode);
	static void unload_level();
	static void load_level(const Update&, AssetID, Mode, b8 = false);
	static void draw_opaque(const RenderParams&);
	static void draw_alpha(const RenderParams&);
	static void draw_additive(const RenderParams&);
	static void term();
};

}
