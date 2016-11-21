#pragma once
#include "data/array.h"
#include "ai.h"
#include "input.h"
#include "render/render.h"
#include "render/views.h"
#include <unordered_map>
#include "constants.h"

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
		count,
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

	enum class MatchResult
	{
		None,
		Victory,
		Loss,
		Forfeit,
		NetworkError,
		OpponentQuit,
		Draw,
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
		MatchResult last_match;
		AssetID last_level;
		b8 story_mode;

		Session();
		void reset();
		r32 effective_time_scale() const;
		s32 local_player_count() const;
		s32 team_count() const;
	};

	enum class ZoneState
	{
		Inaccessible,
		Locked,
		Friendly,
		Hostile,
		Owned,
	};

	enum class Resource
	{
		Energy,
		HackKits,
		Drones,
		count,
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
		ZoneState zones[MAX_ZONES];
		Group group;
		s16 story_index;
		s16 resources[(s32)Resource::count];
		char username[MAX_USERNAME + 1];
		b8 cora_called;

		Save();
	};

	struct Level
	{
		FeatureLevel feature_level;
		r32 time_limit;
		r32 min_y;
		Type type;
		Mode mode;
		Skybox::Config skybox;
		AssetID id = AssetNull;
		Ref<Transform> map_view;
		s16 respawns;
		s16 kill_limit;
		b8 local = true;
		b8 continue_match_after_death;
		AI::Team team_lookup[MAX_PLAYERS];

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
	static b8 cancel_event_eaten[MAX_GAMEPADS];
	static b8 is_gamepad;
	static b8 quit;

	static b8 init(LoopSync*);
	static void execute(const Update&, const char*);
	static void update(const Update&);
	static void schedule_load_level(AssetID, Mode);
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
