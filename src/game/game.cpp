#include "types.h"
#include "vi_assert.h"

#include "render/views.h"
#include "render/render.h"
#include "render/skinned_model.h"
#include "data/animator.h"
#include "data/array.h"
#include "data/entity.h"
#include "data/components.h"
#include "data/ragdoll.h"
#include "drone.h"
#include "player.h"
#include "physics.h"
#include "entities.h"
#include "walker.h"
#include "common.h"
#include "render/ui.h"
#include "asset/armature.h"
#include "asset/texture.h"
#include "asset/mesh.h"
#include "asset/shader.h"
#include "asset/lookup.h"
#include "asset/soundbank.h"
#include "asset/font.h"
#include "asset/Wwise_IDs.h"
#include "asset/level.h"
#include "asset/animation.h"
#include "strings.h"
#include "input.h"
#include "mersenne/mersenne-twister.h"
#include <time.h>
#include "cjson/cJSON.h"
#include "audio.h"
#include "menu.h"
#include "scripts.h"
#include "console.h"
#include "ease.h"
#include "minion.h"
#include "render/particles.h"
#include "ai_player.h"
#include "usernames.h"
#include "net.h"
#include "parkour.h"
#include "overworld.h"
#include "team.h"
#include "load.h"
#include <dirent.h>
#include "settings.h"
#include "data/json.h"
#include "asset/version.h"
#if !SERVER && !defined(__ORBIS__)
#include "steam/steam_api.h"
#include "discord/include/discord-rpc.h"
#endif
#include "data/unicode.h"

#if DEBUG
	#define DEBUG_WALK_NAV_MESH 0
	#define DEBUG_WALK_AI_PATH 0
	#define DEBUG_DRONE_AI_PATH 0
	#define DEBUG_PHYSICS 0
#endif

#include "game.h"

namespace VI
{

b8 Game::quit;
b8 Game::minimize;
GameTime Game::time;
GameTime Game::real_time;
r64 Game::platform_time;
r32 Game::physics_timestep;
r32 Game::inactive_timer;
Net::Master::AuthType Game::auth_type;
const char* Game::language;
u8 Game::auth_key[MAX_AUTH_KEY + 1];
s32 Game::auth_key_length;
Net::Master::UserKey Game::user_key;
#if !defined(__ORBIS__)
char Game::steam_username[MAX_USERNAME + 1];
#endif

Gamepad::Type Game::ui_gamepad_types[MAX_GAMEPADS] = { };
AssetID Game::scheduled_load_level = AssetNull;
AssetID Game::scheduled_dialog = AssetNull;
Game::Mode Game::scheduled_mode = Game::Mode::Pvp;
r32 Game::schedule_timer;
Game::Save Game::save;
Game::Level Game::level;
Game::Session Game::session;
b8 Game::cancel_event_eaten[] = {};
ScreenQuad Game::screen_quad;

template<typename Stream> b8 serialize_save(Stream* p, Game::Save* s)
{
	serialize_r64(p, s->timestamp);
	serialize_s32(p, s->collectibles.length);
	if (Stream::IsReading)
		s->collectibles.resize(s->collectibles.length);
	for (s32 i = 0; i < s->collectibles.length; i++)
	{
		serialize_s16(p, s->collectibles[i].zone);
		serialize_int(p, ID, s->collectibles[i].id, 0, MAX_ENTITIES - 1);
	}
	serialize_s32(p, s->locke_index);
	for (s32 i = 0; i < MAX_ZONES; i++)
		serialize_enum(p, ZoneState, s->zones[i]);
	serialize_enum(p, Game::Group, s->group);
	for (s32 i = 0; i < s32(Resource::count); i++)
		serialize_s32(p, s->resources[i]);
	serialize_s16(p, s->zone_last);
	serialize_s16(p, s->zone_current);
	serialize_s16(p, s->zone_overworld);
	serialize_bool(p, s->locke_spoken);
	return true;
}

Game::Save::Save()
{
	reset();
}

void Game::Save::reset()
{
	this->~Save();

	memset(this, 0, sizeof(*this));

	zone_last = AssetNull;
	zone_current = Asset::Level::Docks;
	zone_overworld = AssetNull;
	locke_index = -1;

	resources[s32(Resource::Energy)] = s16(ENERGY_INITIAL * 3.5f);
	zones[s32(Asset::Level::Docks)] = ZoneState::ParkourUnlocked;
}

Game::Session::Session()
{
	reset(SessionType::Story);
}

r32 Game::Session::effective_time_scale() const
{
	return time_scale * grapple_time_scale;
}

s32 Game::Session::local_player_count() const
{
	return BitUtility::popcount(u32(local_player_mask));
}

void Game::Session::reset(SessionType t)
{
	this->~Session();
	memset(this, 0, sizeof(*this));

	zone_under_attack = AssetNull;
	time_scale = grapple_time_scale = 1.0f;
	new (&config) Net::Master::ServerConfig();

	type = t;

#if SERVER
	local_player_mask = 0;
#else
	local_player_mask = 1;
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
		local_player_uuids[i] = mersenne::rand_u64();
#endif
}

b8 Game::Level::has_feature(FeatureLevel f) const
{
	return s32(feature_level) >= s32(f);
}

void Game::Level::multiplayer_level_schedule()
{
	multiplayer_level_scheduled = id;
	while (Game::session.config.levels.length > 1 && multiplayer_level_scheduled == id)
		multiplayer_level_scheduled = Overworld::zone_id_for_uuid(Game::session.config.levels[mersenne::rand() % Game::session.config.levels.length]);
}

AI::Team Game::Level::team_lookup_reverse(AI::Team t) const
{
	for (s32 i = 0; i < MAX_TEAMS; i++)
	{
		if (team_lookup[i] == t)
			return AI::Team(i);
	}
	return AI::TeamNone;
}

const StaticArray<DirectionalLight, MAX_DIRECTIONAL_LIGHTS>& Game::Level::directional_lights_get() const
{
	if (Overworld::modal())
		return Overworld::directional_lights;
	else
		return directional_lights;
}

const Vec3& Game::Level::ambient_color_get() const
{
	if (Overworld::modal())
		return Overworld::ambient_color;
	else
		return ambient_color;
}

r32 Game::Level::far_plane_get() const
{
	if (Overworld::modal())
		return Overworld::far_plane;
	else
		return skybox.far_plane;
}

r32 Game::Level::fog_start_get() const
{
	if (Overworld::modal())
		return Overworld::fog_start;
	else
		return Game::level.skybox.fog_start;
}

r32 Game::Level::fog_end_get() const
{
	if (Overworld::modal())
		return Overworld::far_plane;
	else
		return Game::level.skybox.fog_end;
}

Array<UpdateFunction> Game::updates;
Array<DrawFunction> Game::draws;
Array<CleanupFunction> Game::cleanups;

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#if !SERVER && !defined(__ORBIS__)
char discord_match_secret[MAX_SERVER_CONFIG_SECRET + 1];
r64 discord_presence_time;

void discord_ready()
{
}

void discord_join_game(const char* secret)
{
	strncpy(discord_match_secret, secret, MAX_SERVER_CONFIG_SECRET);
}

void discord_update_presence()
{
	discord_presence_time = Game::platform_time;

	DiscordRichPresence p;
	memset(&p, 0, sizeof(p));
	char details[256] = {};
	char party_id[32] = {};
	if (Game::level.mode == Game::Mode::Special)
	{
		if (Game::session.type == SessionType::Multiplayer)
			p.state = "In Lobby";
		else
			p.state = "In Menus";
	}
	else
	{
		if (Game::session.type == SessionType::Story)
			p.state = "Playing Story";
		else
		{
			p.state = "In Game";
			snprintf(details, 255, "%s | %s", Net::Master::ServerConfig::game_type_string_human(Game::session.config.game_type), Loader::level_name(Game::level.id));
			p.details = details;
			snprintf(party_id, 32, "%u", Game::session.config.id);
			p.partyId = party_id;
			p.joinSecret = Game::session.config.secret;
			p.partySize = PlayerHuman::list.count();
			p.partyMax = Game::session.config.max_players;
		}
	}
	Discord_UpdatePresence(&p);
}
#endif

void Game::init(LoopSync* sync)
{
	Net::Master::Ruleset::init();

#if !SERVER && !defined(__ORBIS__)
	if (auth_type == Net::Master::AuthType::Steam)
	{
		if (SteamAPI_RestartAppIfNecessary(STEAM_APP_ID))
			quit = true;
		else if (!SteamAPI_Init())
			vi_assert(false);
	}

	{
		DiscordEventHandlers handlers;
		memset(&handlers, 0, sizeof(handlers));
		handlers.joinGame = discord_join_game;
		handlers.ready = discord_ready;
		Discord_Initialize(DISCORD_APP_ID, &handlers, 1, auth_type == Net::Master::AuthType::Steam ? TOSTRING(STEAM_APP_ID) : nullptr);
	}
#endif

	// count scripts
	while (true)
	{
		if (Script::list[Script::count].name)
			Script::count++;
		else
			break;
	}

	AI::init();

	World::init();

	{
		cJSON* overworld_level = Loader::level(Asset::Level::overworld);
		Overworld::init(overworld_level);
		Loader::level_free(overworld_level);
	}

	Net::init();

#if !SERVER
	// replay files
	{
		const char* replay_dir = "rec/";
		DIR* dir = opendir(replay_dir);
		if (dir)
		{
			struct dirent* entry;
			while ((entry = readdir(dir)))
			{
				if (entry->d_type != DT_REG)
					continue; // not a file

				char filename[MAX_PATH_LENGTH + 1];
				snprintf(filename, MAX_PATH_LENGTH, "%s%s", replay_dir, entry->d_name);
				Net::Client::replay_file_add(filename);
			}
			closedir(dir);
		}
	}

	Audio::init();

	Loader::font_permanent(Asset::Font::lowpoly);
	Loader::font_permanent(Asset::Font::pt_sans);

	if (!Loader::soundbank_permanent(Asset::Soundbank::Init))
		vi_assert(false);
	if (!Loader::soundbank_permanent(Asset::Soundbank::SOUNDBANK))
		vi_assert(false);

	// strings
	{
		char string_file[256] = {};
		snprintf(string_file, 255, "assets/str/%s.json", language);

		// UI
		{
			cJSON* json = Json::load(string_file);
			cJSON* str = cJSON_GetObjectItem(json, "str");
			for (s32 i = 0; i < Asset::String::count; i++)
			{
				const char* name = AssetLookup::String::names[i];
				cJSON* value = cJSON_GetObjectItem(str, name);
				strings_set(i, value ? value->valuestring : nullptr);
			}
		}

		Input::init(); // loads localized strings for input bindings. plus other stuff

		// don't free the JSON objects; we'll read strings directly from them
	}

	Menu::init(sync->input);

	for (s32 i = 0; i < ParticleSystem::list.length; i++)
		ParticleSystem::list[i]->init(sync);

	UI::init(sync);

	Console::init();
#endif

	Drone::init();

	Menu::splash();
}

void Game::auth_failed()
{
	if (auth_type == Net::Master::AuthType::GameJolt)
		Scripts::Docks::gamejolt_prompt();
	else
		Menu::dialog(0, &Menu::dialog_no_action, _(strings::auth_failed_permanently));
}

void Game::update(InputState* input, const InputState* last_input)
{
#if !SERVER && !defined(__ORBIS__)
	Discord_UpdateConnection();
	Discord_RunCallbacks();

	if (user_key.id && discord_match_secret[0])
	{
		// we're logged in, and we're supposed to be joining a discord match
		unload_level();
		Net::Client::master_request_server(1, discord_match_secret);
		discord_match_secret[0] = '\0';
	}

	if (platform_time - discord_presence_time > 1.0)
		discord_update_presence();

	if (auth_type == Net::Master::AuthType::Steam)
		SteamAPI_RunCallbacks();
#endif

	UI::update();

	{
		r64 t = platform::time();
		r64 dt = vi_min(t - platform_time, 0.2);
		platform_time = t;

		real_time.total = r32(r64(real_time.total) + dt);
		real_time.delta = r32(dt);
		r64 dt2 = dt * r64(session.effective_time_scale());
		time.delta = r32(dt2);
	}

#if !RELEASE_BUILD
	View::debug_entries.length = 0;
#endif

	if (schedule_timer > 0.0f)
	{
		r32 old_timer = schedule_timer;
		schedule_timer = vi_max(0.0f, schedule_timer - real_time.delta);
#if SERVER
		if (schedule_timer < TRANSITION_TIME && old_timer >= TRANSITION_TIME)
			Net::Server::transition_level(); // let clients know that we're switching levels
#endif
		if (scheduled_load_level != AssetNull && schedule_timer < TRANSITION_TIME * 0.5f && old_timer >= TRANSITION_TIME * 0.5f)
		{
			load_level(scheduled_load_level, scheduled_mode, StoryModeTeam::Attack);
			if (scheduled_dialog != AssetNull)
			{
				Menu::dialog(0, &Menu::dialog_no_action, _(scheduled_dialog));
				scheduled_dialog = AssetNull;
			}
		}
	}

	b8 update_game;
#if SERVER
	update_game = Net::Server::mode() == Net::Server::Mode::Active;
#else
	update_game = (level.local || Net::Client::mode() == Net::Client::Mode::Connected)
		&& !Overworld::modal()
		&& (!should_pause() || PlayerHuman::list.iterator().item()->ui_mode() != PlayerHuman::UIMode::Pause);
#endif

	if (update_game)
		physics_timestep = (1.0f / 60.0f) * session.effective_time_scale();
	else
		physics_timestep = 0.0f;

	if (update_game)
	{
		time.total += time.delta;
		Team::match_time += time.delta;
	}

	Update u;
	u.input = input;
	u.last_input = last_input;
	u.time = time;
	u.real_time = real_time;

	if (update_game || Overworld::modal())
	{
		ParticleSystem::time = update_game ? time.total : real_time.total;
		for (s32 i = 0; i < ParticleSystem::list.length; i++)
			ParticleSystem::list[i]->update();
	}

	Net::update_start(u);

#if !SERVER
	// trigger attract mode
	if (Settings::expo && Net::Client::replay_mode() != Net::Client::ReplayMode::Replaying)
	{
		inactive_timer += u.time.delta;
		if (input->keys.any()
			|| input->cursor.length_squared() > 0.0f
			|| (PlayerControlHuman::list.count() > 0 && PlayerControlHuman::list.iterator().item()->cinematic_active()))
			inactive_timer = 0.0f;
		else
		{
			for (s32 i = 0; i < MAX_GAMEPADS; i++)
			{
				const Gamepad& gamepad = input->gamepads[i];
				if (gamepad.type != Gamepad::Type::None
					&& (gamepad.btns
						|| Input::dead_zone(gamepad.left_x) != 0.0f || Input::dead_zone(gamepad.left_y) != 0.0f
						|| Input::dead_zone(gamepad.right_x) != 0.0f || Input::dead_zone(gamepad.right_y) != 0.0f))
				{
					inactive_timer = 0.0f;
					break;
				}
			}
		}
		if (inactive_timer > 60.0f && Game::scheduled_load_level == AssetNull)
		{
			if (Net::Client::replay_file_count() > 0)
			{
				unload_level();
				save.reset();
				Net::Client::replay();
			}
			else if ((level.id != Asset::Level::Docks && level.id != Asset::Level::overworld)
				|| level.mode != Mode::Special)
			{
				if (Game::session.type == SessionType::Story)
					Menu::title();
				else
					Game::schedule_load_level(Asset::Level::overworld, Game::Mode::Special);
			}
			inactive_timer = 0.0f;
		}
	}

	// determine whether to display gamepad or keyboard bindings
	{
		b8 refresh = false;
		for (s32 i = 0; i < MAX_GAMEPADS; i++)
		{
			const Gamepad& gamepad = input->gamepads[i];
			if (i == 0)
			{
				if (ui_gamepad_types[0] == Gamepad::Type::None)
				{
					// check if we need to update the gamepad type
					if (gamepad.type != Gamepad::Type::None)
					{
						if (gamepad.btns)
						{
							ui_gamepad_types[0] = gamepad.type;
							refresh = true;
						}
						else
						{
							Vec2 left(gamepad.left_x, gamepad.left_y);
							Input::dead_zone(&left.x, &left.y);
							if (left.length_squared() > 0.0f)
							{
								ui_gamepad_types[0] = gamepad.type;
								refresh = true;
							}
							else
							{
								Vec2 right(gamepad.right_x, gamepad.right_y);
								Input::dead_zone(&right.x, &right.y);
								if (right.length_squared() > 0.0f)
								{
									ui_gamepad_types[0] = gamepad.type;
									refresh = true;
								}
							}
						}
					}
				}
				else
				{
					// check if we need to clear the gamepad flag
					if (gamepad.type == Gamepad::Type::None
						|| input->mouse_relative.length_squared() > 0.0f
						|| input->keys.any())
					{
						ui_gamepad_types[0] = Gamepad::Type::None;
						refresh = true;
					}
				}
			}
			else if (gamepad.type != ui_gamepad_types[i])
			{
				ui_gamepad_types[i] = gamepad.type;
				refresh = true;
				break;
			}
		}

		if (refresh)
			Menu::refresh_variables(*u.input);
	}

	Menu::update(u);
#endif

	AI::update(u);

	Team::update_all(u);

	if (update_game)
	{
		Ascensions::update(u);
		Asteroids::update(u);

		Physics::sync_dynamic();

		ShellCasing::update_all(u);

		for (auto i = Ragdoll::list.iterator(); !i.is_last(); i.next())
		{
			if (level.local)
				i.item()->update_server(u);
			i.item()->update_client(u);
		}
		for (auto i = Animator::list.iterator(); !i.is_last(); i.next())
		{
			if (!level.local && i.item()->has<Walker>() && (!i.item()->has<PlayerControlHuman>() || !i.item()->get<PlayerControlHuman>()->local()))
				i.item()->update_client_only(u); // walker animations are synced over the network
			else if (!i.item()->has<Parkour>()) // Parkour component updates the Animator on its own terms
				i.item()->update_server(u);
		}

		for (auto i = TramRunner::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);

		Physics::sync_static();

		ParticleEffect::update_all(u);

		PlayerManager::update_all(u);
		PlayerHuman::update_all(u);

		if (level.local)
		{
			SpawnPoint::update_server_all(u);
			if (session.type == SessionType::Story && level.mode == Mode::Pvp && Team::match_state != Team::MatchState::Done)
			{
				// spawn AI players
				for (s32 i = 0; i < level.ai_config.length; i++)
				{
					const AI::Config& config = level.ai_config[i];
					if (Team::match_time > config.spawn_time)
					{
						Entity* e = World::create<ContainerEntity>();
						PlayerManager* manager = e->add<PlayerManager>(&Team::list[s32(config.team)], Usernames::all[mersenne::rand_u32() % Usernames::count]);
						if (config.spawn_time == 0.0f)
							manager->spawn_timer = 0.01f; // spawn instantly

						PlayerAI* player = PlayerAI::list.add();
						new (player) PlayerAI(manager, config);

						Net::finalize(e);

						level.ai_config.remove(i);
						i--;
					}
				}
			}

			for (auto i = Turret::list.iterator(); !i.is_last(); i.next())
				i.item()->update_server(u);
			for (auto i = Walker::list.iterator(); !i.is_last(); i.next())
				i.item()->update_server(u);
			for (auto i = PlayerAI::list.iterator(); !i.is_last(); i.next())
				i.item()->update_server(u);
			for (auto i = PlayerControlAI::list.iterator(); !i.is_last(); i.next())
				i.item()->update_server(u);
			for (auto i = Bolt::list.iterator(); !i.is_last(); i.next())
				i.item()->simulate(u.time.delta);
			for (auto i = Flag::list.iterator(); !i.is_last(); i.next())
				i.item()->update_server(u);
		}
		else
		{
			for (auto i = Flag::list.iterator(); !i.is_last(); i.next())
				i.item()->update_client(u);
			for (auto i = Glass::list.iterator(); !i.is_last(); i.next())
				i.item()->update_client(u);
		}

#if !SERVER
		Bolt::update_client_all(u);
		Turret::update_client_all(u);
#endif

		for (auto i = Health::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
		Minion::update_all(u);
		Grenade::update_all(u);
		for (auto i = Tile::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
		for (auto i = AirWave::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
		for (auto i = UpgradeStation::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
		Drone::update_all(u);
		for (auto i = PlayerTrigger::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
		Battery::update_all(u);
		Rectifier::update_all(u);
		ForceField::update_all(u);
		for (auto i = EffectLight::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
		for (auto i = PlayerCommon::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
		for (auto i = PlayerControlHuman::list.iterator(); !i.is_last(); i.next())
		{
			if (!level.local && i.item()->local() && i.item()->has<Walker>())
				i.item()->get<Walker>()->update_server(u); // walkers are normally only updated on the server
			i.item()->update(u);
		}
		for (auto i = Parkour::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->get<PlayerControlHuman>()->local())
				i.item()->update_server(u);
			else if (level.local) // server needs to manually update the animator because it's normally updated by the Parkour component
				i.item()->get<Animator>()->update_server(u);
			i.item()->update_client(u);
		}

		Shield::update_all(u);

		for (auto i = PlayerControlHuman::list.iterator(); !i.is_last(); i.next())
			i.item()->update_late(u);

		for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
			i.item()->update_late(u);

		Water::update_all(u);

		for (s32 i = 0; i < updates.length; i++)
			(*updates[i])(u);

#if !SERVER
		GlassShard::update_all(u);
		if (level.rain > 0.0f)
			Rain::spawn(u, level.rain);
#endif
	}
	else
	{
		// don't update game
#if !SERVER
		if (Game::session.type == SessionType::Story && Game::level.local)
		{
			// we're paused; update player UI
			PlayerHuman::update_all(u);
		}
#endif
	}

	Console::update(u);

	Overworld::update(u);

	World::flush();

	Audio::param_global(AK::GAME_PARAMETERS::TIMESCALE, session.effective_time_scale());
	Audio::update_all(u);

#if !SERVER
	Menu::update_end(u);
#endif

	Net::update_end(u);

	u.input->cursor_visible = UI::cursor_active();
}

void Game::term()
{
	Net::term();
	Audio::term();
#if !SERVER && !defined(__ORBIS__)
	if (auth_type == Net::Master::AuthType::Steam)
		SteamAPI_Shutdown();
	Discord_Shutdown();
#endif
}

b8 Game::edge_trigger(r32 time, b8(*fn)(r32))
{
	return fn(time) != fn(time - real_time.delta);
}

b8 Game::edge_trigger(r32 time, r32 speed, b8(*fn)(r32, r32))
{
	return fn(time, speed) != fn(time - real_time.delta, speed);
}

// remove bots to make room for new human players if necessary
void Game::remove_bots_if_necessary(s32 players)
{
	s32 open_slots = vi_min(s32(session.config.max_players), session.config.fill_bots ? session.config.fill_bots + 1 : 0) - PlayerManager::list.count();
	s32 bots_to_remove = vi_min(PlayerAI::list.count(), players - open_slots);
	while (bots_to_remove > 0)
	{
		PlayerAI* ai_player = PlayerAI::list.iterator().item();
		PlayerManager* player = ai_player->manager.ref();
		Entity* instance = player->instance.ref();
		if (instance)
			World::remove(instance);
		World::remove(player->entity());
		PlayerAI::list.remove(ai_player->id());
		bots_to_remove--;
	}
}

void Game::add_local_player(s8 gamepad)
{
	vi_assert
	(
		level.local
		&& level.mode == Mode::Pvp
		&& session.type == SessionType::Multiplayer
		&& PlayerHuman::list.count() < session.config.max_players
		&& !PlayerHuman::for_gamepad(gamepad)
	);

	remove_bots_if_necessary(1);

	AI::Team team = Team::with_least_players()->team();

	char username[MAX_USERNAME + 1] = {};
	snprintf(username, MAX_USERNAME, _(strings::player), gamepad + 1);

	Entity* e = World::alloc<ContainerEntity>();
	PlayerManager* manager = e->create<PlayerManager>(&Team::list[s32(team)], username);

	e->create<PlayerHuman>(true, gamepad); // local = true

	World::awake(e);
	Net::finalize(e);
}

// return true if this entity's transform needs synced over the network
b8 Game::net_transform_filter(const Entity* t, Mode mode)
{
	return t->component_mask &
	(
		Drone::component_mask
		| Grenade::component_mask
		| Rectifier::component_mask
		| ForceField::component_mask
		| Bolt::component_mask
		| Flag::component_mask
		| Battery::component_mask
		| ForceField::component_mask
		| Rectifier::component_mask
		| Walker::component_mask
	);
}

#if SERVER

void Game::draw_opaque(const RenderParams&) { }
void Game::draw_alpha(const RenderParams&) { }
void Game::draw_hollow(const RenderParams&) { }
void Game::draw_override(const RenderParams&) { }
void Game::draw_particles(const RenderParams&) { }
void Game::draw_additive(const RenderParams&) { }
void Game::draw_alpha_late(const RenderParams&) { }

#else

// client

b8 view_filter_culled(const RenderParams& params, const View* v)
{
	return v->shader == Asset::Shader::culled && (v->mask & params.camera->mask);
}

void Game::draw_opaque(const RenderParams& render_params)
{
	b8 default_pass = render_params.technique == RenderTechnique::Default
		&& !(render_params.flags & RenderFlagEdges);

	if (render_params.flags & RenderFlagPolygonOffset)
	{
		render_params.sync->write(RenderOp::PolygonOffset);
		render_params.sync->write(Vec2(1.0f));
	}

	View::draw_opaque(render_params);

	if (default_pass)
	{
		SkyPattern::draw_opaque(render_params);

		// render back faces of culled geometry
		if (render_params.camera->cull_range > 0.0f && !(render_params.flags & RenderFlagPolygonOffset))
		{
			render_params.sync->write<RenderOp>(RenderOp::CullMode);
			render_params.sync->write<RenderCullMode>(RenderCullMode::Front);

			{
				RenderParams p = render_params;
				p.flags |= RenderFlagBackFace;
				View::draw_filtered(p, &view_filter_culled);
			}
			render_params.sync->write<RenderOp>(RenderOp::CullMode);
			render_params.sync->write<RenderCullMode>(RenderCullMode::Back);
		}
	}

	Overworld::draw_opaque(render_params);

	Water::draw_opaque(render_params);

	SkinnedModel::draw_opaque(render_params);

	EffectLight::draw_opaque(render_params);

	ShellCasing::draw_all(render_params);

	Rope::draw_all(render_params);

	if (render_params.flags & RenderFlagPolygonOffset)
	{
		render_params.sync->write(RenderOp::PolygonOffset);
		render_params.sync->write(Vec2::zero);
	}
}

void Game::draw_alpha(const RenderParams& render_params)
{
	GlassShard::draw_all(render_params);

	if (render_params.camera->flag(CameraFlagFog))
	{
		Skybox::draw_alpha(render_params);
		SkyDecals::draw_alpha(render_params);
	}

	Clouds::draw_alpha(render_params);

#if DEBUG_WALK_AI_PATH
	{
		UIText text;
		text.color = UI::color_accent();
		for (auto i = Minion::list.iterator(); !i.is_last(); i.next())
		{
			Minion* minion = i.item();
			for (s32 j = minion->path_index; j < minion->path.length; j++)
			{
				Vec2 p;
				if (UI::project(render_params, minion->path[j], &p))
				{
					text.text(0, "%d", j);
					text.draw(render_params, p);
				}
			}
		}
	}
#endif

#if DEBUG_DRONE_AI_PATH
	{
		UIText text;
		for (auto i = PlayerControlAI::list.iterator(); !i.is_last(); i.next())
		{
			PlayerControlAI* ai = i.item();
			text.color = Team::ui_color(render_params.camera->team, i.item()->get<AIAgent>()->team);
			for (s32 j = 0; j < ai->path.length; j++)
			{
				Vec2 p;
				if (UI::project(render_params, ai->path[j].pos, &p))
				{
					text.text(0, "%d", j);
					text.draw(render_params, p);
				}
			}
		}
	}
#endif

#if DEBUG_PHYSICS
	{
		RenderSync* sync = render_params.sync;

		sync->write(RenderOp::FillMode);
		sync->write(RenderFillMode::Line);

		Loader::shader_permanent(Asset::Shader::flat);

		sync->write(RenderOp::Shader);
		sync->write(Asset::Shader::flat);
		sync->write(render_params.technique);

		for (auto i = RigidBody::list.iterator(); !i.is_last(); i.next())
		{
			RigidBody* body = i.item();
			if (!body->btBody)
				continue;
			btTransform transform = body->btBody->getWorldTransform();

			Vec3 radius;
			Vec4 color;
			AssetID mesh_id;
			switch (body->type)
			{
				case RigidBody::Type::Box:
				{
					mesh_id = Asset::Mesh::cube;
					radius = body->size;
					color = Vec4(1, 0, 0, 1);
					break;
				}
				case RigidBody::Type::Sphere:
				{
					mesh_id = Asset::Mesh::sphere;
					radius = body->size;
					color = Vec4(1, 0, 0, 1);
					break;
				}
				case RigidBody::Type::CapsuleX:
				{
					// capsules: size.x = radius, size.y = height
					mesh_id = Asset::Mesh::cube;
					radius = Vec3((body->size.y + body->size.x * 2.0f) * 0.5f, body->size.x, body->size.x);
					color = Vec4(0, 1, 0, 1);
					break;
				}
				case RigidBody::Type::CapsuleY:
				{
					mesh_id = Asset::Mesh::cube;
					radius = Vec3(body->size.x, (body->size.y + body->size.x * 2.0f) * 0.5f, body->size.x);
					color = Vec4(0, 1, 0, 1);
					break;
				}
				case RigidBody::Type::CapsuleZ:
				{
					mesh_id = Asset::Mesh::cube;
					radius = Vec3(body->size.x, body->size.x, (body->size.y + body->size.x * 2.0f) * 0.5f);
					color = Vec4(0, 1, 0, 1);
					break;
				}
				default:
					continue;
			}

			if (!render_params.camera->visible_sphere(transform.getOrigin(), vi_max(radius.x, vi_max(radius.y, radius.z))))
				continue;

			Loader::mesh_permanent(mesh_id);

			Mat4 m;
			m.make_transform(transform.getOrigin(), radius, transform.getRotation());
			Mat4 mvp = m * render_params.view_projection;

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::mvp);
			sync->write(RenderDataType::Mat4);
			sync->write<s32>(1);
			sync->write<Mat4>(mvp);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::diffuse_color);
			sync->write(RenderDataType::Vec4);
			sync->write<s32>(1);
			sync->write<Vec4>(color);

			sync->write(RenderOp::Mesh);
			sync->write(RenderPrimitiveMode::Triangles);
			sync->write(mesh_id);
		}

		for (auto i = Minion::list.iterator(); !i.is_last(); i.next())
		{
			// render a sphere for the headshot collision volume
			Vec3 head_pos = i.item()->head_pos();
			if (!render_params.camera->visible_sphere(head_pos, MINION_HEAD_RADIUS))
				continue;

			Loader::mesh_permanent(Asset::Mesh::sphere);

			Mat4 m;
			m.make_transform(head_pos, Vec3(MINION_HEAD_RADIUS), Quat::identity);
			Mat4 mvp = m * render_params.view_projection;

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::mvp);
			sync->write(RenderDataType::Mat4);
			sync->write<s32>(1);
			sync->write<Mat4>(mvp);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::diffuse_color);
			sync->write(RenderDataType::Vec4);
			sync->write<s32>(1);
			sync->write<Vec4>(Vec4(1, 1, 1, 1));

			sync->write(RenderOp::Mesh);
			sync->write(RenderPrimitiveMode::Triangles);
			sync->write(Asset::Mesh::sphere);
		}

		sync->write(RenderOp::FillMode);
		sync->write(RenderFillMode::Fill);
	}
#endif

	Rectifier::draw_alpha_all(render_params);

	SkinnedModel::draw_alpha(render_params);

	View::draw_alpha(render_params);

	EffectLight::draw_alpha(render_params);

	Tile::draw_alpha(render_params);
	AirWave::draw_alpha(render_params);

	ParticleEffect::draw_alpha(render_params);

	PlayerHuman* player_human = PlayerHuman::for_camera(render_params.camera);

	if (player_human)
		player_human->draw_ui_early(render_params);

	Ascensions::draw_ui(render_params);

	for (auto i = PlayerControlHuman::list.iterator(); !i.is_last(); i.next())
		i.item()->draw_ui(render_params);

	for (s32 i = 0; i < draws.length; i++)
		(*draws[i])(render_params);

	Overworld::draw_ui(render_params);

	if (player_human)
		player_human->draw_ui(render_params);

	Team::draw_ui(render_params);

	Menu::draw_ui(render_params);

	if (schedule_timer > 0.0f && schedule_timer < TRANSITION_TIME)
		Menu::draw_letterbox(render_params, schedule_timer, TRANSITION_TIME);

	if (render_params.camera->gamepad == 0 && Game::level.id != Asset::Level::splash)
	{
		Console::draw_ui(render_params);

#if RELEASE_BUILD
		// build id
		{
			UIText text;
			text.font = Asset::Font::pt_sans;
			text.text(0, "DECEIVER %s", BUILD_ID);
			text.draw(render_params, Vec2::zero);
		}
#endif
	}
}

void Game::draw_hollow(const RenderParams& render_params)
{
	Overworld::draw_hollow(render_params);

	SkyPattern::draw_hollow(render_params);

	for (auto i = Water::list.iterator(); !i.is_last(); i.next())
		i.item()->draw_hollow(render_params);

#if DEBUG_WALK_NAV_MESH
	AI::debug_draw_nav_mesh(render_params);
#endif

	if (Settings::antialiasing)
		AI::draw_hollow(render_params);
}

void Game::draw_override(const RenderParams& render_params)
{
	Overworld::draw_override(render_params);
}

void Game::draw_particles(const RenderParams& render_params)
{
	Rope::draw_all(render_params);
	Asteroids::draw_alpha(render_params);

	render_params.sync->write(RenderOp::CullMode);
	render_params.sync->write(RenderCullMode::None);
	for (s32 i = 0; i < ParticleSystem::list.length; i++)
		ParticleSystem::list[i]->draw(render_params);
	render_params.sync->write(RenderOp::CullMode);
	render_params.sync->write(RenderCullMode::Back);
}

void Game::draw_additive(const RenderParams& render_params)
{
	View::draw_additive(render_params);
	SkinnedModel::draw_additive(render_params);
}

void Game::draw_alpha_late(const RenderParams& render_params)
{
	Water::draw_alpha_late(render_params);
	for (auto i = PlayerControlHuman::list.iterator(); !i.is_last(); i.next())
		i.item()->draw_alpha_late(render_params);

	PlayerHuman* player_human = PlayerHuman::for_camera(render_params.camera);
	if (player_human)
		player_human->draw_alpha_late(render_params);
}

#endif

void game_end_cheat(b8 win)
{
	if (Game::level.mode == Game::Mode::Pvp && Game::session.type == SessionType::Story)
	{
		PlayerManager* player = PlayerHuman::list.iterator().item()->get<PlayerManager>();
		if (!win)
		{
			PlayerManager* enemy = nullptr;
			for (auto i = PlayerAI::list.iterator(); !i.is_last(); i.next())
			{
				if (i.item()->manager.ref()->team.ref() != player->team.ref())
				{
					enemy = i.item()->manager.ref();
					break;
				}
			}
			if (enemy)
				player = enemy;
			else
				return;
		}

		if (Game::session.config.game_type == GameType::Deathmatch)
			player->kills = Game::session.config.kill_limit;
		else if (Game::session.config.game_type == GameType::Domination)
			player->team.ref()->energy_collected = Game::session.config.energy_collected_limit;
		else if (Game::session.config.game_type == GameType::CaptureTheFlag)
			player->team.ref()->flags_captured = Game::session.config.flag_limit;
		else if (Game::session.config.game_type == GameType::Assault)
		{
			if (player->team.ref()->team() == 0) // defending
				Team::match_time = Game::session.config.time_limit();
			else // attacking
			{
				for (auto i = Turret::list.iterator(); !i.is_last(); i.next())
					i.item()->killed(nullptr);
				for (auto i = CoreModule::list.iterator(); !i.is_last(); i.next())
					i.item()->destroy();
			}
		}
	}
}

void Game::execute(const char* cmd)
{
	if (strcmp(cmd, "netstat") == 0)
		Net::show_stats = !Net::show_stats;
#if !SERVER
	else if (strstr(cmd, "replay") == cmd)
	{
		const char* delimiter = strchr(cmd, ' ');
		const char* filename;
		if (delimiter)
			filename = delimiter + 1;
		else
			filename = nullptr;
		if (!filename || strlen(filename) <= MAX_PATH_LENGTH)
		{
			unload_level();
			save.reset();
			Net::Client::replay(filename);
		}
	}
#endif
	else if (!Settings::god_mode)
	{
		if (strcmp(cmd, "0451") == 0)
		{
			Settings::god_mode = true;
			Loader::settings_save();
			PlayerHuman::log_add(_(strings::god_mode_enabled));
		}
	}
#if !SERVER
	else if (strcmp(cmd, "noclip") == 0)
	{
		level.noclip = !level.noclip;
		if (Game::level.local && level.noclip)
		{
			for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
			{
				Entity* entity = i.item()->get<PlayerManager>()->instance.ref();
				if (entity)
					World::remove(entity);
			}
		}
	}
	else if (!level.local && Net::Client::mode() == Net::Client::Mode::Connected)
	{
		Net::Client::execute(cmd);
		return;
	}
	else if (strstr(cmd, "ldp ") == cmd)
	{
		// parkour mode
		const char* delimiter = strchr(cmd, ' ');
		if (delimiter)
		{
			const char* level_name = delimiter + 1;
			AssetID level = Loader::find_level(level_name);
			if (level != AssetNull)
			{
				save.reset();
				save.zone_current = level;
				schedule_load_level(level, Mode::Parkour);
			}
		}
	}
	else if (strstr(cmd, "resources ") == cmd)
	{
		const char* delimiter = strchr(cmd, ' ');
		const char* number_string = delimiter + 1;
		char* end;
		s32 value = s32(std::strtol(number_string, &end, 10));
		if (*end == '\0')
		{
			for (s32 i = 0; i < s32(Resource::ConsumableCount); i++)
				Overworld::resource_change(Resource(i), s16(value));
		}
	}
	else if (strcmp(cmd, "abilities") == 0)
	{
		Game::save.resources[s32(Resource::DoubleJump)] = 1;
		Game::save.resources[s32(Resource::ExtendedWallRun)] = 1;
		Game::save.resources[s32(Resource::Grapple)] = 1;
	}
#endif
#if !RELEASE_BUILD
	else if (strstr(cmd, "lds ") == cmd)
	{
		// allocate a story-mode server
		const char* delimiter = strchr(cmd, ' ');
		const char* level_name = delimiter + 1;
		AssetID level = Loader::find_level(level_name);
		if (level != AssetNull)
		{
			unload_level();
			save.reset();
			Net::Client::master_request_server(0, nullptr, level); // 0 = story mode
		}
	}
	else if (strstr(cmd, "ld ") == cmd)
	{
		// pvp mode
		const char* delimiter = strchr(cmd, ' ');
		const char* level_name = delimiter + 1;
		AssetID level = Loader::find_level(level_name);
		if (level != AssetNull)
		{
			save.reset();
			save.zone_current = level;
			schedule_load_level(level, Mode::Pvp);
		}
	}
	else if (strcmp(cmd, "killai") == 0)
	{
		for (auto i = PlayerControlAI::list.iterator(); !i.is_last(); i.next())
			i.item()->get<Health>()->kill(nullptr);
	}
	else if (strcmp(cmd, "spawn") == 0)
	{
		for (s32 i = 0; i < level.ai_config.length; i++)
			level.ai_config[i].spawn_time = 0.0f;
	}
	else if (strcmp(cmd, "win") == 0)
		game_end_cheat(true);
	else if (strcmp(cmd, "lose") == 0)
		game_end_cheat(false);
	else if (strcmp(cmd, "die") == 0)
	{
		for (auto i = PlayerControlHuman::list.iterator(); !i.is_last(); i.next())
			i.item()->get<Health>()->kill(nullptr);
	}
#if !SERVER
	else if (strcmp(cmd, "ascension") == 0)
	{
		Net::Client::master_request_ascension();
	}
#endif
#endif
	else if (strstr(cmd, "timescale ") == cmd)
	{
		const char* delimiter = strchr(cmd, ' ');
		const char* number_string = delimiter + 1;
		char* end;
		r32 value = r32(std::strtod(number_string, &end));
		if (*end == '\0')
		{
			session.time_scale = value;
#if SERVER
			Net::Server::sync_time();
#endif
		}
	}
	else if (strstr(cmd, "energy ") == cmd)
	{
		const char* delimiter = strchr(cmd, ' ');
		const char* number_string = delimiter + 1;
		char* end;
		s32 value = s32(std::strtol(number_string, &end, 10));
		if (*end == '\0')
		{
			if (level.mode == Mode::Parkour)
				Overworld::resource_change(Resource::Energy, value);
			else if (PlayerManager::list.count() > 0)
			{
				for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
					i.item()->energy += value;
			}
		}
	}
#if !SERVER
	else
		Overworld::execute(cmd);
#endif
}

void Game::schedule_load_level(AssetID level_id, Mode m, r32 delay)
{
	vi_debug("Scheduling level load: %d", s32(level_id));
	scheduled_load_level = level_id;
	scheduled_mode = m;
	schedule_timer = TRANSITION_TIME + delay;
}

void Game::unload_level()
{
	vi_debug("Unloading level %d", s32(level.id));
	Net::reset();

#if SERVER
	Net::Server::level_unloading();
#endif

	level.local = true;

	GlassShard::clear();
	Overworld::clear();
	Ascensions::clear();
	Asteroids::clear();
	Tile::clear();
	AirWave::clear();
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
		Audio::listener_disable(i);

	World::clear(); // deletes all entities

	// PlayerAI is not part of the entity system
	PlayerAI::list.clear();

	PlayerHuman::clear(); // clear some random player-related stuff

	Particles::clear();
	Rain::audio_clear();
	ShellCasing::clear();
	ParticleEffect::clear();

	Audio::clear();

	Loader::transients_free();
	updates.length = 0;
	draws.length = 0;
	for (s32 i = 0; i < cleanups.length; i++)
		(*cleanups[i])();
	cleanups.length = 0;

	level.~Level();

	new (&level) Level();

	level.skybox.far_plane = 1.0f;
	level.skybox.color = Vec3::zero;
	level.ambient_color = Vec3::zero;
	level.skybox.shader = AssetNull;
	level.skybox.texture = AssetNull;
	level.skybox.mesh = AssetNull;

	Menu::clear();

	Camera::list.clear();

	time.total = 0;

	level.id = AssetNull;

#if SERVER
	Net::Server::level_unloaded();
#endif
}

Entity* EntityFinder::find(const char* name) const
{
	for (s32 j = 0; j < map.length; j++)
	{
		if (strcmp(map[j].name, name) == 0)
			return map[j].entity.ref();
	}
	return nullptr;
}

void EntityFinder::add(const char* name, Entity* entity)
{
	NameEntry* entry = map.add();
	strncpy(entry->name, name, 255);
	entry->entity = entity;
}

template<typename T>
struct LevelLink
{
	Ref<T>* ref;
	const char* target_name;
};

struct RopeEntry
{
	Quat rot;
	Vec3 pos;
	r32 max_distance;
	r32 slack;
	s8 flags;
	b8 attach_end;
};

AI::Team team_lookup(const AI::Team* table, s32 i)
{
	return table[vi_max(0, vi_min(MAX_TEAMS - 1, i))];
}

cJSON* json_entity_by_name(cJSON* json, const char* name)
{
	cJSON* i = json->child;
	while (i)
	{
		if (strcmp(cJSON_GetObjectItem(i, "name")->valuestring, name) == 0)
			return i;
		i = i->next;
	}
	return nullptr;
}

void ingress_points_get(cJSON* json, cJSON* element, MinionTarget* target)
{
	cJSON* links = cJSON_GetObjectItem(element, "links");
	cJSON* link = links->child;
	while (link)
	{
		cJSON* linked_entity = json_entity_by_name(json, link->valuestring);
		vi_assert(linked_entity);
		if (cJSON_GetObjectItem(linked_entity, "IngressPoint"))
			target->ingress_points.add(Json::get_vec3(linked_entity, "pos"));
		link = link->next;
	}
}

void Game::load_level(AssetID l, Mode m, StoryModeTeam story_mode_team)
{
	vi_debug("Loading level %d", s32(l));

	AssetID last_level = level.id;
	Mode last_mode = level.mode;
	unload_level();

	{
		// start abilities may not show up in the default upgrades
		// and we don't want to save them in there in the database (don't want to override user preferences)
		// so apply them on game load
		for (s32 i = 0; i < session.config.ruleset.start_abilities.length; i++)
			session.config.ruleset.upgrades_default |= (1 << s32(session.config.ruleset.start_abilities[i]));
		session.config.ruleset.upgrades_allow &= ~session.config.ruleset.upgrades_default;

		// if we're playing an actual preset, then overwrite the ruleset
		if (session.config.preset != Net::Master::Ruleset::Preset::Custom)
		{
			session.config.ruleset = Net::Master::Ruleset::presets[s32(session.config.preset)];

			// Assault always has the same time limit unless it's Custom preset
			if (session.config.game_type == GameType::Assault)
				session.config.time_limit_minutes[s32(GameType::Assault)] = DEFAULT_ASSAULT_TIME_LIMIT_MINUTES;
		}
	}

	if (m == Mode::Parkour)
	{
		if (l != last_level && last_mode == Mode::Parkour)
		{
			save.zone_last = last_level;
			save.locke_spoken = false;
		}

		save.zone_current = l;
	}

#if SERVER
	Net::Server::level_loading();
#endif

	time.total = 0.0f;

	scheduled_load_level = AssetNull;

	Physics::btWorld->setGravity(btVector3(0, -13.0f, 0));

	Array<Ref<Transform>> transforms;

	Array<RopeEntry> ropes;

	Array<LevelLink<SpawnPoint>> spawn_links;
	Array<LevelLink<Entity>> entity_links;

	cJSON* json = Loader::level(l);

	level.mode = m;
	level.id = l;
	level.local = true;
	level.story_mode_team = story_mode_team;

	if (session.type == SessionType::Multiplayer)
		level.multiplayer_level_schedule();

	// count AI players
	s32 ai_player_count = 0;
	{
		cJSON* element = json->child;
		while (element)
		{
			if (cJSON_HasObjectItem(element, "AIPlayer"))
				ai_player_count++;
			element = element->next;
		}
	}

	struct TramTrackEntry
	{
		TramTrack* track;
		const char* tram_name;
	};
	StaticArray<TramTrackEntry, 3> tram_track_entries;

	{
		// tram tracks
		cJSON* element = json->child;
		while (element)
		{
			if (cJSON_HasObjectItem(element, "TramTrack"))
			{
				cJSON* links = cJSON_GetObjectItem(element, "links");
				const char* tram_name = links->child->valuestring;
				TramTrackEntry* entry = nullptr;
				for (s32 i = 0; i < tram_track_entries.length; i++)
				{
					if (strcmp(tram_track_entries[i].tram_name, tram_name) == 0)
					{
						entry = &tram_track_entries[i];
						break;
					}
				}
				if (!entry)
				{
					entry = tram_track_entries.add();
					entry->tram_name = tram_name;
					entry->track = level.tram_tracks.add();
				}
				TramTrack::Point* point = entry->track->points.add();
				point->pos = Json::get_vec3(element, "pos");
				if (entry->track->points.length > 1)
				{
					const TramTrack::Point& last_point = entry->track->points[entry->track->points.length - 2];
					point->offset = last_point.offset + (point->pos - last_point.pos).length();
				}
			}
			element = element->next;
		}
	}

	// default team index for minions and other items that spawn in the level
	s32 default_team_index = story_mode_team == StoryModeTeam::Defend ? 0 : 1;

	ID collectible_id = 0;

	cJSON* element = json->child;
	while (element)
	{
		Entity* entity = nullptr;

		Vec3 absolute_pos;
		Quat absolute_rot;
		s32 parent = cJSON_GetObjectItem(element, "parent")->valueint;

		{
			Vec3 pos = Json::get_vec3(element, "pos");
			Quat rot = Json::get_quat(element, "rot");
			absolute_pos = pos;
			absolute_rot = rot;

			if (parent != -1)
				transforms[parent].ref()->to_world(&absolute_pos, &absolute_rot);
		}

		if (cJSON_HasObjectItem(element, "World"))
		{
			// World is guaranteed to be the first element in the entity list

			level.feature_level = FeatureLevel(Json::get_s32(element, "feature_level", s32(FeatureLevel::All)));

			level.rain = Json::get_r32(element, "rain");

			if (cJSON_HasObjectItem(element, "ambience"))
				strncpy(level.ambience1, Json::get_string(element, "ambience"), MAX_AUDIO_EVENT_NAME);
			if (cJSON_HasObjectItem(element, "ambience2"))
				strncpy(level.ambience2, Json::get_string(element, "ambience2"), MAX_AUDIO_EVENT_NAME);

			// fill team lookup table
			{
				s32 offset;
				if (level.mode == Mode::Pvp)
				{
					if (session.type == SessionType::Story)
					{
						if (story_mode_team == StoryModeTeam::Defend)
							offset = 0; // put local player on team 0 (defenders)
						else
							offset = 1; // put local player on team 1 (attackers)
					}
					else
					{
						// shuffle teams and make sure they're packed in the array starting at 0
						b8 lock_teams = Json::get_s32(element, "lock_teams");
						offset = lock_teams ? 0 : mersenne::rand() % session.config.team_count;
					}
				}
				else
					offset = 1; // local player is always on team 1 (attackers) in Parkour or Special mode
				for (s32 i = 0; i < MAX_TEAMS; i++)
					level.team_lookup[i] = AI::Team((offset + i) % session.config.team_count);
			}

			level.skybox.far_plane = Json::get_r32(element, "far_plane", 100.0f);
			level.skybox.fog_start = Json::get_r32(element, "fog_start", 10.0f);
			level.skybox.fog_end = Json::get_r32(element, "fog_end", level.skybox.far_plane * 0.75f);
			level.skybox.texture = Loader::find(Json::get_string(element, "skybox_texture"), AssetLookup::Texture::names);
			level.skybox.shader = Asset::Shader::skybox;
			level.skybox.mesh = Asset::Mesh::skybox;
			level.skybox.color = Json::get_vec3(element, "skybox_color");
			level.ambient_color = Json::get_vec3(element, "ambient_color");

			level.min_y = Json::get_r32(element, "min_y", -20.0f);
			level.rotation = Json::get_r32(element, "rotation");

			// initialize teams
			if (m != Mode::Special || level.id == Asset::Level::Docks)
			{
				for (s32 i = 0; i < session.config.team_count; i++)
				{
					Entity* e = World::alloc<ContainerEntity>(); // team entities get awoken and finalized at the end of load_level()
					e->create<Team>();
				}

				for (s32 i = 0; i < MAX_GAMEPADS; i++)
				{
					if (session.local_player_mask & (1 << i))
					{
						AI::Team team = team_lookup(level.team_lookup, i % session.config.team_count);

						char username[MAX_USERNAME + 1] = {};
						snprintf(username, MAX_USERNAME, _(strings::player), i + 1);

						Entity* e = World::alloc<ContainerEntity>();
						PlayerManager* manager = e->create<PlayerManager>(&Team::list[s32(team)], username);

						e->create<PlayerHuman>(true, i); // local = true
						// container entity will be finalized later
					}
				}
			}
		}
		else if (Json::get_s32(element, "min_players") > PlayerManager::list.count() + ai_player_count
			|| Json::get_s32(element, "max_players", MAX_PLAYERS) < PlayerManager::list.count() + ai_player_count
			|| Json::get_s32(element, "min_teams") > Team::list.count()
			|| Json::get_s32(element, "max_teams", MAX_TEAMS) < Team::list.count())
		{
			// not enough players or teams, or too many
			// don't spawn the entity
		}
		else if (cJSON_HasObjectItem(element, "StaticGeom"))
		{
			b8 alpha = b8(Json::get_s32(element, "alpha"));
			b8 additive = b8(Json::get_s32(element, "additive"));
			b8 no_parkour = cJSON_HasObjectItem(element, "noparkour");
			b8 invisible = cJSON_HasObjectItem(element, "invisible");
			AssetID texture = Loader::find(Json::get_string(element, "texture"), AssetLookup::Texture::names);
			s16 extra_collision = (cJSON_HasObjectItem(element, "electric") ? CollisionElectric : 0)
				| (!cJSON_HasObjectItem(element, "nonav") ? CollisionAudio : 0);

			cJSON* meshes = cJSON_GetObjectItem(element, "meshes");
			cJSON* json_mesh = meshes->child;

			while (json_mesh)
			{
				const char* mesh_ref = json_mesh->valuestring;

				AssetID mesh_id = Loader::find_mesh(mesh_ref);

				if (mesh_id != AssetNull)
				{
					Entity* m;
					const Mesh* mesh = Loader::mesh(mesh_id);
					if (mesh->color.w < 0.5f && !(alpha || additive))
					{
						// inaccessible
						if (no_parkour) // no parkour material
							m = World::alloc<StaticGeom>(mesh_id, absolute_pos, absolute_rot, CollisionInaccessible | extra_collision);
						else
							m = World::alloc<StaticGeom>(mesh_id, absolute_pos, absolute_rot, CollisionParkour | CollisionInaccessible | extra_collision);
						m->get<View>()->color.w = MATERIAL_INACCESSIBLE;
					}
					else
					{
						// accessible
						if (no_parkour) // no parkour material
							m = World::alloc<StaticGeom>(mesh_id, absolute_pos, absolute_rot, extra_collision);
						else
							m = World::alloc<StaticGeom>(mesh_id, absolute_pos, absolute_rot, CollisionParkour | extra_collision);
					}

					m->get<View>()->texture = texture;
					if (alpha || additive)
						m->get<View>()->shader = texture == AssetNull ? Asset::Shader::flat : Asset::Shader::flat_texture;
					if (alpha)
						m->get<View>()->alpha();
					if (additive)
						m->get<View>()->additive();
					if (invisible)
						m->get<View>()->mask = 0;

					if (entity)
					{
						World::awake(m);
						m->get<Transform>()->reparent(entity->get<Transform>());
					}
					else
						entity = m;
				}

				json_mesh = json_mesh->next;
			}
		}
		else if (cJSON_HasObjectItem(element, "Rope"))
		{
			RopeEntry* rope = ropes.add();
			rope->pos = absolute_pos;
			rope->rot = absolute_rot;
			rope->slack = Json::get_r32(element, "slack");
			rope->max_distance = Json::get_r32(element, "max_distance", 20.0f);
			rope->attach_end = b8(Json::get_s32(element, "attach_end"));
			if (session.type == SessionType::Story)
				rope->flags = Rope::FlagClimbable;
		}
		else if (cJSON_HasObjectItem(element, "Turret"))
		{
			if (session.config.game_type == GameType::Assault || level.mode == Mode::Parkour)
			{
				entity = World::alloc<TurretEntity>(level.mode == Mode::Parkour ? AI::TeamNone : AI::Team(0)); // in parkour mode, turrets attack everyone
				entity->get<Turret>()->order = Json::get_s32(element, "order", entity->get<Turret>()->id());
				entity->get<Health>()->hp = Json::get_s32(element, "hp", TURRET_HEALTH);
				ingress_points_get(json, element, entity->get<MinionTarget>());
			}
		}
		else if (cJSON_HasObjectItem(element, "PathZone"))
		{
			AI::PathZone* path_zone = level.path_zones.add();
			path_zone->pos = absolute_pos;
			path_zone->radius = Json::get_vec3(element, "scale");
			cJSON* links = cJSON_GetObjectItem(element, "links");
			cJSON* link = links->child;
			while (link)
			{
				cJSON* linked_entity = json_entity_by_name(json, link->valuestring);
				vi_assert(linked_entity);
				if (cJSON_GetObjectItem(linked_entity, "ChokePoint"))
					path_zone->choke_point = Json::get_vec3(linked_entity, "pos");
				else // it's a target for this path zone
				{
					LevelLink<Entity>* target_link = entity_links.add();
					target_link->ref = path_zone->targets.add();
					target_link->target_name = cJSON_GetObjectItem(linked_entity, "name")->valuestring;
				}
				link = link->next;
			}
		}
		else if (cJSON_HasObjectItem(element, "Minion"))
		{
			if (level.mode == Mode::Parkour)
			{
				// starts out owned by player if the zone is friendly
				AI::Team team = team_lookup(level.team_lookup, Json::get_s32(element, "team", default_team_index));
				entity = World::alloc<MinionEntity>(absolute_pos, absolute_rot, team);
			}
		}
		else if (cJSON_HasObjectItem(element, "SpawnPoint"))
		{
			AI::Team team = AI::Team(Json::get_s32(element, "team"));
			b8 is_team_spawn = cJSON_HasObjectItem(element, "team");
			if ((is_team_spawn && Team::list.count() > s32(team))
				|| (!is_team_spawn && session.config.ruleset.enable_batteries))
				entity = World::alloc<SpawnPointEntity>(team, Json::get_s32(element, "visible", 1));
			else
			{
				entity = World::alloc<StaticGeom>(Asset::Mesh::spawn_collision, absolute_pos, absolute_rot, CollisionParkour, ~CollisionParkour & ~CollisionInaccessible & ~CollisionElectric);
				entity->get<View>()->mesh = Asset::Mesh::spawn_dressing;
			}
		}
		else if (cJSON_HasObjectItem(element, "FlagBase"))
		{
			if (session.config.game_type == GameType::CaptureTheFlag)
			{
				AI::Team team = AI::Team(Json::get_s32(element, "team"));
				if (Team::list.count() > s32(team))
				{
					entity = World::create<Prop>(Asset::Mesh::flag_base);
					entity->get<View>()->team = s8(team);
					entity->get<View>()->shader = Asset::Shader::culled;
					Team::list[s32(team)].flag_base = entity->get<Transform>();
				}
			}
		}
		else if (cJSON_HasObjectItem(element, "CoreModule"))
		{
			if (session.config.game_type == GameType::Assault)
			{
				AI::Team team = AI::Team(Json::get_s32(element, "team"));
				absolute_pos += absolute_rot * Vec3(0, DRONE_RADIUS * 0.5f, 0);
				entity = World::alloc<CoreModuleEntity>(team, nullptr, absolute_pos, absolute_rot);
				ingress_points_get(json, element, entity->get<MinionTarget>());
			}
		}
		else if (cJSON_HasObjectItem(element, "ForceField"))
		{
			if (session.config.game_type == GameType::Assault && level.mode == Mode::Pvp)
			{
				AI::Team team = AI::Team(Json::get_s32(element, "team"));
				if (Team::list.count() > s32(team))
				{
					absolute_pos += absolute_rot * Vec3(0, 0, FORCE_FIELD_BASE_OFFSET);
					entity = World::alloc<ForceFieldEntity>(nullptr, absolute_pos, absolute_rot, team, ForceField::Type::Invincible);
					level.core_force_field = entity->get<ForceField>();
				}
			}
		}
		else if (cJSON_HasObjectItem(element, "PlayerTrigger"))
		{
			entity = World::alloc<Empty>();
			PlayerTrigger* trigger = entity->create<PlayerTrigger>();
			trigger->radius = Json::get_vec3(element, "scale", Vec3(1)).x;
		}
		else if (cJSON_HasObjectItem(element, "PointLight"))
		{
			entity = World::alloc<Empty>();
			PointLight* light = entity->create<PointLight>();
			light->color = Json::get_vec3(element, "color");
			light->radius = Json::get_r32(element, "radius");
		}
		else if (cJSON_HasObjectItem(element, "SpotLight"))
		{
			absolute_rot = absolute_rot * Quat::euler(0, 0, PI * 0.5f);
			entity = World::alloc<Empty>();
			SpotLight* light = entity->create<SpotLight>();
			light->color = Json::get_vec3(element, "color");
			light->radius = Json::get_r32(element, "radius");
			light->fov = Json::get_r32(element, "fov");
		}
		else if (cJSON_HasObjectItem(element, "DirectionalLight"))
		{
			DirectionalLight light;
			light.color = Json::get_vec3(element, "color");
			light.shadowed = b8(Json::get_s32(element, "shadowed"));
			light.rot = absolute_rot;
			level.directional_lights.add(light);
		}
		else if (cJSON_HasObjectItem(element, "Cloud"))
		{
			entity = nullptr; // clouds are not part of the entity system
			Clouds::Config config;
			config.color = Json::get_vec4(element, "color");
			config.height = absolute_pos.y;
			config.scale = Json::get_r32(element, "cloud_scale", 1.0f) * 2.0f;
			config.velocity = Vec2(Json::get_r32(element, "velocity_x"), Json::get_r32(element, "velocity_z"));
			config.shadow = Json::get_r32(element, "shadow");
			level.clouds.add(config);
		}
		else if (cJSON_HasObjectItem(element, "WaterSoundNegativeSpace"))
		{
			WaterSoundNegativeSpace space;
			space.pos = absolute_pos;
			space.radius = Json::get_vec3(element, "scale", Vec3(1)).x;
			level.water_sound_negative_spaces.add(space);
		}
		else if (cJSON_HasObjectItem(element, "AIPlayer"))
		{
			// only add an AI player if we are in story mode
			if (session.type == SessionType::Story)
			{
				AI::Team team_original = Json::get_s32(element, "team", 1);
				AI::Team team = team_lookup(level.team_lookup, team_original);
				if (ai_player_count > 1)
				{
					// 2v2 map
					if (story_mode_team == StoryModeTeam::Defend && (team_original == 1 || mersenne::randf_cc() < 0.5f))
						level.ai_config.add(PlayerAI::generate_config(team, 0.0f)); // enemy is attacking; they're there from the beginning
					else
						level.ai_config.add(PlayerAI::generate_config(team, 20.0f + mersenne::randf_cc() * (ZONE_UNDER_ATTACK_TIME * 1.5f)));
				}
				else
				{
					// 1v1 map
					if (story_mode_team == StoryModeTeam::Defend)
						level.ai_config.add(PlayerAI::generate_config(team, 0.0f)); // player is defending, enemy is already there
					else // player is attacking, eventually enemy will come to defend
						level.ai_config.add(PlayerAI::generate_config(team, 20.0f + mersenne::randf_cc() * (ZONE_UNDER_ATTACK_TIME * 1.5f)));
				}
			}
		}
		else if (cJSON_HasObjectItem(element, "Battery"))
		{
			if (level.has_feature(FeatureLevel::Batteries) && session.config.ruleset.enable_batteries)
			{
				AI::Team team;
				if (session.type == SessionType::Story)
				{
					if (story_mode_team == StoryModeTeam::Defend // defending; enemy might have already captured some batteries
						&& mersenne::randf_cc() < 0.2f) // only some batteries though, not all
						team = team_lookup(level.team_lookup, Json::get_s32(element, "team", 1));
					else
						team = AI::TeamNone;
				}
				else
					team = AI::TeamNone;

				entity = World::alloc<BatteryEntity>(absolute_pos, team);
				entity->get<Battery>()->order = Json::get_s32(element, "order", entity->get<Battery>()->id());

				absolute_rot = Quat::identity;

				RopeEntry* rope = ropes.add();
				rope->pos = absolute_pos + Vec3(0, 1, 0);
				rope->rot = Quat::identity;
				rope->slack = 0.0f;
				rope->max_distance = 100.0f;
				rope->attach_end = true;
				rope->flags = Rope::FlagClimbable;

				cJSON* links = cJSON_GetObjectItem(element, "links");
				vi_assert(links && cJSON_GetArraySize(links) == 1);
				const char* spawn_point_name = links->child->valuestring;

				LevelLink<SpawnPoint>* spawn_link = spawn_links.add();
				spawn_link->ref = &entity->get<Battery>()->spawn_point;
				spawn_link->target_name = spawn_point_name;
			}
		}
		else if (cJSON_HasObjectItem(element, "SkyDecal"))
		{
			entity = nullptr; // sky decals are not part of the entity system

			SkyDecals::Config config;
			config.rot = absolute_rot;
			config.color = Vec4(Json::get_r32(element, "r", 1.0f), Json::get_r32(element, "g", 1.0f), Json::get_r32(element, "b", 1.0f), Json::get_r32(element, "a", 1.0f));
			config.scale = Json::get_r32(element, "scale", 1.0f);
			config.texture = Loader::find(Json::get_string(element, "SkyDecal"), AssetLookup::Texture::names);
			level.sky_decals.add(config);
		}
		else if (cJSON_HasObjectItem(element, "Script"))
		{
			const char* name = Json::get_string(element, "Script");
			AssetID script = Script::find(name);
			vi_assert(script != AssetNull);
			level.scripts.add(script);
		}
		else if (cJSON_HasObjectItem(element, "Water"))
		{
			cJSON* meshes = cJSON_GetObjectItem(element, "meshes");

			vi_assert(meshes);
			cJSON* mesh_json = meshes->child;
			vi_assert(mesh_json);

			const char* mesh_ref = mesh_json->valuestring;

			AssetID mesh_id = Loader::find_mesh(mesh_ref);
			vi_assert(mesh_id != AssetNull);

			const Mesh* mesh = Loader::mesh(mesh_id);

			entity = World::alloc<WaterEntity>(mesh_id);
			Water* water = entity->get<Water>();
			water->config.texture = Loader::find(Json::get_string(element, "texture", "water_normal"), AssetLookup::Texture::names);
			water->config.displacement_horizontal = Json::get_r32(element, "displacement_horizontal", 1.25f);
			water->config.displacement_vertical = Json::get_r32(element, "displacement_vertical", 1.0f);
			water->config.ocean = b8(Json::get_s32(element, "ocean", mesh->bounds_radius > 100.0f ? 1 : 0));
		}
		else if (cJSON_HasObjectItem(element, "Prop"))
		{
			const char* name = Json::get_string(element, "Prop");

			b8 alpha = b8(Json::get_s32(element, "alpha"));
			b8 additive = b8(Json::get_s32(element, "additive"));
			Vec3 scale = Json::get_vec3(element, "scale", Vec3(1));
			const char* armature = Json::get_string(element, "armature");
			const char* animation = Json::get_string(element, "animation");
			const char* shader_name = Json::get_string(element, "shader");

			AssetID texture = Loader::find(Json::get_string(element, "texture"), AssetLookup::Texture::names);

			AssetID shader;
			if (shader_name)
				shader = Loader::find(shader_name, AssetLookup::Shader::names);
			else
			{
				if (armature)
					shader = Asset::Shader::armature; // todo: alpha support for skinned meshes
				else
					shader = alpha || additive ? Asset::Shader::flat : Asset::Shader::culled;
			}

			if (name)
			{
				AssetID mesh_id = Loader::find_mesh(name);
				vi_assert(mesh_id != AssetNull);
				AssetID armature_id = Loader::find(armature, AssetLookup::Armature::names);
				vi_assert((armature_id == AssetNull) == (armature == nullptr));
				AssetID animation_id = Loader::find(animation, AssetLookup::Animation::names);
				vi_assert((animation_id == AssetNull) == (animation == nullptr));
				entity = World::create<Prop>(mesh_id, armature_id, animation_id);
				// todo: clean this up
				if (entity->has<View>())
				{
					// View
					entity->get<View>()->texture = texture;
					entity->get<View>()->shader = shader;
					if (cJSON_HasObjectItem(element, "r"))
						entity->get<View>()->color.x = Json::get_r32(element, "r");
					if (cJSON_HasObjectItem(element, "g"))
						entity->get<View>()->color.y = Json::get_r32(element, "g");
					if (cJSON_HasObjectItem(element, "b"))
						entity->get<View>()->color.z = Json::get_r32(element, "b");
					if (cJSON_HasObjectItem(element, "a"))
						entity->get<View>()->color.w = Json::get_r32(element, "a");
					if (!alpha && !additive)
					{
						const Mesh* mesh = Loader::mesh(mesh_id);
						if (mesh->color.w < 0.5f)
							entity->get<View>()->color.w = MATERIAL_INACCESSIBLE;
					}
					if (alpha)
						entity->get<View>()->alpha();
					if (additive)
						entity->get<View>()->additive();
					entity->get<View>()->offset.scale(scale);
				}
				else
				{
					// SkinnedModel
					if (cJSON_HasObjectItem(element, "blend"))
					{
						r32 blend = Json::get_r32(element, "blend");
						Animator* animator = entity->get<Animator>();
						for (s32 i = 0; i < MAX_ANIMATIONS; i++)
							animator->layers[i].blend_time = blend;
					}

					entity->get<SkinnedModel>()->texture = texture;
					entity->get<SkinnedModel>()->shader = shader;
					if (cJSON_HasObjectItem(element, "r"))
						entity->get<SkinnedModel>()->color.x = Json::get_r32(element, "r");
					if (cJSON_HasObjectItem(element, "g"))
						entity->get<SkinnedModel>()->color.y = Json::get_r32(element, "g");
					if (cJSON_HasObjectItem(element, "b"))
						entity->get<SkinnedModel>()->color.z = Json::get_r32(element, "b");
					if (cJSON_HasObjectItem(element, "a"))
						entity->get<SkinnedModel>()->color.w = Json::get_r32(element, "a");
					if (!alpha && !additive)
					{
						const Mesh* mesh = Loader::mesh(mesh_id);
						if (mesh->color.w < 0.5f)
							entity->get<SkinnedModel>()->color.w = MATERIAL_INACCESSIBLE;
					}
					if (alpha)
						entity->get<SkinnedModel>()->alpha();
					if (additive)
						entity->get<SkinnedModel>()->additive();
					entity->get<SkinnedModel>()->offset.scale(scale);
					if (cJSON_HasObjectItem(element, "radius"))
						entity->get<SkinnedModel>()->radius = Json::get_r32(element, "radius");
				}
			}

			cJSON* meshes = cJSON_GetObjectItem(element, "meshes");

			vi_assert(name || meshes);

			if (meshes)
			{
				cJSON* mesh = meshes->child;
				while (mesh)
				{
					const char* mesh_ref = mesh->valuestring;

					AssetID mesh_id = Loader::find_mesh(mesh_ref);

					Entity* m = World::alloc<Prop>(mesh_id);

					m->get<View>()->texture = texture;
					m->get<View>()->shader = shader;
					if (cJSON_HasObjectItem(element, "r"))
						m->get<View>()->color.x = Json::get_r32(element, "r");
					if (cJSON_HasObjectItem(element, "g"))
						m->get<View>()->color.y = Json::get_r32(element, "g");
					if (cJSON_HasObjectItem(element, "b"))
						m->get<View>()->color.z = Json::get_r32(element, "b");
					if (cJSON_HasObjectItem(element, "a"))
						m->get<View>()->color.w = Json::get_r32(element, "a");
					if (!alpha && !additive)
					{
						const Mesh* mesh = Loader::mesh(mesh_id);
						if (!mesh)
							vi_debug("Invalid mesh: %s", mesh_ref);
						if (mesh->color.w < 0.5f)
							m->get<View>()->color.w = MATERIAL_INACCESSIBLE;
					}

					if (alpha)
						m->get<View>()->alpha();
					if (additive)
						m->get<View>()->additive();
					m->get<View>()->offset.scale(scale);

					if (entity)
					{
						World::awake(m);
						m->get<Transform>()->reparent(entity->get<Transform>());
					}
					else
						entity = m;

					mesh = mesh->next;
				}
			}

			if (const char* physics = Json::get_string(element, "physics"))
			{
				const Mesh* mesh = Loader::mesh(entity->get<View>()->mesh);
				const r32 density = 0.5f;
				Vec3 size = (mesh->bounds_max - mesh->bounds_min) * 0.5f;
				r32 mass = size.dot(Vec3(1)) * density;
				if (strcmp(physics, "Box") == 0)
					entity->create<RigidBody>(RigidBody::Type::Box, size, Json::get_r32(element, "mass", mass), CollisionDroneIgnore, ~CollisionAllTeamsForceField, AssetNull, RigidBody::FlagGhost);
				else if (strcmp(physics, "Sphere") == 0)
					entity->create<RigidBody>(RigidBody::Type::Sphere, Vec3(mesh->bounds_radius), Json::get_r32(element, "mass", mass), CollisionDroneIgnore, ~CollisionAllTeamsForceField, AssetNull, RigidBody::FlagGhost);
				else
					vi_assert(false);
			}
		}
		else if (cJSON_HasObjectItem(element, "Tram"))
		{
			s32 track = -1;
			for (s32 i = 0; i < tram_track_entries.length; i++)
			{
				if (strcmp(tram_track_entries[i].tram_name, Json::get_string(element, "name")) == 0)
				{
					track = i;
					break;
				}
			}
			vi_assert(track != -1);
			level.tram_tracks[track].level = Loader::find_level(Json::get_string(element, "level"));
			Entity* runner_a = World::create<TramRunnerEntity>(track, false);
			Entity* runner_b = World::create<TramRunnerEntity>(track, true);
			entity = World::alloc<TramEntity>(runner_a->get<TramRunner>(), runner_b->get<TramRunner>());
			if (Json::get_s32(element, "arrive_only"))
				entity->get<Tram>()->arrive_only = true;
		}
		else if (cJSON_HasObjectItem(element, "Interactable"))
		{
			cJSON* links = cJSON_GetObjectItem(element, "links");
			const char* tram_name = links->child->valuestring;
			s32 track = -1;
			for (s32 i = 0; i < tram_track_entries.length; i++)
			{
				if (strcmp(tram_track_entries[i].tram_name, tram_name) == 0)
				{
					track = i;
					break;
				}
			}
			vi_assert(track != -1);
			entity = World::alloc<TramInteractableEntity>(absolute_pos, absolute_rot, s8(track));
		}
		else if (cJSON_HasObjectItem(element, "Collectible"))
		{
			b8 already_collected = false;
			for (s32 i = 0; i < save.collectibles.length; i++)
			{
				const CollectibleEntry& entry = save.collectibles[i];
				if (entry.zone == level.id && entry.id == collectible_id)
				{
					already_collected = true;
					break;
				}
			}
			if (!already_collected)
			{
				const char* type_str = Json::get_string(element, "Collectible");
				Resource type;
				if (strcmp(type_str, "AccessKeys") == 0)
					type = Resource::AccessKeys;
				else if (strcmp(type_str, "DroneKits") == 0)
					type = Resource::DroneKits;
				else if (strcmp(type_str, "AudioLog") == 0)
					type = Resource::AudioLog;
				else
					type = Resource::Energy;
				entity = World::alloc<CollectibleEntity>(collectible_id, type, s16(Json::get_s32(element, "amount")));
				if (type == Resource::AudioLog)
				{
					AssetID id = Scripts::AudioLogs::get_id(Json::get_string(element, "AudioLog"));
					vi_assert(id != AssetNull);
					entity->get<Collectible>()->audio_log = id;
				}
			}
			collectible_id++;
		}
		else if (cJSON_HasObjectItem(element, "Shop"))
		{
			vi_assert(level.mode != Mode::Pvp);
			entity = World::alloc<ShopEntity>();
			level.shop = entity;

			{
				Entity* i = World::alloc<ShopInteractable>();
				i->get<Transform>()->parent = entity->get<Transform>();
				i->get<Transform>()->pos = Vec3(-3.0f, 0, 0);
				i->get<Interactable>()->user_data = ShopInteractable::flags_default; // todo: allow shop to enable and disable items
				World::awake(i);
			}

			{
				Entity* locke = World::create<Prop>(Asset::Mesh::locke, Asset::Armature::locke);
				locke->get<SkinnedModel>()->color.w = MATERIAL_INACCESSIBLE;
				locke->get<Transform>()->pos = absolute_pos + absolute_rot * Vec3(-1.25f, 0, 0);
				locke->get<Transform>()->rot = absolute_rot;
				locke->add<PlayerTrigger>()->radius = 5.0f;
				locke->get<Animator>()->layers[0].behavior = Animator::Behavior::Freeze;
				level.finder.add("locke", locke);
				level.scripts.add(Script::find("locke"));
			}
		}
		else if (cJSON_HasObjectItem(element, "Glass"))
		{
			Vec3 scale = Json::get_vec3(element, "scale");
			entity = World::alloc<GlassEntity>(Vec2(fabsf(scale.x), fabsf(scale.y)));
		}
		else if (cJSON_HasObjectItem(element, "Empty") || cJSON_HasObjectItem(element, "Camera"))
			entity = World::alloc<Empty>();
		else if (strcmp(Json::get_string(element, "name"), "terminal") == 0)
		{
			vi_assert(level.mode != Mode::Pvp);
			entity = World::alloc<TerminalEntity>();
			level.terminal = entity;

			Entity* i = World::alloc<TerminalInteractable>();
			i->get<Transform>()->parent = entity->get<Transform>();
			i->get<Transform>()->pos = Vec3(1.0f, 0, 0);
			i->get<Interactable>()->user_data = Loader::find_level(Json::get_string(element, "level"));
			World::awake(i);
			level.terminal_interactable = i;
		}

		if (entity && entity->has<Transform>())
		{
			Transform* transform = entity->get<Transform>();

			if (parent != -1)
				transform->parent = transforms[parent];

			transform->absolute(absolute_pos, absolute_rot);

			transforms.add(transform);
		}
		else
			transforms.add(nullptr);

		if (entity)
			level.finder.add(Json::get_string(element, "name"), entity);

		element = element->next;
	}

	for (s32 i = 0; i < spawn_links.length; i++)
	{
		LevelLink<SpawnPoint>* link = &spawn_links[i];
		*link->ref = level.finder.find(link->target_name)->get<SpawnPoint>();
	}

	if (level.mode == Mode::Pvp)
	{
		if (session.config.game_type == GameType::CaptureTheFlag)
		{
			// create flags
			for (s32 i = 0; i < Team::list.count(); i++)
				World::create<FlagEntity>(AI::Team(i));
		}
		else if (session.config.ruleset.enable_spawn_shields)
		{
			// create spawn force fields
			for (auto i = SpawnPoint::list.iterator(); !i.is_last(); i.next())
			{
				if (!i.item()->battery())
				{
					// this is some team's default spawn point; create a force field around it
					Vec3 absolute_pos;
					Quat absolute_rot;
					i.item()->get<Transform>()->absolute(&absolute_pos, &absolute_rot);
					absolute_pos += absolute_rot * Vec3(0, 0, FORCE_FIELD_BASE_OFFSET);
					World::create<ForceFieldEntity>(nullptr, absolute_pos, absolute_rot, i.item()->team, ForceField::Type::Permanent);
				}
			}
		}
	}

	for (s32 i = 0; i < entity_links.length; i++)
	{
		LevelLink<Entity>* link = &entity_links[i];
		*link->ref = level.finder.find(link->target_name);
	}

	for (s32 i = 0; i < level.finder.map.length; i++)
		World::awake(level.finder.map[i].entity.ref());

	Physics::sync_static();

	for (s32 i = 0; i < ropes.length; i++)
	{
		const RopeEntry& entry = ropes[i];
		Rope::spawn(entry.pos, entry.rot * Vec3(0, 1, 0), entry.max_distance, entry.slack, entry.attach_end, entry.flags);
	}

	for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
		World::awake(i.item()->entity());

	awake_all();

	Loader::level_free(json);

#if SERVER
	Net::Server::level_loaded();
#endif
}

void Game::awake_all()
{
	// preload stuff
	{
		if (level.mode == Mode::Pvp)
		{
			Loader::animation(Asset::Animation::character_aim);
			Loader::animation(Asset::Animation::character_fire);
			Loader::animation(Asset::Animation::character_idle);
			Loader::animation(Asset::Animation::character_walk);
			Loader::animation(Asset::Animation::character_melee);

			Loader::mesh(Asset::Mesh::drone);
			Loader::armature(Asset::Armature::drone);
			Loader::animation(Asset::Animation::drone_dash);
			Loader::animation(Asset::Animation::drone_fly);
			Loader::animation(Asset::Animation::drone_idle);
			Loader::mesh(Asset::Mesh::rectifier);
			Loader::mesh(Asset::Mesh::force_field_base);
			Loader::mesh(Asset::Mesh::force_field_sphere);

			Loader::mesh(Asset::Mesh::grenade_attached);
			Loader::mesh(Asset::Mesh::grenade_detached);
		}
		else
		{
			Loader::animation(Asset::Animation::character_climb_down);
			Loader::animation(Asset::Animation::character_climb_up);
			Loader::animation(Asset::Animation::character_fall);
			Loader::animation(Asset::Animation::character_fire);
			Loader::animation(Asset::Animation::character_hang);
			Loader::animation(Asset::Animation::character_idle);
			Loader::animation(Asset::Animation::character_interact);
			Loader::animation(Asset::Animation::character_jump1);
			Loader::animation(Asset::Animation::character_land);
			Loader::animation(Asset::Animation::character_land_hard);
			Loader::animation(Asset::Animation::character_mantle);
			Loader::animation(Asset::Animation::character_pickup);
			Loader::animation(Asset::Animation::character_run);
			Loader::animation(Asset::Animation::character_run_backward);
			Loader::animation(Asset::Animation::character_run_left);
			Loader::animation(Asset::Animation::character_run_right);
			Loader::animation(Asset::Animation::character_terminal_enter);
			Loader::animation(Asset::Animation::character_terminal_exit);
			Loader::animation(Asset::Animation::character_top_out);
			Loader::animation(Asset::Animation::character_walk);
			Loader::animation(Asset::Animation::character_walk_backward);
			Loader::animation(Asset::Animation::character_walk_left);
			Loader::animation(Asset::Animation::character_walk_right);
			Loader::animation(Asset::Animation::character_wall_run_left);
			Loader::animation(Asset::Animation::character_wall_run_right);
			Loader::animation(Asset::Animation::character_wall_run_straight);
			Loader::animation(Asset::Animation::character_wall_slide);

			Loader::shader(Asset::Shader::flat_texture_offset);

			Loader::mesh(Asset::Mesh::tile);
			Loader::mesh(Asset::Mesh::reticle_grapple);
		}

		Loader::mesh_permanent(Asset::Mesh::character);
		Loader::armature_permanent(Asset::Armature::character);

		Loader::mesh(Asset::Mesh::cylinder);
		Loader::mesh(Asset::Mesh::sphere_highres);
		Loader::mesh_permanent(Asset::Mesh::tri_tube);
		Loader::mesh_instanced(Asset::Mesh::tri_tube);
		Loader::mesh_permanent(Asset::Mesh::shell_casing);
		Loader::mesh_instanced(Asset::Mesh::shell_casing);

		Loader::mesh_permanent(Asset::Mesh::heal_effect);
		Loader::mesh_instanced(Asset::Mesh::heal_effect);

		Loader::mesh_permanent(Asset::Mesh::icon_chevron);
		Loader::mesh_permanent(Asset::Mesh::icon_warning);
		Loader::mesh_permanent(Asset::Mesh::icon_turret2);
		Loader::mesh_permanent(Asset::Mesh::icon_turret);
		Loader::mesh_permanent(Asset::Mesh::icon_spot);
		Loader::mesh_permanent(Asset::Mesh::icon_sniper);
		Loader::mesh_permanent(Asset::Mesh::icon_shotgun);
		Loader::mesh_permanent(Asset::Mesh::icon_rectifier);
		Loader::mesh_permanent(Asset::Mesh::icon_network_error);
		Loader::mesh_permanent(Asset::Mesh::icon_minion);
		Loader::mesh_permanent(Asset::Mesh::icon_grenade);
		Loader::mesh_permanent(Asset::Mesh::icon_gamepad);
		Loader::mesh_permanent(Asset::Mesh::icon_force_field);
		Loader::mesh_permanent(Asset::Mesh::icon_flag);
		Loader::mesh_permanent(Asset::Mesh::icon_drone);
		Loader::mesh_permanent(Asset::Mesh::icon_core_module);
		Loader::mesh_permanent(Asset::Mesh::icon_close);
		Loader::mesh_permanent(Asset::Mesh::icon_checkmark);
		Loader::mesh_permanent(Asset::Mesh::icon_bolter);
		Loader::mesh_permanent(Asset::Mesh::icon_battery);
		Loader::mesh_permanent(Asset::Mesh::icon_arrow_main);
		Loader::mesh_permanent(Asset::Mesh::icon_active_armor);
		Loader::mesh_permanent(Asset::Mesh::icon_access_key);
		Loader::mesh_permanent(Asset::Mesh::icon_ability_pip);
		Loader::mesh_permanent(Asset::Mesh::icon_reticle_invalid);
	}

	if (level.rain > 0.0f)
		Rain::audio_init();

#if !SERVER
	if (Settings::expo)
		Audio::volume_multiplier(Net::Client::replay_mode() == Net::Client::ReplayMode::Replaying ? 0.25f : 1.0f);
#endif

	if (level.ambience1[0])
		Audio::post_global(Audio::get_id(level.ambience1));
	if (level.ambience2[0])
		Audio::post_global(Audio::get_id(level.ambience2));

	for (s32 i = 0; i < level.scripts.length; i++)
		Script::list[level.scripts[i]].function(level.finder);

	Team::awake_all();

	Loader::nav_mesh(level.id, session.config.game_type);

#if !SERVER && !defined(__ORBIS__)
	discord_update_presence();
#endif
}

b8 Game::should_pause()
{
	return level.local && PlayerHuman::list.count() == 1;
}


}
