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
			Turrets,
			All,
			count,
		};

		struct Session
		{
			u64 local_player_uuids[MAX_GAMEPADS];
			r32 time_scale;
			r32 zone_under_attack_timer;
			Net::Master::ServerConfig config;
			AssetID zone_under_attack = AssetNull;
			s8 local_player_mask;
			SessionType type;

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
			StaticArray<DirectionalLight, MAX_DIRECTIONAL_LIGHTS> directional_lights;
			Vec3 ambient_color;
			r32 min_y;
			r32 rotation;
			r32 rain;
			s32 max_teams;
			FeatureLevel feature_level;
			Mode mode;
			Skybox::Config skybox;
			StaticArray<Clouds::Config, 4> clouds;
			StaticArray<AssetID, 8> scripts;
			AssetID id = AssetNull;
			Ref<Transform> map_view;
			Ref<Entity> terminal;
			Ref<Entity> terminal_interactable;
			Ref<Entity> shop;
			AI::Team team_lookup[MAX_TEAMS];
			b8 local = true;
			b8 continue_match_after_death;
			b8 post_pvp; // true if we've already played a PvP match on this level

			b8 has_feature(FeatureLevel) const;
			AI::Team team_lookup_reverse(AI::Team) const;
			const StaticArray<DirectionalLight, MAX_DIRECTIONAL_LIGHTS>& directional_lights_get() const;
			const Vec3& ambient_color_get() const;
		};

		static Session session;
		static Net::Master::Save save;
		static Level level;

		static Gamepad::Type ui_gamepad_types[MAX_GAMEPADS];

		static Array<UpdateFunction> updates;
		static Array<DrawFunction> draws;
		static Array<CleanupFunction> cleanups;
		static ScreenQuad screen_quad;
		static GameTime time;
		static GameTime real_time;
		static r32 inactive_timer;
		static r32 physics_timestep;
		static r32 schedule_timer;
		static s32 width;
		static s32 height;
		static Mode scheduled_mode;
		static AssetID scheduled_load_level;
		static AssetID scheduled_dialog;
		static b8 cancel_event_eaten[MAX_GAMEPADS];
		static b8 attract_mode;
		static b8 quit;
		static Net::Master::AuthType auth_type;
		static char auth_key[MAX_AUTH_KEY + 1];
		static Net::Master::UserKey user_key;

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
		static void auth_failed();
		static void term();

		static b8 edge_trigger(r32, b8(*)(r32));
		static b8 edge_trigger(r32, r32, b8(*)(r32, r32));
		static b8 net_transform_filter(const Entity*, Mode);
	};

}