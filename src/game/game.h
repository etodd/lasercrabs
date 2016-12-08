#pragma once
#include "data/array.h"
#include "ai.h"
#include "input.h"
#include "render/render.h"
#include "render/views.h"
#include <unordered_map>
#include "constants.h"
#include "net_serialize.h"

namespace VI
{

struct RenderParams;
struct Transform;

typedef void(*UpdateFunction)(const Update&);
typedef void(*DrawFunction)(const RenderParams&);
typedef void(*CleanupFunction)();

struct EntityFinder
{
	struct NameEntry
	{
		const char* name;
		Ref<Entity> entity;
		cJSON* properties;
	};

	Array<NameEntry> map;
	Entity* find(const char*) const;
};

#if DEBUG
#define DEBUG_AI_CONTROL 0
#endif

struct Game
{
	enum class Mode
	{
		Special,
		Parkour,
		Pvp,
		None,
		count = None,
	};

	enum class Group
	{
		None,
		Futifs,
		Zodiak,
		count,
	};

	enum class FeatureLevel
	{
		Base,
		EnergyPickups,
		Abilities,
		All,
		count,
	};

	enum class Type
	{
		Rush,
		Deathmatch,
		count,
	};

	struct Session
	{
		AI::Team local_player_config[MAX_GAMEPADS];
		u64 local_player_uuids[MAX_GAMEPADS];
		r32 time_scale;
		b8 story_mode;

		Session();
		void reset();
		r32 effective_time_scale() const;
		s32 local_player_count() const;
		s32 team_count() const;
	};

	enum class ZoneState
	{
		Locked,
		Friendly,
		Hostile,
	};

	struct Message
	{
		r64 timestamp;
		AssetID contact;
		AssetID text;
		b8 read;
	};

	struct Save
	{
		r64 timestamp;
		Array<Message> messages;
		Array<Message> messages_scheduled;
		std::unordered_map<AssetID, AssetID> variables; // todo: kill STL
		Vec3 zone_current_restore_position;
		r32 zone_current_restore_rotation;
		ZoneState zones[MAX_ZONES];
		Group group;
		s16 story_index;
		s16 resources[s32(Resource::count)];
		AssetID zone_last;
		AssetID zone_current;
		AssetID zone_overworld;
		char username[MAX_USERNAME + 1];
		b8 cora_called;
		b8 zone_current_restore;

		Save();
	};

	struct TramTrack
	{
		struct Point
		{
			Vec3 pos;
			r32 offset;
		};
		StaticArray<Point, 32> points;
		AssetID level;
	};

	struct Level
	{
		StaticArray<TramTrack, 3> tram_tracks;
		r32 time_limit;
		r32 min_y;
		r32 rotation;
		s32 max_teams;
		FeatureLevel feature_level;
		Type type;
		Mode mode;
		StaticArray<AI::Config, MAX_PLAYERS> ai_config;
		Skybox::Config skybox;
		AssetID id = AssetNull;
		Ref<Transform> map_view;
		Ref<Entity> terminal;
		Ref<Entity> terminal_interactable;
		s16 respawns;
		s16 kill_limit;
		AI::Team team_lookup[MAX_PLAYERS];
		b8 local = true;
		b8 continue_match_after_death;
		b8 post_pvp; // true if we've already played a PvP match on this level

		b8 has_feature(FeatureLevel) const;
	};

	static Session session;
	static Save save;
	static Level level;

	static GameTime time;
	static GameTime real_time;
	static Array<UpdateFunction> updates;
	static Array<DrawFunction> draws;
	static Array<CleanupFunction> cleanups;
	static r32 physics_timestep;
	static AssetID scheduled_load_level;
	static Mode scheduled_mode;
	static r32 schedule_timer;
	static b8 cancel_event_eaten[MAX_GAMEPADS];
	static b8 is_gamepad;
	static b8 quit;

	static b8 init(LoopSync*);
	static void execute(const Update&, const char*);
	static void update(const Update&);
	static void schedule_load_level(AssetID, Mode, r32 = 0.0f);
	static void unload_level();
	static void load_level(const Update&, AssetID, Mode, b8 = false);
	static void draw_opaque(const RenderParams&);
	static void draw_hollow(const RenderParams&);
	static void draw_particles(const RenderParams&);
	static void draw_alpha(const RenderParams&);
	static void draw_additive(const RenderParams&);
	static void draw_override(const RenderParams&);
	static void term();
};

}
