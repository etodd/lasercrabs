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
#include "awk.h"
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
#include "data/behavior.h"
#include "render/particles.h"
#include "ai_player.h"
#include "usernames.h"
#include "utf8/utf8.h"
#include "cora.h"
#include "net.h"
#include "parkour.h"

#if DEBUG
	#define DEBUG_NAV_MESH 0
	#define DEBUG_AI_PATH 0
	#define DEBUG_AWK_NAV_MESH 0
	#define DEBUG_AWK_AI_PATH 0
	#define DEBUG_PHYSICS 0
#endif

#include "game.h"

namespace VI
{

b8 Game::quit = false;
b8 Game::is_gamepad = false;
GameTime Game::time;
GameTime Game::real_time;
r32 Game::physics_timestep;

AssetID Game::scheduled_load_level = AssetNull;
Game::Mode Game::scheduled_mode = Game::Mode::Pvp;
Game::Save Game::save = Game::Save();
Game::Level Game::level;
Game::Session Game::session;
b8 Game::cancel_event_eaten[] = {};

Game::Session::Session()
	:
#if SERVER
	local_player_config{ AI::TeamNone, AI::TeamNone, AI::TeamNone, AI::TeamNone },
#else
	local_player_config{ 0, AI::TeamNone, AI::TeamNone, AI::TeamNone },
#endif
	story_mode(true),
	time_scale(1.0f),
	network_timer(),
	network_time(),
	network_state(),
	network_quality(),
	last_match()
{
	for (s32 i = 0; i < MAX_PLAYERS; i++)
		local_player_uuids[i] = mersenne::rand_u64();
}

r32 Game::Session::effective_time_scale() const
{
	switch (network_state)
	{
		case NetworkState::Lag:
		{
			return 0.0f;
		}
		case NetworkState::Recover:
		{
			return time_scale * 3.0f;
		}
		default:
		{
			return time_scale;
		}
	}
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

s32 Game::Session::team_count() const
{
	if (story_mode)
		return 2;
	else
	{
#if SERVER
		return Net::Server::expected_clients();
#else
		s32 team_counts[MAX_PLAYERS] = {};
		for (s32 i = 0; i < MAX_GAMEPADS; i++)
		{
			if (local_player_config[i] != AI::TeamNone)
				team_counts[local_player_config[i]]++;
		}

		s32 count = 0;
		for (s32 i = 0; i < MAX_PLAYERS; i++)
		{
			if (team_counts[i] > 0)
				count++;
		}
		return count;
#endif
	}
}

void Game::Session::reset()
{
	*this = Session();
}

Game::Save::Save()
	: zones(),
	story_index(),
	variables(),
	resources(),
	messages(),
	messages_scheduled(),
	username("etodd"),
	group(),
	cora_called()
{
}

b8 Game::Level::has_feature(Game::FeatureLevel f) const
{
	return (s32)feature_level >= (s32)f;
}

Array<UpdateFunction> Game::updates;
Array<DrawFunction> Game::draws;
Array<CleanupFunction> Game::cleanups;

b8 Game::init(LoopSync* sync)
{
	World::init();

	if (!Net::init())
		return false;

#if !SERVER
	if (!Audio::init())
		return false;

	Loader::font_permanent(Asset::Font::lowpoly);

	if (!Loader::soundbank_permanent(Asset::Soundbank::Init))
		return false;
	if (!Loader::soundbank_permanent(Asset::Soundbank::SOUNDBANK))
		return false;

	// strings
	{
		const char* language_file = "language.txt";
		cJSON* json_language = Json::load(language_file);
		const char* language = Json::get_string(json_language, "language", "en");
		char ui_string_file[255];
		sprintf(ui_string_file, "assets/str/ui_%s.json", language);
		char dialogue_string_file[255];
		sprintf(dialogue_string_file, "assets/str/dialogue_%s.json", language);
		char misc_file[255];
		sprintf(misc_file, "assets/str/misc_%s.json", language);

		// UI
		{
			cJSON* json = Json::load(ui_string_file);
			for (s32 i = 0; i < Asset::String::count; i++)
			{
				const char* name = AssetLookup::String::names[i];
				cJSON* value = cJSON_GetObjectItem(json, name);
				strings_set(i, value ? value->valuestring : nullptr);
			}
		}

		// dialogue strings
		{
			cJSON* json = Json::load(dialogue_string_file);
			cJSON* element = json->child;
			while (element)
			{
				strings_add_dynamic(element->string, element->valuestring);
				element = element->next;
			}
		}

		// misc strings
		{
			cJSON* json = Json::load(misc_file);
			cJSON* element = json->child;
			while (element)
			{
				strings_add_dynamic(element->string, element->valuestring);
				element = element->next;
			}
		}

		Input::load_strings(); // loads localized strings for input bindings

		// don't free the JSON objects; we'll read strings directly from them
	}

	Cora::global_init();

	Menu::init();

	for (s32 i = 0; i < ParticleSystem::all.length; i++)
		ParticleSystem::all[i]->init(sync);

	UI::init(sync);

	Console::init();
#endif

	return true;
}

void Game::update(const Update& update_in)
{
	b8 update_game;
#if SERVER
	update_game = Net::Server::mode() == Net::Server::Mode::Active;
#else
	update_game = level.local || Net::Client::mode() == Net::Client::Mode::Connected;
#endif

	real_time = update_in.time;
	time.delta = update_in.time.delta * session.effective_time_scale();
	if (update_game)
		time.total += time.delta;
	physics_timestep = (1.0f / 60.0f) * session.effective_time_scale();

	Update u = update_in;
	u.time = time;

	// lag simulation
	if (level.mode == Mode::Pvp && level.local && session.story_mode && session.network_quality != NetworkQuality::Perfect)
	{
		session.network_timer -= real_time.delta;
		if (session.network_timer < 0.0f)
		{
			switch (session.network_state)
			{
				case NetworkState::Normal:
				{
					session.network_state = NetworkState::Lag;
					if (session.network_quality == NetworkQuality::Bad)
						session.network_time = 0.2f + mersenne::randf_cc() * (mersenne::randf_cc() < 0.1f ? 4.0f : 0.3f);
					else
						session.network_time = 0.05f + mersenne::randf_cc() * 1.0f;
					break;
				}
				case NetworkState::Lag:
				{
					session.network_state = NetworkState::Recover;
					if (session.network_time > 0.5f)
						session.network_time = session.network_time * 0.5f + mersenne::randf_cc() * 0.3f; // recovery time is proportional to lag time
					else
						session.network_time = 0.05f + mersenne::randf_cc() * 0.2f;
					break;
				}
				case NetworkState::Recover:
				{
					session.network_state = NetworkState::Normal;
					if (mersenne::randf_cc() < (session.network_quality == NetworkQuality::Bad ? 0.5f : 0.2f))
						session.network_time = 0.05f + mersenne::randf_cc() * 0.15f; // go right back into lag state
					else
					{
						// some time before next lag
						if (session.network_quality == NetworkQuality::Bad)
							session.network_time = 2.0f + mersenne::randf_cc() * 8.0f;
						else
							session.network_time = 20.0f + mersenne::randf_cc() * 50.0f;
					}
					break;
				}
			}
			session.network_timer = session.network_time;
		}
		else
		{
			if (session.network_state == NetworkState::Lag && session.network_time - session.network_timer > 3.5f)
				Team::transition_next(MatchResult::NetworkError); // disconnect
		}
	}

	Net::update_start(u);

#if !SERVER
	// determine whether to display gamepad or keyboard bindings
	{
		const Gamepad& gamepad = update_in.input->gamepads[0];
		s32 gamepad_count = 0;
		for (s32 i = 0; i < MAX_GAMEPADS; i++)
		{
			if (update_in.input->gamepads[i].active)
				gamepad_count++;
		}
		b8 refresh = false;
		if (is_gamepad)
		{
			// check if we need to clear the gamepad flag
			if (gamepad_count <= 1
				&& (!gamepad.active || update_in.input->cursor_x != 0 || update_in.input->cursor_y != 0))
			{
				is_gamepad = false;
				refresh = true;
			}
		}
		else
		{
			// check if we need to set the gamepad flag
			if (gamepad_count > 1)
			{
				is_gamepad = true;
				refresh = true;
			}
			else if (gamepad.active)
			{
				if (gamepad.btns)
				{
					is_gamepad = true;
					refresh = true;
				}
				else
				{
					Vec2 left(gamepad.left_x, gamepad.left_y);
					Input::dead_zone(&left.x, &left.y);
					if (left.length_squared() > 0.0f)
					{
						is_gamepad = true;
						refresh = true;
					}
					else
					{
						Vec2 right(gamepad.right_x, gamepad.right_y);
						Input::dead_zone(&right.x, &right.y);
						if (right.length_squared() > 0.0f)
						{
							is_gamepad = true;
							refresh = true;
						}
					}
				}
			}
		}

		if (refresh)
			Menu::refresh_variables();
	}

	Menu::update(u);
	Terminal::update(u);
#endif

	if (scheduled_load_level != AssetNull)
		load_level(u, scheduled_load_level, scheduled_mode);

	AI::update(u);

	if (update_game)
	{
		Physics::sync_dynamic();

		for (auto i = Ragdoll::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
		for (auto i = Animator::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);

		LerpTo<Vec3>::update_active(u);
		Delay::update_active(u);

		Physics::sync_static();

		AI::update(u);

		Team::update_all(u);
		PlayerManager::update_all(u);
		PlayerHuman::update_all(u);
		for (auto i = Health::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
		for (auto i = PlayerAI::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
		for (auto i = Awk::list.iterator(); !i.is_last(); i.next())
		{
			if (level.local || (i.item()->has<PlayerControlHuman>() && i.item()->get<PlayerControlHuman>()->local()))
				i.item()->update_server(u);
			i.item()->update_client(u);
		}
		for (auto i = PlayerControlAI::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
		for (auto i = PlayerTrigger::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
		MinionAI::update_all(u);
		for (auto i = MinionCommon::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
		for (auto i = Walker::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
		EnergyPickup::update_all(u);
		Sensor::update_all(u);
		ContainmentField::update_all(u);
		for (auto i = ControlPoint::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
		for (auto i = Shockwave::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
		for (auto i = Projectile::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
		for (auto i = Rocket::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
		for (auto i = Parkour::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
		for (auto i = PlayerCommon::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
		for (auto i = PlayerControlHuman::list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);

		for (s32 i = 0; i < updates.length; i++)
			(*updates[i])(u);
	}

	Console::update(u);

	Audio::update();

	World::flush();

	// reset cancel event eaten flags
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		if (!u.input->get(Controls::Cancel, i) && !u.last_input->get(Controls::Cancel, i))
			cancel_event_eaten[i] = false;
	}

	Net::update_end(u);
}

void Game::term()
{
	Net::term();
	Audio::term();
}

#if SERVER

void Game::draw_opaque(const RenderParams& render_params)
{
}

void Game::draw_override(const RenderParams& params)
{
}

void Game::draw_alpha(const RenderParams& params)
{
}

void Game::draw_alpha_depth(const RenderParams& render_params)
{
}

void Game::draw_additive(const RenderParams& render_params)
{
}

#else

// client

void Game::draw_opaque(const RenderParams& render_params)
{
	for (auto i = PlayerControlHuman::list.iterator(); !i.is_last(); i.next())
		i.item()->draw(render_params);

	View::draw_opaque(render_params);

	for (auto i = Water::list.iterator(); !i.is_last(); i.next())
		i.item()->draw_opaque(render_params);

	Rope::draw_opaque(render_params);
	SkinnedModel::draw_opaque(render_params);

	SkyPattern::draw_opaque(render_params);
}

void Game::draw_override(const RenderParams& params)
{
	Terminal::draw_override(params);
}

void Game::draw_alpha(const RenderParams& render_params)
{
	for (auto i = Water::list.iterator(); !i.is_last(); i.next())
		i.item()->draw_alpha(render_params);

	if (render_params.camera->colors)
		Skybox::draw_alpha(render_params);
	SkyDecal::draw_alpha(render_params);
	SkyPattern::draw_alpha(render_params);

#if DEBUG_NAV_MESH
	AI::debug_draw_nav_mesh(render_params);
#endif

#if DEBUG_AWK_NAV_MESH
	AI::debug_draw_awk_nav_mesh(render_params);
#endif

#if DEBUG_AI_PATH
	{
		UIText text;
		text.color = UI::color_accent;
		for (auto i = MinionAI::list.iterator(); !i.is_last(); i.next())
		{
			MinionAI* minion = i.item();
			for (s32 j = minion->path_index; j < minion->path.length; j++)
			{
				Vec2 p;
				if (UI::project(render_params, minion->path[j], &p))
				{
					text.text("%d", j);
					text.draw(render_params, p);
				}
			}
		}
	}
#endif

#if DEBUG_AWK_AI_PATH
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
					text.text("%d", j);
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

		for (auto i = MinionCommon::list.iterator(); !i.is_last(); i.next())
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

	for (s32 i = 0; i < ParticleSystem::all.length; i++)
		ParticleSystem::all[i]->draw(render_params);

	for (auto i = PlayerControlHuman::list.iterator(); !i.is_last(); i.next())
		i.item()->draw_alpha(render_params);
	for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
		i.item()->draw_alpha(render_params);

	Menu::draw(render_params);
	Terminal::draw(render_params);

	for (s32 i = 0; i < draws.length; i++)
		(*draws[i])(render_params);

	Console::draw(render_params);
}

void Game::draw_alpha_depth(const RenderParams& render_params)
{
	SkinnedModel::draw_alpha_depth(render_params);
	View::draw_alpha_depth(render_params);
}

void Game::draw_additive(const RenderParams& render_params)
{
	View::draw_additive(render_params);
	SkinnedModel::draw_additive(render_params);
}

#endif

void Game::execute(const Update& u, const char* cmd)
{
	if (utf8cmp(cmd, "killai") == 0)
	{
		for (auto i = PlayerControlAI::list.iterator(); !i.is_last(); i.next())
		{
			Health* health = i.item()->get<Health>();
			health->damage(nullptr, health->hp_max + health->shield_max);
		}
	}
	else if (utf8cmp(cmd, "die") == 0)
	{
		for (auto i = PlayerControlHuman::list.iterator(); !i.is_last(); i.next())
		{
			Health* health = i.item()->get<Health>();
			health->damage(nullptr, health->hp_max + health->shield_max);
		}
	}
	else if (utf8cmp(cmd, "noclip") == 0)
	{
		level.continue_match_after_death = true;
		for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
		{
			Entity* entity = i.item()->get<PlayerManager>()->entity.ref();
			if (entity)
				World::remove(entity);
		}
	}
	else if (strstr(cmd, "timescale ") == cmd)
	{
		const char* delimiter = strchr(cmd, ' ');
		if (delimiter)
		{
			const char* number_string = delimiter + 1;
			char* end;
			r32 value = std::strtod(number_string, &end);
			if (*end == '\0')
				session.time_scale = value;
		}
	}
	else if (strstr(cmd, "energy ") == cmd)
	{
		const char* delimiter = strchr(cmd, ' ');
		if (delimiter)
		{
			const char* number_string = delimiter + 1;
			char* end;
			s32 value = (s32)std::strtol(number_string, &end, 10);
			if (*end == '\0')
			{
				if (level.id == Asset::Level::terminal)
					Game::save.resources[(s32)Game::Resource::Energy] += value;
				else if (PlayerManager::list.count() > 0)
				{
					for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
						i.item()->credits += value;
				}
			}
		}
	}
	else if (strstr(cmd, "unlock") == cmd)
	{
		for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->entity.ref())
			{
				s16 credits = i.item()->credits;
				i.item()->credits = 10000;
				for (s32 upgrade = 0; upgrade < (s32)Upgrade::count; upgrade++)
				{
					while (i.item()->upgrade_available((Upgrade)upgrade))
					{
						i.item()->upgrade_start((Upgrade)upgrade);
						i.item()->upgrade_complete();
					}
				}
				i.item()->credits = credits;
			}
		}
	}
	else if (strstr(cmd, "terminal") == cmd)
		Terminal::show();
	else if (strstr(cmd, "loadai ") == cmd)
	{
		// AI test
		const char* delimiter = strchr(cmd, ' ');
		if (delimiter)
		{
			const char* level_name = delimiter + 1;
			AssetID level = Loader::find_level(level_name);
			if (level != AssetNull)
			{
				save = Save();
				load_level(u, level, Mode::Pvp, true);
			}
		}
	}
	else if (strstr(cmd, "load ") == cmd)
	{
		// pvp mode
		const char* delimiter = strchr(cmd, ' ');
		if (delimiter)
		{
			const char* level_name = delimiter + 1;
			AssetID level = Loader::find_level(level_name);
			if (level != AssetNull)
			{
				save = Save();
				schedule_load_level(level, Mode::Pvp);
			}
		}
	}
	else if (strcmp(cmd, "netstat") == 0)
	{
		Net::show_stats = !Net::show_stats;
	}
	else if (strstr(cmd, "loadp ") == cmd)
	{
		// parkour mode
		const char* delimiter = strchr(cmd, ' ');
		if (delimiter)
		{
			const char* level_name = delimiter + 1;
			AssetID level = Loader::find_level(level_name);
			if (level != AssetNull)
			{
				save = Save();
				schedule_load_level(level, Mode::Parkour);
			}
		}
	}
	else if (strstr(cmd, "conn") == cmd)
	{
#if !SERVER
		// connect to server
		const char* delimiter = strchr(cmd, ' ');
		const char* host;
		if (delimiter)
			host = delimiter + 1;
		else
			host = "127.0.0.1";

		Game::save = Game::Save();
		Game::session.reset();
		Game::session.story_mode = false;
		Game::unload_level();
		Net::Client::connect(host, 3494);
#endif
	}
	else if (level.id == Asset::Level::terminal)
		Terminal::execute(cmd);
}

void Game::schedule_load_level(AssetID level_id, Mode m)
{
	scheduled_load_level = level_id;
	scheduled_mode = m;
}

void Game::unload_level()
{
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
		Audio::listener_disable(i);

	World::clear(); // deletes all entities

	// PlayerAI is not part of the entity system
	PlayerAI::list.clear();

	for (s32 i = 0; i < ParticleSystem::all.length; i++)
		ParticleSystem::all[i]->clear();

	Loader::transients_free();
	updates.length = 0;
	draws.length = 0;
	for (s32 i = 0; i < cleanups.length; i++)
		(*cleanups[i])();
	cleanups.length = 0;

	level.skybox.far_plane = 1.0f;
	level.skybox.color = Vec3::zero;
	level.skybox.ambient_color = Vec3::zero;
	level.skybox.shader = AssetNull;
	level.skybox.texture = AssetNull;
	level.skybox.mesh = AssetNull;
	level.skybox.player_light = Vec3::zero;

	Audio::post_global_event(AK::EVENTS::STOP_ALL);
	level.id = AssetNull;
	Menu::clear();

	for (s32 i = 0; i < Camera::max_cameras; i++)
	{
		if (Camera::list[i].active)
			Camera::list[i].remove();
	}

	Net::reset();
}

Entity* EntityFinder::find(const char* name) const
{
	for (s32 j = 0; j < map.length; j++)
	{
		if (utf8cmp(map[j].name, name) == 0)
			return map[j].entity.ref();
	}
	return nullptr;
}

template<typename T>
struct LevelLink
{
	Ref<T>* ref;
	const char* target_name;
};

struct RopeEntry
{
	Vec3 pos;
	Quat rot;
	r32 max_distance;
	r32 slack;
};

AI::Team team_lookup(const AI::Team* table, s32 i)
{
	return table[vi_max(0, vi_min(MAX_PLAYERS, i))];
}

void Game::load_level(const Update& u, AssetID l, Mode m, b8 ai_test)
{
	time.total = 0.0f;

	Menu::clear();

	level.mode = m;
	level.local = true;

	session.network_state = NetworkState::Normal;
	session.network_timer = session.network_time = 0.0f;

	if (level.mode == Mode::Pvp && level.local && session.story_mode)
	{
		// choose network quality
		r32 random = mersenne::randf_cc();
		if (random < 0.95f)
			session.network_quality = NetworkQuality::Perfect;
		else if (random < 0.99f)
			session.network_quality = NetworkQuality::Okay;
		else
			session.network_quality = NetworkQuality::Bad;
	}
	else
		session.network_quality = NetworkQuality::Perfect;

	scheduled_load_level = AssetNull;

	unload_level();

	Audio::post_global_event(AK::EVENTS::PLAY_START_SESSION);

	Physics::btWorld->setGravity(btVector3(0, -12.0f, 0));

	Array<Transform*> transforms;

	Array<RopeEntry> ropes;

	Array<Script*> scripts;

	Array<LevelLink<Entity>> links;
	Array<LevelLink<Transform>> transform_links;

	EntityFinder finder;

	// material override colors
	AI::Team teams[MAX_PLAYERS];

	cJSON* json = Loader::level(l);

	level = Level();
	level.mode = m;
	level.id = l;

	// count control point sets and pick one of them
	s32 control_point_set = 0;
	if (level.type == Type::Rush)
	{
		s32 max_control_point_set = 0;
		cJSON* element = json->child;
		while (element)
		{
			if (cJSON_GetObjectItem(element, "ControlPoint"))
				max_control_point_set = vi_max(max_control_point_set, Json::get_s32(element, "set", -1));
			element = element->next;
		}

		// pick a set of control points
		if (max_control_point_set > 0)
			control_point_set = mersenne::rand() % (max_control_point_set + 1);
		else if (level.type == Type::Rush) // no control points
			level.type = Type::Deathmatch;
	}

	switch (level.type)
	{
		case Type::Rush:
		{
			level.respawns = 5;
			level.kill_limit = 0;
			level.time_limit = (60.0f * 5.0f) + PLAYER_SPAWN_DELAY;
			break;
		}
		case Type::Deathmatch:
		{
			level.respawns = -1;
			level.kill_limit = 10;
			level.time_limit = (60.0f * 10.0f) + PLAYER_SPAWN_DELAY;
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}

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

		if (cJSON_GetObjectItem(element, "World"))
		{
			// World is guaranteed to be the first element in the entity list

			level.feature_level = (FeatureLevel)Json::get_s32(element, "feature_level", (s32)FeatureLevel::All);
			level.lock_teams = Json::get_s32(element, "lock_teams");

			s32 team_count = session.team_count();

			{
				// shuffle teams and make sure they're packed in the array starting at 0
				s32 offset = level.lock_teams ? 0 : mersenne::rand() % team_count;
				for (s32 i = 0; i < MAX_PLAYERS; i++)
					teams[i] = (AI::Team)((offset + i) % team_count);
			}

			level.skybox.far_plane = Json::get_r32(element, "far_plane", 100.0f);
			level.skybox.texture = Loader::find(Json::get_string(element, "skybox_texture"), AssetLookup::Texture::names);
			level.skybox.shader = Asset::Shader::skybox;
			level.skybox.mesh = Asset::Mesh::skybox;
			level.skybox.color = Json::get_vec3(element, "skybox_color");
			level.skybox.ambient_color = Json::get_vec3(element, "ambient_color");
			level.skybox.player_light = Json::get_vec3(element, "zenith_color");

			level.min_y = Json::get_r32(element, "min_y", -20.0f);

			// initialize teams
			if (m != Mode::Special)
			{
				for (s32 i = 0; i < team_count; i++)
				{
					Entity* e = World::alloc<ContainerEntity>(); // team entities get awoken and finalized at the end of load_level()
					e->create<Team>();
				}

				for (s32 i = 0; i < MAX_GAMEPADS; i++)
				{
					if (session.local_player_config[i] != AI::TeamNone)
					{
						AI::Team team = team_lookup(teams, (s32)session.local_player_config[i]);

						Entity* e = World::create<ContainerEntity>();
						PlayerManager* manager = e->add<PlayerManager>(&Team::list[(s32)team]);

						if (ai_test)
						{
							PlayerAI* player = PlayerAI::list.add();
							new (player) PlayerAI(manager, PlayerAI::generate_config());
							utf8cpy(manager->username, Usernames::all[mersenne::rand_u32() % Usernames::count]);
						}
						else
						{
							e->add<PlayerHuman>(true, i); // local = true

							if (level.local && !session.story_mode)
								sprintf(manager->username, _(strings::player), i + 1);
							else
							{
								if (i == 0)
									sprintf(manager->username, "%s", save.username);
								else
									sprintf(manager->username, "%s [%d]", save.username, i + 1);
							}
						}
						Net::finalize(e);
					}
				}
			}
		}
		else if (Json::get_s32(element, "min_players") > PlayerManager::list.count()
			|| Json::get_s32(element, "min_teams") > Team::list.count())
		{
			// not enough players or teams
			// don't spawn the entity
		}
		else if (cJSON_GetObjectItem(element, "StaticGeom"))
		{
			b8 alpha = (b8)Json::get_s32(element, "alpha");
			b8 additive = (b8)Json::get_s32(element, "additive");
			b8 no_parkour = cJSON_HasObjectItem(element, "no_parkour");
			AssetID texture = (AssetID)Loader::find(Json::get_string(element, "texture"), AssetLookup::Texture::names);

			cJSON* meshes = cJSON_GetObjectItem(element, "meshes");
			cJSON* json_mesh = meshes->child;

			while (json_mesh)
			{
				char* mesh_ref = json_mesh->valuestring;

				AssetID mesh_id = Loader::find_mesh(mesh_ref);

				if (mesh_id != AssetNull)
				{
					Entity* m;
					const Mesh* mesh = Loader::mesh(mesh_id);
					if (mesh->color.w < 0.5f && !(alpha || additive))
					{
						// inaccessible
						if (no_parkour) // no parkour material
							m = World::alloc<StaticGeom>(mesh_id, absolute_pos, absolute_rot, CollisionInaccessible, ~CollisionParkour & ~CollisionInaccessibleMask);
						else
							m = World::alloc<StaticGeom>(mesh_id, absolute_pos, absolute_rot, CollisionParkour | CollisionInaccessible, ~CollisionParkour & ~CollisionInaccessibleMask);
						m->get<View>()->color.w = MATERIAL_NO_OVERRIDE;
					}
					else
					{
						// accessible
						if (no_parkour) // no parkour material
							m = World::alloc<StaticGeom>(mesh_id, absolute_pos, absolute_rot, 0, ~CollisionParkour & ~CollisionInaccessible);
						else
							m = World::alloc<StaticGeom>(mesh_id, absolute_pos, absolute_rot, CollisionParkour, ~CollisionParkour);
					}

					m->get<View>()->texture = texture;
					if (alpha || additive)
						m->get<View>()->shader = texture == AssetNull ? Asset::Shader::flat : Asset::Shader::flat_texture;
					if (alpha)
						m->get<View>()->alpha();
					if (additive)
						m->get<View>()->additive();

					if (entity)
					{
						World::awake(m);
						m->get<Transform>()->reparent(entity->get<Transform>());
						Net::finalize(m);
					}
					else
						entity = m;
				}

				json_mesh = json_mesh->next;
			}
		}
		else if (cJSON_GetObjectItem(element, "Rope"))
		{
			RopeEntry* rope = ropes.add();
			rope->pos = absolute_pos;
			rope->rot = absolute_rot;
			rope->slack = Json::get_r32(element, "slack");
			rope->max_distance = Json::get_r32(element, "max_distance", 100.0f);
		}
		else if (cJSON_GetObjectItem(element, "Minion"))
		{
			AI::Team team = (AI::Team)Json::get_s32(element, "team");
			entity = World::alloc<Minion>(absolute_pos, absolute_rot, team);
			s32 health = Json::get_s32(element, "health");
			if (health)
				entity->get<Health>()->hp = health;
		}
		else if (cJSON_GetObjectItem(element, "PlayerSpawn"))
		{
			AI::Team team = (AI::Team)Json::get_s32(element, "team");

			if (Team::list.count() > (s32)team)
			{
				entity = World::alloc<PlayerSpawnEntity>(team);
				Team::list[(s32)team].player_spawn = entity->get<Transform>();
			}
			else
				entity = World::alloc<PlayerSpawnEntity>(AI::TeamNone);
		}
		else if (cJSON_GetObjectItem(element, "ControlPoint"))
		{
			if (level.type == Type::Rush && (!cJSON_GetObjectItem(element, "set") || Json::get_s32(element, "set") == control_point_set))
			{
				absolute_pos.y += 1.5f;
				entity = World::alloc<ControlPointEntity>(AI::Team(0), absolute_pos);
			}
		}
		else if (cJSON_GetObjectItem(element, "PlayerTrigger"))
		{
			entity = World::alloc<Empty>();
			PlayerTrigger* trigger = entity->create<PlayerTrigger>();
			trigger->radius = Json::get_r32(element, "scale", 1.0f);
		}
		else if (cJSON_GetObjectItem(element, "PointLight"))
		{
			entity = World::alloc<Empty>();
			PointLight* light = entity->create<PointLight>();
			light->color = Json::get_vec3(element, "color");
			light->radius = Json::get_r32(element, "radius");
		}
		else if (cJSON_GetObjectItem(element, "SpotLight"))
		{
			absolute_rot = absolute_rot * Quat::euler(0, 0, PI * 0.5f);
			entity = World::alloc<Empty>();
			SpotLight* light = entity->create<SpotLight>();
			light->color = Json::get_vec3(element, "color");
			light->radius = Json::get_r32(element, "radius");
			light->fov = Json::get_r32(element, "fov");
		}
		else if (cJSON_GetObjectItem(element, "DirectionalLight"))
		{
			entity = World::alloc<Empty>();
			DirectionalLight* light = entity->create<DirectionalLight>();
			light->color = Json::get_vec3(element, "color");
			light->shadowed = Json::get_s32(element, "shadowed");
		}
		else if (cJSON_GetObjectItem(element, "AIPlayer"))
		{
			// only add an AI player if we are in online pvp mode
			if (level.mode == Mode::Pvp && session.story_mode)
			{
				AI::Team team = team_lookup(teams, Json::get_s32(element, "team", 1));

				Entity* e = World::create<ContainerEntity>();
				PlayerManager* manager = e->add<PlayerManager>(&Team::list[(s32)team]);
				utf8cpy(manager->username, Usernames::all[mersenne::rand_u32() % Usernames::count]);
				Net::finalize(e);

				PlayerAI* player = PlayerAI::list.add();
				new (player) PlayerAI(manager, PlayerAI::generate_config());
			}
		}
		else if (cJSON_GetObjectItem(element, "EnergyPickup"))
		{
			if (level.has_feature(FeatureLevel::EnergyPickups))
			{
				entity = World::alloc<EnergyPickupEntity>(absolute_pos);
				absolute_rot = Quat::identity;

				RopeEntry* rope = ropes.add();
				rope->pos = absolute_pos + Vec3(0, 1, 0);
				rope->rot = Quat::identity;
				rope->slack = 0.0f;
				rope->max_distance = 100.0f;
			}
		}
		else if (cJSON_GetObjectItem(element, "AICue"))
		{
			const char* type = Json::get_string(element, "AICue");
			AICue::Type t;
			if (strcmp(type, "rocket") == 0)
				t = AICue::Type::Rocket;
			else if (strcmp(type, "snipe") == 0)
				t = AICue::Type::Snipe;
			else
				t = AICue::Type::Sensor;
			entity = World::alloc<Empty>();
			entity->create<AICue>(t);
		}
		else if (cJSON_GetObjectItem(element, "SkyDecal"))
		{
			entity = World::alloc<Empty>();

			SkyDecal* decal = entity->create<SkyDecal>();
			decal->color = Vec4(Json::get_r32(element, "r", 1.0f), Json::get_r32(element, "g", 1.0f), Json::get_r32(element, "b", 1.0f), Json::get_r32(element, "a", 1.0f));
			decal->scale = Json::get_r32(element, "scale", 1.0f);
			decal->texture = Loader::find(Json::get_string(element, "SkyDecal"), AssetLookup::Texture::names);
		}
		else if (cJSON_GetObjectItem(element, "Script"))
		{
			const char* name = Json::get_string(element, "Script");
			Script* script = Script::find(name);
			vi_assert(script);
			scripts.add(script);
		}
		else if (cJSON_GetObjectItem(element, "Water"))
		{
			cJSON* meshes = cJSON_GetObjectItem(element, "meshes");

			vi_assert(meshes);
			cJSON* mesh_json = meshes->child;
			vi_assert(mesh_json);

			char* mesh_ref = mesh_json->valuestring;

			AssetID mesh_id = Loader::find_mesh(mesh_ref);
			vi_assert(mesh_id != AssetNull);

			const Mesh* mesh = Loader::mesh(mesh_id);

			entity = World::alloc<WaterEntity>(mesh_id);
			Water* water = entity->get<Water>();
			water->texture = Loader::find(Json::get_string(element, "texture", "water_normal"), AssetLookup::Texture::names);
			water->displacement_horizontal = Json::get_r32(element, "displacement_horizontal", 2.0f);
			water->displacement_vertical = Json::get_r32(element, "displacement_vertical", 0.75f);
		}
		else if (cJSON_GetObjectItem(element, "Prop"))
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
					shader = alpha || additive ? Asset::Shader::flat : Asset::Shader::standard;
			}

			if (name)
			{
				AssetID mesh_id = Loader::find_mesh(name);
				entity = World::create<Prop>(mesh_id, Loader::find(armature, AssetLookup::Armature::names), Loader::find(animation, AssetLookup::Animation::names));
				// todo: clean this up
				if (entity->has<View>())
				{
					// View
					entity->get<View>()->texture = texture;
					entity->get<View>()->shader = shader;
					if (!alpha && !additive)
					{
						const Mesh* mesh = Loader::mesh(mesh_id);
						if (mesh->color.w < 0.5f)
							entity->get<View>()->color.w = MATERIAL_NO_OVERRIDE;
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
					if (!alpha && !additive)
					{
						const Mesh* mesh = Loader::mesh(mesh_id);
						if (mesh->color.w < 0.5f)
							entity->get<SkinnedModel>()->color.w = MATERIAL_NO_OVERRIDE;
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
					char* mesh_ref = mesh->valuestring;

					AssetID mesh_id = Loader::find_mesh(mesh_ref);

					Entity* m = World::alloc<Prop>(mesh_id);

					m->get<View>()->texture = texture;
					m->get<View>()->shader = shader;
					if (!alpha && !additive)
					{
						const Mesh* mesh = Loader::mesh(mesh_id);
						if (mesh->color.w < 0.5f)
							m->get<View>()->color.w = MATERIAL_NO_OVERRIDE;
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
						Net::finalize(m);
					}
					else
						entity = m;

					mesh = mesh->next;
				}
			}
		}
		else
			entity = World::alloc<Empty>();

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
		{
			EntityFinder::NameEntry* entry = finder.map.add();
			entry->name = Json::get_string(element, "name");
			entry->entity = entity;
			entry->properties = element;

			Net::finalize(entity);
		}

		element = element->next;
	}

	for (s32 i = 0; i < links.length; i++)
	{
		LevelLink<Entity>& link = links[i];
		*link.ref = finder.find(link.target_name);
	}

	for (s32 i = 0; i < transform_links.length; i++)
	{
		LevelLink<Transform>& link = transform_links[i];
		*link.ref = finder.find(link.target_name)->get<Transform>();
	}

	for (s32 i = 0; i < finder.map.length; i++)
		World::awake(finder.map[i].entity.ref());

	Physics::sync_static();

	for (s32 i = 0; i < ropes.length; i++)
		Rope::spawn(ropes[i].pos, ropes[i].rot * Vec3(0, 1, 0), ropes[i].max_distance, ropes[i].slack);

	// Set map view for local players
	{
		Entity* map_view = finder.find("map_view");
		if (map_view && map_view->has<Transform>())
		{
			for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
				i.item()->map_view = map_view->get<Transform>();
		}
	}

	for (s32 i = 0; i < scripts.length; i++)
		scripts[i]->function(u, finder);

	Terminal::init(u, finder);

	Loader::level_free(json);

	Team::awake_all();
	for (auto i = Team::list.iterator(); !i.is_last(); i.next())
		Net::finalize(i.item()->entity());
}

}
