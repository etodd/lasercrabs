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

#if DEBUG
	#define DEBUG_NAV_MESH 0
	#define DEBUG_AI_PATH 0
	#define DEBUG_DRONE_NAV_MESH 0
	#define DEBUG_DRONE_AI_PATH 0
	#define DEBUG_PHYSICS 0
#endif

#include "game.h"

namespace VI
{

b8 Game::quit = false;
s32 Game::width;
s32 Game::height;
GameTime Game::time;
GameTime Game::real_time;
r32 Game::physics_timestep;
r32 Game::inactive_timer;

Gamepad::Type Game::ui_gamepad_types[MAX_GAMEPADS] = { };
AssetID Game::scheduled_load_level = AssetNull;
AssetID Game::scheduled_dialog = AssetNull;
Game::Mode Game::scheduled_mode = Game::Mode::Pvp;
r32 Game::schedule_timer;
Net::Master::Save Game::save;
Game::Level Game::level;
Game::Session Game::session;
b8 Game::cancel_event_eaten[] = {};
ScreenQuad Game::screen_quad;

Game::Session::Session()
	:
#if SERVER
	local_player_config{ AI::TeamNone, AI::TeamNone, AI::TeamNone, AI::TeamNone },
#else
	local_player_config{ 0, AI::TeamNone, AI::TeamNone, AI::TeamNone },
#endif
	type(SessionType::Story),
	player_slots(1),
	team_count(2),
	time_scale(1.0f),
	time_limit(MATCH_TIME_DEFAULT),
	game_type(GameType::Assault),
	respawns(DEFAULT_ASSAULT_DRONES),
	kill_limit(8)
{
	for (s32 i = 0; i < MAX_PLAYERS; i++)
		local_player_uuids[i] = mersenne::rand_u64();
}

r32 Game::Session::effective_time_scale() const
{
	return time_scale;
}

s32 Game::Session::local_player_count() const
{
	s32 count = 0;
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		if (local_player_config[i] != AI::TeamNone)
			count++;
	}
	return count;
}

void Game::Session::reset()
{
	this->~Session();
	new (this) Session();
}

b8 Game::Level::has_feature(FeatureLevel f) const
{
	return s32(feature_level) >= s32(f);
}

AI::Team Game::Level::team_lookup_reverse(AI::Team t) const
{
	for (s32 i = 0; i < MAX_PLAYERS; i++)
	{
		if (team_lookup[i] == t)
			return AI::Team(i);
	}
	return AI::TeamNone;
}

Array<UpdateFunction> Game::updates;
Array<DrawFunction> Game::draws;
Array<CleanupFunction> Game::cleanups;

b8 Game::init(LoopSync* sync)
{
	// count scripts
	while (true)
	{
		if (Script::list[Script::count].name)
			Script::count++;
		else
			break;
	}

	World::init();

	{
		cJSON* overworld_level = Loader::level(Asset::Level::overworld, GameType::Assault, false);
		Overworld::init(overworld_level);
		Loader::level_free(overworld_level);
	}

	if (!Net::init())
		return false;

#if !SERVER
	// replay files
	{
		const char* replay_dir = "rec/";
		DIR* dir = opendir(replay_dir);
		if (dir)
		{
			Net::Client::record = true;
			struct dirent* entry;
			while ((entry = readdir(dir)))
			{
				if (entry->d_type != DT_REG)
					continue; // not a file

				char filename[MAX_PATH_LENGTH];
				sprintf(filename, "%s%s", replay_dir, entry->d_name);
				Net::Client::replay_file_add(filename);
			}
			closedir(dir);
		}
	}

	if (!Audio::init())
		return false;

	Loader::font_permanent(Asset::Font::lowpoly);
	Loader::font_permanent(Asset::Font::pt_sans);

	if (!Loader::soundbank_permanent(Asset::Soundbank::Init))
		return false;
	if (!Loader::soundbank_permanent(Asset::Soundbank::SOUNDBANK))
		return false;

	// strings
	{
		const char* language_file = "language.txt";
		cJSON* json_language = Json::load(language_file);
		const char* language = Json::get_string(json_language, "language", "en");
		char string_file[255];
		sprintf(string_file, "assets/str/%s.json", language);

		// UI
		{
			cJSON* json = Json::load(string_file);
			for (s32 i = 0; i < Asset::String::count; i++)
			{
				const char* name = AssetLookup::String::names[i];
				cJSON* value = cJSON_GetObjectItem(json, name);
				strings_set(i, value ? value->valuestring : nullptr);
			}
		}

		Input::load_strings(); // loads localized strings for input bindings

		// don't free the JSON objects; we'll read strings directly from them
	}

	Menu::init(sync->input);

	for (s32 i = 0; i < ParticleSystem::list.length; i++)
		ParticleSystem::list[i]->init(sync);

	UI::init(sync);

	Console::init();
#endif

	return true;
}

void Game::update(const Update& update_in)
{
	width = update_in.input->width;
	height = update_in.input->height;

	real_time = update_in.time;
	time.delta = update_in.time.delta * session.effective_time_scale();

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
#if !SERVER
			if (level.local
				&& session.type == SessionType::Story
				&& level.id == Asset::Level::Dock
				&& scheduled_load_level == Asset::Level::Port_District) // we're playing locally on the title screen; need to switch to a server
			{
				save.zone_last = level.id; // hack to ensure hand-off works correctly
				unload_level();
				Net::Master::ServerState s;
				s.make_story();
				s.level = scheduled_load_level;
				Net::Client::allocate_server(s);
				scheduled_load_level = AssetNull;
			}
			else
#endif
				load_level(scheduled_load_level, scheduled_mode);
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
	update_game = (!Overworld::modal() || level.id == Asset::Level::overworld) && (level.local || Net::Client::mode() == Net::Client::Mode::Connected);
#endif

	if (update_game)
		physics_timestep = (1.0f / 60.0f) * session.effective_time_scale();
	else
		physics_timestep = 0.0f;

	Update u = update_in;
	u.time = time;

	if (update_game)
	{
		time.total += time.delta;
		Team::match_time += time.delta;
		ParticleSystem::time = time.total;
		for (s32 i = 0; i < ParticleSystem::list.length; i++)
			ParticleSystem::list[i]->update(u);
	}
	else if (Overworld::modal()) // particles still work in overworld even though the rest of the world is paused
	{
		ParticleSystem::time += time.delta;
		for (s32 i = 0; i < ParticleSystem::list.length; i++)
			ParticleSystem::list[i]->update(u);
	}

	Net::update_start(u);

#if !SERVER
	// trigger attract mode
	if (Net::Client::replay_file_count() > 0 && Net::Client::replay_mode() != Net::Client::ReplayMode::Replaying)
	{
		inactive_timer += u.time.delta;
		if (update_in.input->keys.any()
			|| update_in.input->cursor_x != 0 || update_in.input->cursor_y != 0)
			inactive_timer = 0.0f;
		else
		{
			for (s32 i = 0; i < MAX_GAMEPADS; i++)
			{
				const Gamepad& gamepad = update_in.input->gamepads[i];
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
		if (inactive_timer > 60.0f)
		{
			unload_level();
			save.reset();
			Net::Client::replay();
			inactive_timer = 0.0f;
		}
	}

	// determine whether to display gamepad or keyboard bindings
	{
		b8 refresh = false;
		for (s32 i = 0; i < MAX_GAMEPADS; i++)
		{
			const Gamepad& gamepad = update_in.input->gamepads[i];
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
						|| update_in.input->cursor_x != 0
						|| update_in.input->cursor_y != 0
						|| update_in.input->keys.any())
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
	Ascensions::update(u);
#endif

	Overworld::update(u);

	AI::update(u);

	if (update_game)
	{
		Physics::sync_dynamic();

		for (auto i = Ragdoll::list.iterator(); !i.is_last(); i.next())
		{
			if (level.local)
				i.item()->update_server(u);
			i.item()->update_client(u);
		}
		for (auto i = Animator::list.iterator(); !i.is_last(); i.next())
		{
			if (!level.local && i.item()->has<Minion>())
				i.item()->update_client_only(u); // minion animations are synced over the network
			else if (!i.item()->has<Parkour>()) // Parkour component updates the Animator on its own terms
				i.item()->update_server(u);
		}

		for (auto i = TramRunner::list.iterator(); !i.is_last(); i.next())
		{
			if (level.local)
				i.item()->update_server(u);
			i.item()->update_client(u);
		}

		Physics::sync_static();

		AI::update(u);

		Team::update(u);

		PlayerManager::update_all(u);
		PlayerHuman::update_all(u);
		if (level.local)
		{
			SpawnPoint::update_server_all(u);
			if (session.type == SessionType::Story && level.mode == Mode::Pvp && !Team::game_over)
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
			for (auto i = Health::list.iterator(); !i.is_last(); i.next())
				i.item()->update_server(u);
			for (auto i = Walker::list.iterator(); !i.is_last(); i.next())
				i.item()->update(u);
			for (auto i = Grenade::list.iterator(); !i.is_last(); i.next())
				i.item()->update_server(u);
			for (auto i = Minion::list.iterator(); !i.is_last(); i.next())
				i.item()->update_server(u);
			for (auto i = PlayerAI::list.iterator(); !i.is_last(); i.next())
				i.item()->update(u);
			for (auto i = Bolt::list.iterator(); !i.is_last(); i.next())
				i.item()->update_server(u);
		}

		for (auto i = Health::list.iterator(); !i.is_last(); i.next())
			i.item()->update_client(u);
		Bolt::update_client_all(u);
		Turret::update_client_all(u);
		Minion::update_client_all(u);
		Grenade::update_client_all(u);
		for (auto i = Drone::list.iterator(); !i.is_last(); i.next())
		{
			if (level.local || (i.item()->has<PlayerControlHuman>() && i.item()->get<PlayerControlHuman>()->local()))
				i.item()->update_server(u);
		}
		Drone::update_client_all(u);
		for (auto i = PlayerControlAI::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
		for (auto i = PlayerTrigger::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
		Battery::update_all(u);
		Sensor::update_client_all(u);
		ForceField::update_all(u);
		for (auto i = EffectLight::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
		for (auto i = PlayerCommon::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
		for (auto i = PlayerControlHuman::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
		for (auto i = Parkour::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->get<PlayerControlHuman>()->local())
			{
				if (!level.local)
					i.item()->get<Walker>()->update(u); // walkers are normally only updated on the server
				i.item()->update(u);
			}
			else if (level.local) // server needs to manually update the animator because it's normally updated by the Parkour component
				i.item()->get<Animator>()->update_server(u);
		}

		Shield::update_client_all(u);

		for (auto i = PlayerControlHuman::list.iterator(); !i.is_last(); i.next())
			i.item()->update_late(u);

		for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
			i.item()->update_late(u);

		for (auto i = Water::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);

		for (s32 i = 0; i < updates.length; i++)
			(*updates[i])(u);

		if (level.rain > 0.0f)
			Rain::spawn(u, level.rain);
	}

	Console::update(u);

	World::flush();

	Audio::update();

#if !SERVER
	Menu::update_end(u);
#endif

	Net::update_end(u);
}

void Game::term()
{
	Net::term();
	Audio::term();
}

b8 Game::edge_trigger(r32 time, b8(*fn)(r32))
{
	return fn(time) != fn(time - real_time.delta);
}

b8 Game::edge_trigger(r32 time, r32 speed, b8(*fn)(r32, r32))
{
	return fn(time, speed) != fn(time - real_time.delta, speed);
}

// return true if this entity's transform needs synced over the network
b8 Game::net_transform_filter(const Entity* t, Mode mode)
{
	// energy pickups are not synced in parkour mode

	if (t->has<Sensor>() && !t->has<Battery>())
		return true;

	const ComponentMask mask_parkour =
	(
		Drone::component_mask
		| Bolt::component_mask
		| Minion::component_mask
		| Grenade::component_mask
		| TramRunner::component_mask
	);
	const ComponentMask mask_pvp =
	(
		mask_parkour
		| Battery::component_mask
	);
	return t->component_mask & (mode == Mode::Pvp ? mask_pvp : mask_parkour);
}

#if SERVER

void Game::draw_opaque(const RenderParams&) { }
void Game::draw_override(const RenderParams&) { }
void Game::draw_alpha(const RenderParams&) { }
void Game::draw_hollow(const RenderParams&) { }
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
	View::draw_opaque(render_params);

	Water::draw_opaque(render_params);

	SkinnedModel::draw_opaque(render_params);

	if (render_params.technique == RenderTechnique::Default
		&& !(render_params.flags & RenderFlagEdges))
	{
		SkyPattern::draw_opaque(render_params);

		// render back faces of culled geometry
		if (render_params.camera->cull_range > 0.0f)
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

	if (render_params.technique == RenderTechnique::Shadow && !Overworld::modal())
		Rope::draw(render_params);
}

void Game::draw_override(const RenderParams& params)
{
	Overworld::draw_override(params);
}

void Game::draw_alpha(const RenderParams& render_params)
{
	if (render_params.camera->flag(CameraFlagFog))
	{
		Skybox::draw_alpha(render_params);
		SkyDecal::draw_alpha(render_params);
		Clouds::draw_alpha(render_params);
	}

#if DEBUG_NAV_MESH
	AI::debug_draw_nav_mesh(render_params);
#endif

#if DEBUG_DRONE_NAV_MESH
	AI::debug_draw_drone_nav_mesh(render_params);
#endif

#if DEBUG_AI_PATH
	{
		UIText text;
		text.color = UI::color_accent;
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
					mesh_id = Asset::Mesh::cube;
					radius = body->size;
					color = Vec4(1, 0, 0, 1);
					break;
				case RigidBody::Type::Sphere:
					mesh_id = Asset::Mesh::sphere;
					radius = body->size;
					color = Vec4(1, 0, 0, 1);
					break;
				case RigidBody::Type::CapsuleX:
					// capsules: size.x = radius, size.y = height
					mesh_id = Asset::Mesh::cube;
					radius = Vec3((body->size.y + body->size.x * 2.0f) * 0.5f, body->size.x, body->size.x);
					color = Vec4(0, 1, 0, 1);
					break;
				case RigidBody::Type::CapsuleY:
					mesh_id = Asset::Mesh::cube;
					radius = Vec3(body->size.x, (body->size.y + body->size.x * 2.0f) * 0.5f, body->size.x);
					color = Vec4(0, 1, 0, 1);
					break;
				case RigidBody::Type::CapsuleZ:
					mesh_id = Asset::Mesh::cube;
					radius = Vec3(body->size.x, body->size.x, (body->size.y + body->size.x * 2.0f) * 0.5f);
					color = Vec4(0, 1, 0, 1);
					break;
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

	SkinnedModel::draw_alpha(render_params);

	View::draw_alpha(render_params);

	EffectLight::draw_alpha(render_params);
	
	Ascensions::draw_ui(render_params);

	for (auto i = PlayerControlHuman::list.iterator(); !i.is_last(); i.next())
		i.item()->draw_ui(render_params);

	for (s32 i = 0; i < draws.length; i++)
		(*draws[i])(render_params);

	Overworld::draw_ui(render_params);

	for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
		i.item()->draw_ui(render_params);

	Team::draw_ui(render_params);

	Menu::draw(render_params);

	if (schedule_timer > 0.0f && schedule_timer < TRANSITION_TIME)
		Menu::draw_letterbox(render_params, schedule_timer, TRANSITION_TIME);

	Console::draw(render_params);
}

void Game::draw_hollow(const RenderParams& render_params)
{
	SkyPattern::draw_hollow(render_params);

	Overworld::draw_hollow(render_params);

	for (auto i = Water::list.iterator(); !i.is_last(); i.next())
		i.item()->draw_hollow(render_params);
}

void Game::draw_particles(const RenderParams& render_params)
{
	if (render_params.camera->mask && !Overworld::modal())
		Rope::draw(render_params);

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

		if (Game::level.type == GameType::Deathmatch)
			player->kills = Game::level.kill_limit;
		else if (Game::level.type == GameType::Assault)
		{
			if (player->team.ref()->team() == 0) // defending
				Team::match_time = Game::level.time_limit;
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
	else if (strcmp(cmd, "solve") == 0)
	{
		for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
			i.item()->sudoku.solve(i.item());
	}
#if !SERVER
	else if (strstr(cmd, "conn") == cmd)
	{
		// connect to server
		const char* delimiter = strchr(cmd, ' ');
		const char* host;
		if (delimiter)
			host = delimiter + 1;
		else
			host = "127.0.0.1";

		unload_level();
		save.reset();
		Net::Client::connect(host, 3494);
	}
	else if (strstr(cmd, "replay") == cmd)
	{
		const char* delimiter = strchr(cmd, ' ');
		const char* filename;
		if (delimiter)
			filename = delimiter + 1;
		else
			filename = nullptr;
		if (!filename || strlen(filename) < MAX_PATH_LENGTH)
		{
			unload_level();
			save.reset();
			Net::Client::replay(filename);
		}
	}
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
			Net::Master::ServerState s;
			s.make_story();
			s.level = level;
			Net::Client::allocate_server(s);
		}
	}
#endif
#if DEBUG && !SERVER
	else if (!level.local && Net::Client::mode() == Net::Client::Mode::Connected)
	{
		Net::Client::execute(cmd);
		return;
	}
#endif
	else if (strcmp(cmd, "killai") == 0)
	{
		for (auto i = PlayerControlAI::list.iterator(); !i.is_last(); i.next())
		{
			Health* health = i.item()->get<Health>();
			health->damage(nullptr, health->hp_max + health->shield_max);
		}
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
		{
			Health* health = i.item()->get<Health>();
			health->damage(nullptr, health->hp_max + health->shield_max);
		}
	}
	else if (strcmp(cmd, "noclip") == 0)
	{
		level.continue_match_after_death = !level.continue_match_after_death;
		if (level.continue_match_after_death)
		{
			for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
			{
				Entity* entity = i.item()->get<PlayerManager>()->instance.ref();
				if (entity)
					World::remove(entity);
			}
		}
	}
	else if (strstr(cmd, "timescale ") == cmd)
	{
		const char* delimiter = strchr(cmd, ' ');
		const char* number_string = delimiter + 1;
		char* end;
		r32 value = std::strtod(number_string, &end);
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
		s32 value = (s32)std::strtol(number_string, &end, 10);
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
	else if (strstr(cmd, "upgrade") == cmd)
	{
		for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->instance.ref())
			{
				s16 energy = i.item()->energy;
				i.item()->energy = 10000;
				for (s32 upgrade = 0; upgrade < s32(Upgrade::count); upgrade++)
				{
					while (i.item()->upgrade_available(Upgrade(upgrade)))
					{
						i.item()->upgrade_start(Upgrade(upgrade));
						i.item()->upgrade_complete();
					}
				}
				i.item()->energy = energy;
			}
		}
	}
	else if (strstr(cmd, "lda ") == cmd)
	{
		// AI test
		const char* delimiter = strchr(cmd, ' ');
		const char* level_name = delimiter + 1;
		AssetID level = Loader::find_level(level_name);
		if (level != AssetNull)
		{
			save.reset();
			save.zone_current = level;
			load_level(level, Mode::Pvp, true);
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
		r32 value = std::strtod(number_string, &end);
		if (*end == '\0')
		{
			for (s32 i = 0; i < s32(Resource::count); i++)
				Overworld::resource_change(Resource(i), value);
		}
	}
	else if (strstr(cmd, "capture ") == cmd)
	{
		const char* delimiter = strchr(cmd, ' ');
		const char* zone_string = delimiter + 1;
		AssetID id = Loader::find(zone_string, AssetLookup::Level::names);
		Overworld::zone_change(id, ZoneState::Friendly);
	}
	else if (strcmp(cmd, "unlock") == 0)
		Overworld::zone_change(level.id, ZoneState::Hostile);
	else
		Overworld::execute(cmd);
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

	Overworld::clear();
	Ascensions::clear();
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
		Audio::listener_disable(i);

	World::clear(); // deletes all entities

	// PlayerAI is not part of the entity system
	PlayerAI::list.clear();

	PlayerHuman::clear(); // clear some random player-related stuff

	Particles::clear();

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
	level.skybox.ambient_color = Vec3::zero;
	level.skybox.shader = AssetNull;
	level.skybox.texture = AssetNull;
	level.skybox.mesh = AssetNull;

	Audio::post_global_event(AK::EVENTS::STOP_ALL);

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
	b8 attach_end;
};

AI::Team team_lookup(const AI::Team* table, s32 i)
{
	return table[vi_max(0, vi_min(MAX_PLAYERS, i))];
}

void Game::load_level(AssetID l, Mode m, b8 ai_test)
{
	vi_debug("Loading level %d", s32(l));

	AssetID last_level = level.id;
	Mode last_mode = level.mode;
	unload_level();

	if (m == Mode::Parkour)
	{
		if (l != last_level && last_mode == Mode::Parkour)
		{
			save.zone_last = last_level;
			if (!save.zone_current_restore)
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

	Array<Transform*> transforms;

	Array<RopeEntry> ropes;

	Array<LevelLink<SpawnPoint>> spawn_links;

	level.time_limit = session.time_limit;
	level.respawns = session.respawns;
	level.kill_limit = session.kill_limit;
	level.type = session.game_type;

	cJSON* json = Loader::level(l, level.type, true);

	level.mode = m;
	level.id = l;
	level.local = true;
	if (m == Mode::Pvp)
		level.post_pvp = true;

	if (save.zone_current_restore) // we are restoring the player's position in this map; must have happened after a PvP match
		level.post_pvp = true;

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
	s32 default_team_index = (save.zones[level.id] == ZoneState::Friendly || save.zones[level.id] == ZoneState::GroupOwned) ? 0 : 1;

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
				transforms[parent]->to_world(&absolute_pos, &absolute_rot);
		}

		if (cJSON_HasObjectItem(element, "World"))
		{
			// World is guaranteed to be the first element in the entity list

			level.feature_level = FeatureLevel(Json::get_s32(element, "feature_level", s32(FeatureLevel::All)));

			level.rain = Json::get_r32(element, "rain");

			// fill team lookup table
			{
				s32 offset;
				if (session.type == SessionType::Story)
				{
					if (save.zones[level.id] == ZoneState::Friendly || save.zones[level.id] == ZoneState::GroupOwned)
						offset = 0; // put local player on team 0 (defenders)
					else
						offset = 1; // put local player on team 1 (attackers)
				}
				else
				{
					// shuffle teams and make sure they're packed in the array starting at 0
					b8 lock_teams = Json::get_s32(element, "lock_teams");
					offset = lock_teams ? 0 : mersenne::rand() % session.team_count;
				}
				for (s32 i = 0; i < MAX_PLAYERS; i++)
					level.team_lookup[i] = AI::Team((offset + i) % session.team_count);
			}

			level.skybox.far_plane = Json::get_r32(element, "far_plane", 100.0f);
			level.skybox.fog_start = Json::get_r32(element, "fog_start", level.skybox.far_plane * 0.25f);
			level.skybox.texture = Loader::find(Json::get_string(element, "skybox_texture"), AssetLookup::Texture::names);
			level.skybox.shader = Asset::Shader::skybox;
			level.skybox.mesh = Asset::Mesh::skybox;
			level.skybox.color = Json::get_vec3(element, "skybox_color");
			level.skybox.ambient_color = Json::get_vec3(element, "ambient_color");

			level.min_y = Json::get_r32(element, "min_y", -20.0f);
			level.rotation = Json::get_r32(element, "rotation");

			// initialize teams
			if (m != Mode::Special || level.id == Asset::Level::Dock)
			{
				for (s32 i = 0; i < session.team_count; i++)
				{
					Entity* e = World::alloc<ContainerEntity>(); // team entities get awoken and finalized at the end of load_level()
					e->create<Team>();
				}

				for (s32 i = 0; i < MAX_GAMEPADS; i++)
				{
					if (session.local_player_config[i] != AI::TeamNone)
					{
						AI::Team team = team_lookup(level.team_lookup, s32(session.local_player_config[i]));

						char username[MAX_USERNAME + 64] = {};
						if (ai_test)
							strncpy(username, Usernames::all[mersenne::rand_u32() % Usernames::count], MAX_USERNAME);
						else
						{
							if (level.local && session.type != SessionType::Story)
								sprintf(username, _(strings::player), i + 1);
							else
							{
								if (i == 0)
									sprintf(username, "%s", save.username);
								else
									sprintf(username, "%s [%d]", save.username, i + 1);
							}
						}

						Entity* e = World::alloc<ContainerEntity>();
						PlayerManager* manager = e->create<PlayerManager>(&Team::list[(s32)team], username);

						if (ai_test)
						{
							PlayerAI* player = PlayerAI::list.add();
							new (player) PlayerAI(manager, PlayerAI::generate_config(team, 0.0f));
						}
						else
							e->create<PlayerHuman>(true, i); // local = true
						// container entity will be finalized later
					}
				}
			}
		}
		else if (Json::get_s32(element, "min_players") > PlayerManager::list.count() + ai_player_count
			|| Json::get_s32(element, "min_teams") > Team::list.count())
		{
			// not enough players or teams
			// don't spawn the entity
		}
		else if (cJSON_HasObjectItem(element, "StaticGeom"))
		{
			b8 alpha = (b8)Json::get_s32(element, "alpha");
			b8 additive = (b8)Json::get_s32(element, "additive");
			b8 no_parkour = cJSON_HasObjectItem(element, "noparkour");
			b8 invisible = cJSON_HasObjectItem(element, "invisible");
			AssetID texture = Loader::find(Json::get_string(element, "texture"), AssetLookup::Texture::names);
			s16 extra_flags = cJSON_HasObjectItem(element, "electric") ? CollisionElectric : 0;

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
							m = World::alloc<StaticGeom>(mesh_id, absolute_pos, absolute_rot, CollisionInaccessible | extra_flags, ~CollisionParkour & ~CollisionInaccessible & ~CollisionElectric);
						else
							m = World::alloc<StaticGeom>(mesh_id, absolute_pos, absolute_rot, CollisionParkour | CollisionInaccessible | extra_flags, ~CollisionParkour & ~CollisionInaccessible & ~CollisionElectric);
						m->get<View>()->color.w = MATERIAL_INACCESSIBLE;
					}
					else
					{
						// accessible
						if (no_parkour) // no parkour material
							m = World::alloc<StaticGeom>(mesh_id, absolute_pos, absolute_rot, extra_flags, ~CollisionParkour & ~CollisionInaccessible & ~CollisionElectric);
						else
							m = World::alloc<StaticGeom>(mesh_id, absolute_pos, absolute_rot, CollisionParkour | extra_flags, ~CollisionParkour & ~CollisionInaccessible & ~CollisionElectric);
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
			rope->attach_end = b8(Json::get_s32(element, "attach_end", 0));
		}
		else if (cJSON_HasObjectItem(element, "Turret"))
		{
			entity = World::alloc<StaticGeom>(Asset::Mesh::turret_base, absolute_pos, absolute_rot, CollisionInaccessible, ~CollisionParkour & ~CollisionInaccessible & ~CollisionElectric);
			entity->get<View>()->color.w = MATERIAL_INACCESSIBLE;

			if (level.type == GameType::Assault)
			{
				Entity* turret = World::alloc<TurretEntity>(AI::Team(0));
				turret->get<Transform>()->absolute(absolute_pos + absolute_rot * Vec3(0, 0, TURRET_HEIGHT), absolute_rot);

				cJSON* links = cJSON_GetObjectItem(element, "links");
				cJSON* link = links->child;
				while (link)
				{
					const char* ingress_entity_name = link->valuestring;

					// look up ingress point
					cJSON* i = json->child;
					b8 found = false;
					while (i)
					{
						if (strcmp(cJSON_GetObjectItem(i, "name")->valuestring, ingress_entity_name) == 0)
						{
							turret->get<Turret>()->ingress_points.add(Json::get_vec3(i, "pos"));
							found = true;
							break;
						}
						i = i->next;
					}
					vi_assert(found);

					link = link->next;
				}

				World::awake(turret);
			}
		}
		else if (cJSON_HasObjectItem(element, "Minion"))
		{
			if (session.type == SessionType::Story)
			{
				// starts out owned by player if the zone is friendly
				AI::Team team = team_lookup(level.team_lookup, Json::get_s32(element, "team", default_team_index));
				entity = World::alloc<MinionEntity>(absolute_pos, absolute_rot, team);
			}
		}
		else if (cJSON_HasObjectItem(element, "SpawnPoint"))
		{
			AI::Team team = AI::Team(Json::get_s32(element, "team"));

			if (Team::list.count() > s32(team))
				entity = World::alloc<SpawnPointEntity>(team, Json::get_s32(element, "visible", 1));
		}
		else if (cJSON_HasObjectItem(element, "CoreModule"))
		{
			if (level.type == GameType::Assault)
			{
				AI::Team team = AI::Team(Json::get_s32(element, "team"));
				if (Team::list.count() > s32(team))
					entity = World::alloc<CoreModuleEntity>(team, nullptr, absolute_pos, absolute_rot);
			}
		}
		else if (cJSON_HasObjectItem(element, "ForceField"))
		{
			if (level.type == GameType::Assault)
			{
				AI::Team team = AI::Team(Json::get_s32(element, "team"));
				if (Team::list.count() > s32(team))
				{
					absolute_pos += absolute_rot * Vec3(0, 0, FORCE_FIELD_BASE_OFFSET);
					entity = World::alloc<ForceFieldEntity>(nullptr, absolute_pos, absolute_rot, team, ForceField::Type::Permanent);
				}
			}
		}
		else if (cJSON_HasObjectItem(element, "PlayerTrigger"))
		{
			entity = World::alloc<Empty>();
			PlayerTrigger* trigger = entity->create<PlayerTrigger>();
			trigger->radius = Json::get_r32(element, "scale", 1.0f);
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
			entity = World::alloc<Empty>();
			DirectionalLight* light = entity->create<DirectionalLight>();
			light->color = Json::get_vec3(element, "color");
			light->shadowed = Json::get_s32(element, "shadowed");
		}
		else if (cJSON_HasObjectItem(element, "Cloud"))
		{
			entity = nullptr; // clouds are not part of the entity system
			Clouds::Config config;
			config.color = Json::get_vec4(element, "color");
			config.height = absolute_pos.y;
			config.scale = Json::get_r32(element, "scale", 1.0f) * 2.0f;
			config.velocity = Vec2(Json::get_r32(element, "velocity_x"), Json::get_r32(element, "velocity_z"));
			config.shadow = Json::get_r32(element, "shadow");
			level.clouds.add(config);
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
					if ((save.zones[level.id] == ZoneState::Friendly || save.zones[level.id] == ZoneState::GroupOwned) && (team_original == 1 || mersenne::randf_cc() < 0.5f))
						level.ai_config.add(PlayerAI::generate_config(team, 0.0f)); // enemy is attacking; they're there from the beginning
					else
						level.ai_config.add(PlayerAI::generate_config(team, 20.0f + mersenne::randf_cc() * (ZONE_UNDER_ATTACK_THRESHOLD * 1.5f)));
				}
				else
				{
					// 1v1 map
					if (save.zones[level.id] == ZoneState::Friendly || save.zones[level.id] == ZoneState::GroupOwned)
						level.ai_config.add(PlayerAI::generate_config(team, 0.0f)); // player is defending, enemy is already there
					else // player is attacking, eventually enemy will come to defend
						level.ai_config.add(PlayerAI::generate_config(team, 20.0f + mersenne::randf_cc() * (ZONE_UNDER_ATTACK_THRESHOLD * 1.5f)));
				}
			}
		}
		else if (cJSON_HasObjectItem(element, "Battery"))
		{
			if (level.has_feature(FeatureLevel::Batteries))
			{
				AI::Team team;
				if (session.type == SessionType::Story)
				{
					if ((save.zones[level.id] == ZoneState::Friendly || save.zones[level.id] == ZoneState::GroupOwned) // defending; enemy might have already captured some batteries
						&& mersenne::randf_cc() < 0.2f) // only some batteries though, not all
						team = team_lookup(level.team_lookup, Json::get_s32(element, "team", 1));
					else
						team = AI::TeamNone;
				}
				else
					team = AI::TeamNone;

				entity = World::alloc<BatteryEntity>(absolute_pos, team);

				absolute_rot = Quat::identity;

				RopeEntry* rope = ropes.add();
				rope->pos = absolute_pos + Vec3(0, 1, 0);
				rope->rot = Quat::identity;
				rope->slack = 0.0f;
				rope->max_distance = 100.0f;
				rope->attach_end = true;

				cJSON* links = cJSON_GetObjectItem(element, "links");
				vi_assert(links && cJSON_GetArraySize(links) == 1);
				const char* spawn_point_name = links->child->valuestring;

				LevelLink<SpawnPoint>* spawn_link = spawn_links.add();
				spawn_link->ref = &entity->get<Battery>()->spawn_point;
				spawn_link->target_name = spawn_point_name;
			}
		}
		else if (cJSON_HasObjectItem(element, "AICue"))
		{
			const char* type = Json::get_string(element, "AICue");
			AICue::Type t;
			if (strcmp(type, "snipe") == 0)
				t = AICue::Type::Snipe;
			else
				t = AICue::Type::Sensor;
			entity = World::alloc<Empty>();
			entity->create<AICue>(t);
		}
		else if (cJSON_HasObjectItem(element, "SkyDecal"))
		{
			entity = World::alloc<Empty>();

			SkyDecal* decal = entity->create<SkyDecal>();
			decal->color = Vec4(Json::get_r32(element, "r", 1.0f), Json::get_r32(element, "g", 1.0f), Json::get_r32(element, "b", 1.0f), Json::get_r32(element, "a", 1.0f));
			decal->scale = Json::get_r32(element, "scale", 1.0f);
			decal->texture = Loader::find(Json::get_string(element, "SkyDecal"), AssetLookup::Texture::names);
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
		}
		else if (cJSON_HasObjectItem(element, "Prop"))
		{
			const char* name = Json::get_string(element, "Prop");

			b8 alpha = (b8)Json::get_s32(element, "alpha");
			b8 additive = (b8)Json::get_s32(element, "additive");
			r32 scale = Json::get_r32(element, "scale", 1.0f);
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
					entity->get<View>()->offset.scale(Vec3(scale));
				}
				else
				{
					// SkinnedModel
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
					entity->get<SkinnedModel>()->offset.scale(Vec3(scale));
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
					m->get<View>()->offset.scale(Vec3(scale));

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
			if (session.type == SessionType::Story)
			{
				b8 already_collected = false;
				ID id = ID(transforms.length);
				for (s32 i = 0; i < save.collectibles.length; i++)
				{
					const Net::Master::CollectibleEntry& entry = save.collectibles[i];
					if (entry.zone == level.id && entry.id == id)
					{
						already_collected = true;
						break;
					}
				}
				if (!already_collected)
				{
					const char* type_str = Json::get_string(element, "Collectible");
					Resource type;
					if (strcmp(type_str, "HackKits") == 0)
						type = Resource::HackKits;
					else if (strcmp(type_str, "Drones") == 0)
						type = Resource::Drones;
					else
						type = Resource::Energy;
					entity = World::alloc<CollectibleEntity>(id, type, s16(Json::get_s32(element, "amount")));
				}
			}
		}
		else if (cJSON_HasObjectItem(element, "Shop"))
		{
			if (session.type == SessionType::Story)
			{
				entity = World::alloc<ShopEntity>();

				{
					Entity* i = World::alloc<ShopInteractable>();
					i->get<Transform>()->parent = entity->get<Transform>();
					i->get<Transform>()->pos = Vec3(-3.0f, 0, 0);
					World::awake(i);
				}

				{
					Entity* locke = World::create<Prop>(Asset::Mesh::locke, Asset::Armature::locke);
					locke->get<Transform>()->pos = absolute_pos + absolute_rot * Vec3(-1.25f, 0, 0);
					locke->get<Transform>()->rot = absolute_rot;
					locke->add<PlayerTrigger>()->radius = 5.0f;
					locke->get<Animator>()->layers[0].behavior = Animator::Behavior::Freeze;
					level.finder.add("locke", locke);
					level.scripts.add(Script::find("locke"));
				}
			}
			else
			{
				entity = World::alloc<StaticGeom>(Asset::Mesh::shop, absolute_pos, absolute_rot, CollisionInaccessible, ~CollisionParkour & ~CollisionInaccessible & ~CollisionElectric);
				entity->get<View>()->color.w = MATERIAL_INACCESSIBLE;
			}
		}
		else if (cJSON_HasObjectItem(element, "Empty") || cJSON_HasObjectItem(element, "Camera"))
			entity = World::alloc<Empty>();
		else if (strcmp(Json::get_string(element, "name"), "terminal") == 0)
		{
			if (session.type == SessionType::Story)
			{
				entity = World::alloc<TerminalEntity>();
				level.terminal = entity;

				Entity* i = World::alloc<TerminalInteractable>();
				i->get<Transform>()->parent = entity->get<Transform>();
				i->get<Transform>()->pos = Vec3(1.0f, 0, 0);
				World::awake(i);
				level.terminal_interactable = i;
			}
			else
			{
				entity = World::alloc<StaticGeom>(Asset::Mesh::terminal_collision, absolute_pos, absolute_rot, CollisionInaccessible, ~CollisionParkour & ~CollisionInaccessible & ~CollisionElectric);
				entity->get<View>()->color.w = MATERIAL_INACCESSIBLE;
			}
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

	{
		Entity* map_view = level.finder.find("map_view");
		if (map_view && map_view->has<Transform>())
			level.map_view = map_view->get<Transform>();
	}

	for (s32 i = 0; i < level.finder.map.length; i++)
		World::awake(level.finder.map[i].entity.ref());

	Physics::sync_static();

	for (s32 i = 0; i < ropes.length; i++)
		Rope::spawn(ropes[i].pos, ropes[i].rot * Vec3(0, 1, 0), ropes[i].max_distance, ropes[i].slack, ropes[i].attach_end);

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
	for (s32 i = 0; i < level.scripts.length; i++)
		Script::list[level.scripts[i]].function(level.finder);

	Team::awake_all();
}


}
