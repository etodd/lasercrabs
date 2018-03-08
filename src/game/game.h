#pragma once
#include "data/array.h"
#include "ai.h"
#include "input.h"
#include "render/render.h"
#include "render/views.h"
#include "constants.h"
#include "master.h"

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
			Ref<Entity> entity;
			char name[256];
		};

		Array<NameEntry> map;
		Entity* find(const char*) const;
		void add(const char*, Entity*);
	};

	struct ForceField;
	struct SpawnPoint;

#if DEBUG
#define DEBUG_AI_CONTROL 0
#endif

struct Game
{
	enum class Mode : s8
	{
		Special,
		Parkour,
		Pvp,
		None,
		count = None,
	};

	enum class FeatureLevel : s8
	{
		Base,
		Batteries,
		Abilities,
		TutorialAll,
		All,
		count,
	};

	struct Session
	{
		u64 local_player_uuids[MAX_GAMEPADS];
		r32 time_scale;
		r32 grapple_time_scale;
		r32 zone_under_attack_timer;
		Net::Master::ServerConfig config;
		AssetID zone_under_attack = AssetNull;
		s8 local_player_mask;
		SessionType type;

		Session();
		void reset(SessionType);
		r32 effective_time_scale() const;
		s32 local_player_count() const;
	};

	struct TramTrack
	{
		struct Point
		{
			Vec3 pos;
			r32 offset;
		};
		StaticArray<Point, 32> points;
		s32 energy_threshold;
		AssetID level;
	};

	struct CollectibleEntry
	{
		AssetID zone;
		ID id;
	};

	enum class Group : s8
	{
		None,
		WuGang,
		Ephyra,
		count,
	};

	enum class PreinitResult : s8
	{
		Success,
		Failure,
		Restarting,
		count,
	};

	struct Save
	{
		r64 timestamp;
		Array<CollectibleEntry> collectibles;
		s32 locke_index;
		ZoneState zones[MAX_ZONES];
		Group group;
		s32 resources[s32(Resource::count)];
		AssetID zone_last;
		AssetID zone_current;
		AssetID zone_overworld;
		b8 locke_spoken;
		b8 tutorial_complete;
		b8 inside_terminal;

		Save();
		void reset();
	};

	struct WaterSoundNegativeSpace
	{
		Vec3 pos;
		r32 radius;
	};

	struct BatterySpawnPoint
	{
		Vec3 pos;
		Ref<SpawnPoint> spawn_point;
		s8 order;
	};

	struct Level
	{
		StaticArray<AI::PathZone, 16> path_zones;
		EntityFinder finder;
		StaticArray<TramTrack, 3> tram_tracks;
		StaticArray<AI::Config, MAX_PLAYERS> ai_config;
		StaticArray<DirectionalLight, MAX_DIRECTIONAL_LIGHTS> directional_lights;
		Net::Master::ServerConfig config_scheduled;
		Vec3 ambient_color;
		r32 min_y;
		r32 rotation;
		r32 rain;
		r32 asteroids;
		s32 max_teams;
		Skybox::Config skybox;
		StaticArray<WaterSoundNegativeSpace, 4> water_sound_negative_spaces;
		StaticArray<Clouds::Config, 4> clouds;
		StaticArray<SkyDecals::Config, 4> sky_decals;
		StaticArray<AssetID, 8> scripts;
		StaticArray<BatterySpawnPoint, 12> battery_spawns;
		AssetID id = AssetNull;
		AssetID multiplayer_level_scheduled = AssetNull;
		Ref<Entity> terminal;
		Ref<Entity> terminal_interactable;
		Ref<Entity> shop;
		AI::Team team_lookup[MAX_TEAMS];
		FeatureLevel feature_level;
		Mode mode;
		StoryModeTeam story_mode_team;
		char ambience1[MAX_AUDIO_EVENT_NAME + 1];
		char ambience2[MAX_AUDIO_EVENT_NAME + 1];
		s8 battery_spawn_index;
		s8 battery_spawn_group_size;
		b8 local = true;
		b8 noclip;
		b8 config_scheduled_apply; // true if we have a scheduled ServerConfig we need to apply on the next level transition

		b8 has_feature(FeatureLevel) const;
		AI::Team team_lookup_reverse(AI::Team) const;
		void multiplayer_level_schedule();
		const StaticArray<DirectionalLight, MAX_DIRECTIONAL_LIGHTS>& directional_lights_get() const;
		const Vec3& ambient_color_get() const;
		r32 far_plane_get() const;
		r32 fog_start_get() const;
		r32 fog_end_get() const;
	};

	static Session session;
	static Save save;
	static Level level;

	static Gamepad::Type ui_gamepad_types[MAX_GAMEPADS];

	static Array<UpdateFunction> updates;
	static Array<DrawFunction> draws;
	static Array<CleanupFunction> cleanups;
	static ScreenQuad screen_quad;
	static GameTime time;
	static GameTime real_time;
	static r32 inactive_timer;
	static r64 platform_time;
	static r32 physics_timestep;
	static r32 schedule_timer;
	static Mode scheduled_mode;
	static AssetID scheduled_load_level;
	static AssetID scheduled_dialog;
	static b8 cancel_event_eaten[MAX_GAMEPADS];
	static b8 quit;
	static b8 minimize;
	static Net::Master::AuthType auth_type;
	static const char* language;
	static u8 auth_key[MAX_AUTH_KEY + 1];
	static s32 auth_key_length;
	static Net::Master::UserKey user_key;
#if !defined(__ORBIS__)
	static char steam_username[MAX_USERNAME + 1];
#endif

	static u32 steam_app_id;
	static PreinitResult pre_init(const char**);
	static const char* init(LoopSync*);
	static void execute(const char*);
	static void update(InputState*, const InputState*);
	static void schedule_load_level(AssetID, Mode, r32 = 0.0f);
	static void unload_level();
	static void load_level(AssetID, Mode, StoryModeTeam = StoryModeTeam::Attack);
	static void awake_all();
	static void draw_opaque(const RenderParams&);
	static void draw_hollow(const RenderParams&);
	static void draw_override(const RenderParams&);
	static void draw_particles(const RenderParams&);
	static void draw_alpha(const RenderParams&);
	static void draw_alpha_late(const RenderParams&);
	static void draw_additive(const RenderParams&);
	static void auth_failed();
	static void term();

	static b8 should_pause();
	static b8 hi_contrast();
	static void remove_bots_if_necessary(s32);
	static void add_local_player(s8);

	static b8 edge_trigger(r32, b8(*)(r32));
	static b8 edge_trigger(r32, r32, b8(*)(r32, r32));
	static b8 net_transform_filter(const Entity*, Mode);
};

}
