#pragma once
#include "data/array.h"
#include "ai.h"
#include "input.h"
#include "render/render.h"
#include "render/views.h"
#include <unordered_map>
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

		enum class FeatureLevel
		{
			Base,
			EnergyPickups,
			Abilities,
			AbilitiesDone,
			TutorialAll,
			All,
			count,
		};

		struct Session
		{
			AI::Team local_player_config[MAX_GAMEPADS];
			u64 local_player_uuids[MAX_GAMEPADS];
			r32 time_scale;
			r32 time_limit;
			r32 zone_under_attack_timer;
			SessionType type;
			GameType game_type;
			AssetID zone_under_attack = AssetNull;
			s16 respawns;
			s16 kill_limit;
			s8 player_slots;
			s8 team_count;

			Session();
			void reset();
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
			AssetID level;
		};

		struct Level
		{
			EntityFinder finder;
			StaticArray<TramTrack, 3> tram_tracks;
			StaticArray<AI::Config, MAX_PLAYERS> ai_config;
			r32 time_limit;
			r32 min_y;
			r32 rotation;
			r32 rain;
			s32 max_teams;
			FeatureLevel feature_level;
			GameType type;
			Mode mode;
			Skybox::Config skybox;
			StaticArray<Clouds::Config, 4> clouds;
			StaticArray<AssetID, 8> scripts;
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
			AI::Team team_lookup_reverse(AI::Team) const;
		};

		static Session session;
		static Net::Master::Save save;
		static Level level;

		static Gamepad::Type ui_gamepad_types[MAX_GAMEPADS];

		static GameTime time;
		static GameTime real_time;
		static r32 inactive_timer;
		static b8 enable_attract;
		static ScreenQuad screen_quad;
		static Array<UpdateFunction> updates;
		static Array<DrawFunction> draws;
		static Array<CleanupFunction> cleanups;
		static r32 physics_timestep;
		static AssetID scheduled_load_level;
		static AssetID scheduled_dialog;
		static Mode scheduled_mode;
		static r32 schedule_timer;
		static b8 cancel_event_eaten[MAX_GAMEPADS];
		static b8 quit;
		static s32 width;
		static s32 height;

		static b8 init(LoopSync*);
		static void execute(const char*);
		static void update(const Update&);
		static void schedule_load_level(AssetID, Mode, r32 = 0.0f);
		static void unload_level();
		static void load_level(AssetID, Mode, b8 = false);
		static void awake_all();
		static void draw_opaque(const RenderParams&);
		static void draw_hollow(const RenderParams&);
		static void draw_particles(const RenderParams&);
		static void draw_alpha(const RenderParams&);
		static void draw_alpha_late(const RenderParams&);
		static void draw_additive(const RenderParams&);
		static void draw_override(const RenderParams&);
		static void term();

		static b8 net_transform_filter(const Entity*, Mode);
	};

}