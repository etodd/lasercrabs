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
#include "asset/Wwise_IDs.h"
#include "input.h"
#include "mersenne/mersenne-twister.h"
#include <time.h>
#include "cjson/cJSON.h"
#include "audio.h"
#include "menu.h"
#include "scripts.h"
#include "console.h"
#include "ease.h"
#include "sentinel.h"
#include "data/behavior.h"

#if DEBUG
	#define DEBUG_AI 0
	#define DEBUG_PHYSICS 0
#endif

#include "game.h"

namespace VI
{

Game::Bindings Game::bindings =
{
	{ KeyCode::Return, Gamepad::Btn::Start }, // Start
	{ KeyCode::Space, Gamepad::Btn::A }, // Action
	{ KeyCode::Escape, Gamepad::Btn::B }, // Cancel
};

b8 Game::quit = false;
GameTime Game::time = GameTime();
GameTime Game::real_time = GameTime();

r32 Game::time_scale = 1.0f;
AssetID Game::scheduled_load_level = AssetNull;
#define initial_credits 250
Game::Data Game::data = Game::Data();
Vec2 Game::cursor = Vec2(200, 200);
b8 Game::cursor_updated = false;

Game::Data::Data()
	: level(AssetNull),
	previous_level(AssetNull),
	mode(Mode::Multiplayer),
	third_person()
{
	for (s32 i = 0; i < (s32)AI::Team::count; i++)
		credits[i] = initial_credits;
}

Array<UpdateFunction> Game::updates = Array<UpdateFunction>();
Array<DrawFunction> Game::draws = Array<DrawFunction>();
Array<CleanupFunction> Game::cleanups = Array<CleanupFunction>();

b8 Game::init(LoopSync* sync)
{
	World::init();
	View::init();
	RigidBody::init();

	if (!Audio::init())
		return false;

	if (!Loader::soundbank_permanent(Asset::Soundbank::Init))
		return false;
	if (!Loader::soundbank_permanent(Asset::Soundbank::SOUNDBANK))
		return false;

	UI::init(sync);

	AI::init();

	Console::init();

	Menu::init();

	return true;
}

void Game::update(const Update& update_in)
{
	real_time = update_in.time;
	time.delta = update_in.time.delta * time_scale;
	time.total += time.delta;

	cursor_updated = false;

	Update u = update_in;
	u.time = time;

	Menu::update(u);

	if (scheduled_load_level != AssetNull)
		load_level(u, scheduled_load_level);

	Physics::sync_dynamic();

	for (auto i = Ragdoll::list.iterator(); !i.is_last(); i.next())
		i.item()->update(u);
	for (auto i = Animator::list.iterator(); !i.is_last(); i.next())
		i.item()->update(u);
	for (auto i = Mover::list.iterator(); !i.is_last(); i.next())
		i.item()->update(u);

	Physics::sync_static();

	if (u.input->keys[(s32)KeyCode::K] && !u.last_input->keys[(s32)KeyCode::K])
	{
		for (auto i = SentinelCommon::list.iterator(); !i.is_last(); i.next())
			i.item()->killed(nullptr);
	}

	for (auto i = LocalPlayer::list.iterator(); !i.is_last(); i.next())
		i.item()->update(u);
	for (auto i = AIPlayer::list.iterator(); !i.is_last(); i.next())
		i.item()->update(u);
	for (auto i = Walker::list.iterator(); !i.is_last(); i.next())
		i.item()->update(u);
	for (auto i = Awk::list.iterator(); !i.is_last(); i.next())
		i.item()->update(u);
	PlayerCommon::update_visibility();
	for (auto i = NoclipControl::list.iterator(); !i.is_last(); i.next())
		i.item()->update(u);
	for (auto i = SentinelControl::list.iterator(); !i.is_last(); i.next())
		i.item()->update(u);
	for (auto i = PlayerTrigger::list.iterator(); !i.is_last(); i.next())
		i.item()->update(u);
	for (auto i = AIPlayerControl::list.iterator(); !i.is_last(); i.next())
		i.item()->update(u);
	for (auto i = SentinelCommon::list.iterator(); !i.is_last(); i.next())
		i.item()->update(u);
	for (auto i = Shockwave::list.iterator(); !i.is_last(); i.next())
		i.item()->update(u);
	for (auto i = PlayerCommon::list.iterator(); !i.is_last(); i.next())
		i.item()->update(u);

	for (s32 i = 0; i < updates.length; i++)
		(*updates[i])(u);

	for (auto i = LocalPlayerControl::list.iterator(); !i.is_last(); i.next())
		i.item()->update(u);

	Console::update(u);

	LerpTo<Vec3>::update_all(u);

	Audio::update();

	World::flush();
}

void Game::term()
{
	Audio::term();
}

void Game::draw_opaque(const RenderParams& render_params)
{
	View::draw_opaque(render_params);
	Rope::draw_opaque(render_params);
	for (auto i = SkinnedModel::list.iterator(); !i.is_last(); i.next())
		i.item()->draw(render_params);
}

void Game::draw_alpha(const RenderParams& render_params)
{
	Skybox::draw(render_params);
	SkyDecal::draw(render_params);

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
					radius = Vec3(body->size.y * 0.5f, body->size.x, body->size.x);
					color = Vec4(0, 1, 0, 1);
					break;
				case RigidBody::Type::CapsuleY:
					mesh_id = Asset::Mesh::cube;
					radius = Vec3(body->size.x, body->size.y * 0.5f, body->size.x);
					color = Vec4(0, 1, 0, 1);
					break;
				case RigidBody::Type::CapsuleZ:
					mesh_id = Asset::Mesh::cube;
					radius = Vec3(body->size.x, body->size.x, body->size.y * 0.5f);
					color = Vec4(0, 1, 0, 1);
					break;
				default:
					continue;
			}

			if (!render_params.camera->visible_sphere(transform.getOrigin(), fmax(radius.x, fmax(radius.y, radius.z))))
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
			sync->write(mesh_id);
		}
		sync->write(RenderOp::FillMode);
		sync->write(RenderFillMode::Fill);
	}
#endif

	View::draw_alpha(render_params);

	Console::draw(render_params);

	for (auto i = PortalControl::list.iterator(); !i.is_last(); i.next())
		i.item()->draw_alpha(render_params);
	for (auto i = PlayerCommon::list.iterator(); !i.is_last(); i.next())
		i.item()->draw_alpha(render_params);
	for (auto i = LocalPlayerControl::list.iterator(); !i.is_last(); i.next())
		i.item()->draw_alpha(render_params);
	for (auto i = LocalPlayer::list.iterator(); !i.is_last(); i.next())
		i.item()->draw_alpha(render_params);

	Menu::draw(render_params);

	for (s32 i = 0; i < draws.length; i++)
		(*draws[i])(render_params);

	if (cursor_updated)
		UI::mesh(render_params, Asset::Mesh::cursor, Game::cursor, Vec2(18, 18) * UI::scale);
}

void Game::update_cursor(const Update& u)
{
	if (!cursor_updated)
	{
		cursor_updated = true;
		cursor.x = LMath::clampf(cursor.x + u.input->cursor_x, 0.0f, u.input->width);
		cursor.y = LMath::clampf(cursor.y - u.input->cursor_y, 0.0f, u.input->height);
	}
}

void Game::draw_additive(const RenderParams& render_params)
{
	View::draw_additive(render_params);

#if DEBUG_AI
	AI::debug_draw(render_params);
#endif

}

void Game::execute(const Update& u, const char* cmd)
{
	if (strcmp(cmd, "third_person") == 0)
	{
		Game::data.third_person = !Game::data.third_person;
	}
	else if (strcmp(cmd, "noclip") == 0)
	{
		Vec3 pos = Vec3::zero;
		Quat quat = Quat::identity;
		if (LocalPlayerControl::list.count() > 0)
		{
			auto players = LocalPlayerControl::list.iterator();
			LocalPlayerControl* p = players.item();
			pos = p->get<Transform>()->absolute_pos();
			quat = p->camera->rot;

			while (!players.is_last())
			{
				World::remove(players.item()->entity());
				players.next();
			}

			Noclip* noclip = World::create<Noclip>();
			noclip->get<Transform>()->pos = pos;
		}
		else if (NoclipControl::list.count() > 0)
		{
			NoclipControl* noclip = NoclipControl::list.iterator().item();
			pos = noclip->get<Transform>()->absolute_pos();
			quat = noclip->camera->rot;
			World::remove(noclip->entity());
		}
	}
	else if (strstr(cmd, "timescale") == cmd)
	{
		const char* delimiter = strchr(cmd, ' ');
		if (delimiter)
		{
			const char* number_string = delimiter + 1;
			char* end;
			r32 value = std::strtod(number_string, &end);
			if (*end == '\0')
				time_scale = value;
		}
	}
	else if (strstr(cmd, "load") == cmd)
	{
		const char* delimiter = strchr(cmd, ' ');
		if (delimiter)
		{
			const char* level_name = delimiter + 1;
			AssetID level = Loader::find(level_name, AssetLookup::Level::names);
			if (level != AssetNull)
			{
				if (LocalPlayer::list.count() == 0)
				{
					LocalPlayer* player = LocalPlayer::list.add();
					new (player) LocalPlayer(AI::Team::A, 0);
				}
				for (s32 i = 0; i < (s32)AI::Team::count; i++)
					data.credits[i] = initial_credits;
				Menu::transition(level);
			}
		}
	}
}

void Game::schedule_load_level(AssetID level_id)
{
	scheduled_load_level = level_id;
}

void Game::unload_level()
{
	for (auto i = AIPlayer::list.iterator(); !i.is_last(); i.next())
		AIPlayer::list.remove(i.index);

	for (auto i = Entity::list.iterator(); !i.is_last(); i.next())
		World::remove(i.item());

	Loader::transients_free();
	updates.length = 0;
	draws.length = 0;
	for (s32 i = 0; i < cleanups.length; i++)
		(*cleanups[i])();
	cleanups.length = 0;
	Skybox::set(1.0f, Vec3::zero, Vec3::zero, Vec3::zero, AssetNull, AssetNull, AssetNull);
	Audio::post_global_event(AK::EVENTS::STOP_ALL);
	data.level = AssetNull;
	Menu::clear();
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

void EntityFinder::add(const char* name, Entity* e)
{
	NameEntry* entry = map.add();
	entry->name = name;
	entry->entity = e;
}

template<typename T>
struct LevelLink
{
	Ref<T>* ref;
	const char* target_name;
};

struct MoverEntry
{
	Mover* mover;
	const char* object_name;
	const char* end_name;
	r32 speed;
};

struct RopeEntry
{
	Vec3 pos;
	Quat rot;
	r32 max_distance;
	r32 slack;
};

struct SpawnEntry
{
	Ref<Transform> ref;
	AI::Team team;
};

void Game::load_level(const Update& u, AssetID l)
{
	time.total = 0.0f;

	scheduled_load_level = AssetNull;

	if (!Menu::is_special_level(data.level))
		data.previous_level = data.level;

	unload_level();

	Audio::post_global_event(AK::EVENTS::PLAY_START_SESSION);

	data.level = l;

	Physics::btWorld->setGravity(btVector3(0, -12.0f, 0));

	Array<Transform*> transforms;

	Array<RopeEntry> ropes;

	Array<Script*> scripts;

	Array<LevelLink<Entity>> links;
	Array<LevelLink<Transform>> transform_links;
	Array<MoverEntry> mover_links;
	Array<SpawnEntry> spawns;

	EntityFinder finder;
	
	cJSON* json = Loader::level(data.level);
	cJSON* element = json->child;
	while (element)
	{
		Entity* entity = nullptr;

		Vec3 pos = Json::get_vec3(element, "pos");
		Quat rot = Json::get_quat(element, "rot");
		Vec3 absolute_pos = pos;
		Quat absolute_rot = rot;

		s32 parent = cJSON_GetObjectItem(element, "parent")->valueint;
		if (parent != -1)
			transforms[parent]->to_world(&absolute_pos, &absolute_rot);

		b8 alpha = (b8)Json::get_s32(element, "alpha");
		b8 additive = (b8)Json::get_s32(element, "additive");
		s32 order = Json::get_s32(element, "order");

		if (cJSON_GetObjectItem(element, "StaticGeom"))
		{
			s32 inaccessible_index = Json::get_s32(element, "inaccessible_index");
			cJSON* meshes = cJSON_GetObjectItem(element, "meshes");
			cJSON* mesh = meshes->child;

			// First count the meshes
			s32 mesh_count = 0;
			while (mesh)
			{
				mesh = mesh->next;
				mesh_count++;
			}

			b8 has_inaccessible = cJSON_GetObjectItem(element, "inaccessible_index") || mesh_count > 1;

			// Start over from the beginning of the mesh list
			mesh = meshes->child;

			s32 mesh_index = 0;
			while (mesh)
			{
				char* mesh_ref = mesh->valuestring;

				Entity* m;

				s32 mesh_id = Loader::find(mesh_ref, AssetLookup::Mesh::names);
				vi_assert(mesh_id != AssetNull);
				if (mesh_index == inaccessible_index && has_inaccessible)
				{
					m = World::create<StaticGeom>(mesh_id, absolute_pos, absolute_rot, CollisionInaccessible, CollisionInaccessibleMask);
					m->get<View>()->color.w = 1.0f / 255.0f; // special G-buffer index for inaccessible materials
				}
				else
					m = World::create<StaticGeom>(mesh_id, absolute_pos, absolute_rot);

				if (entity)
					m->get<Transform>()->reparent(entity->get<Transform>());
				else
					entity = m;

				if (alpha || additive)
				{
					m->get<View>()->shader = Asset::Shader::flat;
					m->get<View>()->alpha(additive, order);
				}

				mesh = mesh->next;
				mesh_index++;
			}
		}
		else if (cJSON_GetObjectItem(element, "SentinelSpawn"))
		{
			entity = World::create<SentinelSpawn>(absolute_pos, absolute_rot);

			cJSON* entity_link = cJSON_GetObjectItem(element, "links")->child;
			if (entity_link)
			{
				LevelLink<Transform> link = { &entity->get<SentinelSpawnControl>()->spawn, entity_link->valuestring };
				transform_links.add(link);
				entity_link = entity_link->next;
			}

			while (entity_link)
			{
				LevelLink<Transform> link = { entity->get<SentinelSpawnControl>()->idle_path.add(), entity_link->valuestring };
				transform_links.add(link);
				entity_link = entity_link->next;
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
		else if (cJSON_GetObjectItem(element, "CreditsPickup"))
		{
			entity = World::create<CreditsPickupEntity>(absolute_pos, absolute_rot);
		}
		else if (cJSON_GetObjectItem(element, "Sentinel"))
		{
			AI::Team team = (AI::Team)Json::get_s32(element, "team", (s32)AI::Team::None);
			entity = World::create<Sentinel>(absolute_pos, absolute_rot, team);

			cJSON* links = cJSON_GetObjectItem(element, "links");
			cJSON* l = links->child;

			SentinelControl* sentinel = entity->add<SentinelControl>();
			while (l)
			{
				LevelLink<Transform> link = { sentinel->idle_path.add(), l->valuestring };
				transform_links.add(link);
				l = l->next;
			}
		}
		else if (cJSON_GetObjectItem(element, "PlayerSpawn"))
		{
			AI::Team team = (AI::Team)Json::get_s32(element, "team");

			if (Game::data.mode == Game::Mode::Multiplayer)
				entity = World::create<PlayerSpawn>(team);
			else
				entity = World::create<Empty>(); // in parkour mode, the spawn point is invisible

			absolute_pos += Vec3(0, 3.75f * 0.5f, 0);
			if (parent == -1)
				pos = absolute_pos;
			else
				pos = transforms[parent]->to_local(absolute_pos);

			spawns.add({ entity->get<Transform>(), team });
		}
		else if (cJSON_GetObjectItem(element, "Portal"))
		{
			entity = World::create<Portal>();
			const char* next = Json::get_string(element, "Portal");
			if (next)
				entity->get<PortalControl>()->next = Loader::find(next, AssetLookup::Level::names);
			entity->get<PlayerTrigger>()->radius = Json::get_r32(element, "scale", 1.0f) * 0.5f;
		}
		else if (cJSON_GetObjectItem(element, "PlayerTrigger"))
		{
			entity = World::create<Empty>();
			PlayerTrigger* trigger = entity->add<PlayerTrigger>();
			trigger->radius = Json::get_r32(element, "scale", 1.0f) * 0.5f;
		}
		else if (cJSON_GetObjectItem(element, "Mover"))
		{
			entity = World::create<MoverEntity>((b8)Json::get_s32(element, "reversed"), (b8)Json::get_s32(element, "translation", 1), (b8)Json::get_s32(element, "rotation", 1));
			cJSON* entity_link = cJSON_GetObjectItem(element, "links")->child;
			if (entity_link)
			{
				MoverEntry* entry = mover_links.add();
				entry->mover = entity->get<Mover>();
				entry->object_name = entity_link->valuestring;
				entry->end_name = entity_link->next->valuestring;
				entry->speed = Json::get_r32(element, "speed", 1.0f);
			}
			Mover* mover = entity->get<Mover>();
			mover->ease = (Ease::Type)Json::get_enum(element, "ease", Ease::type_names);
		}
		else if (cJSON_GetObjectItem(element, "World"))
		{
			AssetID texture = Loader::find(Json::get_string(element, "skybox_texture"), AssetLookup::Texture::names);
			Vec3 color = Json::get_vec3(element, "skybox_color");
			Vec3 ambient = Json::get_vec3(element, "ambient_color");
			Vec3 zenith = Json::get_vec3(element, "zenith_color");
			r32 far_plane = Json::get_r32(element, "far_plane", 100.0f);
			Skybox::set(far_plane, color, ambient, zenith, Asset::Shader::skybox, Asset::Mesh::skybox, texture);
			const char* mode_string = Json::get_string(element, "mode");
			if (mode_string)
				Game::data.mode = strcmp(mode_string, "parkour") == 0 ? Game::Mode::Parkour : Game::Mode::Multiplayer;
			else
				Game::data.mode = Game::Mode::Multiplayer;
		}
		else if (cJSON_GetObjectItem(element, "PointLight"))
		{
			entity = World::create<Empty>();
			PointLight* light = entity->add<PointLight>();
			light->color = Json::get_vec3(element, "color");
			light->radius = Json::get_r32(element, "radius");
		}
		else if (cJSON_GetObjectItem(element, "DirectionalLight"))
		{
			entity = World::create<Empty>();
			DirectionalLight* light = entity->add<DirectionalLight>();
			light->color = Json::get_vec3(element, "color");
			light->shadowed = Json::get_s32(element, "shadowed");
		}
		else if (cJSON_GetObjectItem(element, "AIPlayer"))
		{
			AIPlayer* player = AIPlayer::list.add();
			AI::Team team = (AI::Team)Json::get_s32(element, "team", (s32)AI::Team::None);
			new (player) AIPlayer(team);
		}
		else if (cJSON_GetObjectItem(element, "Socket"))
		{
			entity = World::create<SocketEntity>(absolute_pos, absolute_rot, (b8)Json::get_s32(element, "powered"));
			cJSON* entity_link = cJSON_GetObjectItem(element, "links")->child;
			if (entity_link)
			{
				LevelLink<Entity> link = { &entity->get<Socket>()->target, entity_link->valuestring };
				links.add(link);
			}
		}
		else if (cJSON_GetObjectItem(element, "SkyDecal"))
		{
			entity = World::create<Empty>();

			SkyDecal* decal = entity->add<SkyDecal>();
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
		else if (cJSON_GetObjectItem(element, "Prop"))
		{
			const char* name = Json::get_string(element, "Prop");

			b8 alpha = (b8)Json::get_s32(element, "alpha");
			b8 additive = (b8)Json::get_s32(element, "additive");
			s32 order = Json::get_s32(element, "order");
			const char* armature = Json::get_string(element, "armature");
			const char* animation = Json::get_string(element, "animation");

			if (name)
			{
				entity = World::create<Prop>(Loader::find(name, AssetLookup::Mesh::names), Loader::find(armature, AssetLookup::Armature::names), Loader::find(animation, AssetLookup::Animation::names));
				if (alpha || additive)
				{
					entity->get<View>()->shader = Asset::Shader::flat;
					entity->get<View>()->alpha(additive, order);
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

					AssetID mesh_id = Loader::find(mesh_ref, AssetLookup::Mesh::names);

					Entity* m = World::create<Prop>(mesh_id);
					if (entity)
						m->get<Transform>()->reparent(entity->get<Transform>());
					else
						entity = m;

					if (alpha || additive)
					{
						m->get<View>()->shader = Asset::Shader::flat;
						m->get<View>()->alpha(additive, order);
					}

					mesh = mesh->next;
				}
			}
		}
		else
			entity = World::create<Empty>();

		if (entity && entity->has<Transform>())
		{
			Transform* transform = entity->get<Transform>();
			transform->pos = pos;
			transform->rot = rot;

			if (parent != -1)
				transform->parent = transforms[parent];
			transforms.add(transform);
		}
		else
			transforms.add(nullptr);

		finder.add(Json::get_string(element, "name"), entity);

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

	for (s32 i = 0; i < mover_links.length; i++)
	{
		MoverEntry& link = mover_links[i];
		Entity* object = finder.find(link.object_name);
		Entity* end = finder.find(link.end_name);
		link.mover->setup(object->get<Transform>(), end->get<Transform>(), link.speed);
	}

	for (s32 i = 0; i < spawns.length; i++)
	{
		SpawnEntry& spawn = spawns[i];
		for (auto j = LocalPlayer::list.iterator(); !j.is_last(); j.next())
		{
			if (j.item()->team == spawn.team)
				j.item()->spawn = spawn.ref;
		}
		for (auto j = AIPlayer::list.iterator(); !j.is_last(); j.next())
		{
			if (j.item()->team == spawn.team)
				j.item()->spawn = spawn.ref;
		}
	}

	AI::load_nav_mesh(data.level);

	Physics::sync_static();

	for (s32 i = 0; i < ropes.length; i++)
		Rope::spawn(ropes[i].pos, ropes[i].rot * Vec3(0, 1, 0), ropes[i].max_distance, ropes[i].slack);

	// Set map view for local players
	{
		Entity* map_view = finder.find("map_view");
		if (map_view && map_view->has<Transform>())
		{
			for (auto i = LocalPlayer::list.iterator(); !i.is_last(); i.next())
				i.item()->map_view = map_view->get<Transform>();
		}
	}

	for (s32 i = 0; i < scripts.length; i++)
		scripts[i]->function(u, finder);

	Loader::level_free(json);
}

}
