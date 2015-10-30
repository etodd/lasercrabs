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
#include "physics.h"
#include "common.h"
#include "render/ui.h"
#include "asset/armature.h"
#include "asset/texture.h"
#include "asset/level.h"
#include "asset/mesh.h"
#include "asset/shader.h"
#include "input.h"
#include "ai.h"
#include "mersenne-twister.h"
#include <time.h>
#include "asset/lookup.h"
#include "cJSON.h"

#if DEBUG
	#include "console.h"

	#define DEBUG_ENTITIES 0
	#define DEBUG_AI 0
#endif

#include "game.h"

namespace VI
{

float Game::time_scale = 1.0f;
AssetID Game::scheduled_load_level = AssetNull;
Game::Data Game::data =
{
	AssetNull,
};
const Game::Data Game::initial_data =
{
	Asset::Level::level1,
};

Array<UpdateFunction> Game::updates = Array<UpdateFunction>();
Array<DrawFunction> Game::draws = Array<DrawFunction>();
Array<CleanupFunction> Game::cleanups = Array<CleanupFunction>();

bool Game::init(RenderSync* sync)
{
	UI::init(sync);

	AI::init();

#if DEBUG
	Console::init();
#endif

	schedule_load_level(Asset::Level::title);

	return true;
}

void Game::update(const Update& u)
{
	if (scheduled_load_level != AssetNull)
		load_level(scheduled_load_level);

	Physics::sync_dynamic();

	for (auto i = World::components<Ragdoll>().iterator(); !i.is_last(); i.next())
		i.item()->update(u);
	for (auto i = World::components<Animator>().iterator(); !i.is_last(); i.next())
		i.item()->update(u);

	Physics::sync_static();

	for (auto i = World::components<NoclipControl>().iterator(); !i.is_last(); i.next())
		i.item()->update(u);

	for (int i = 0; i < updates.length; i++)
		(*updates[i])(u);

#if DEBUG
	Console::update(u);
#endif
}

void Game::term()
{
}

void Game::draw_opaque(const RenderParams& render_params)
{
	Skybox::draw(render_params);
	View::draw_opaque(render_params);
	for (auto i = World::components<SkinnedModel>().iterator(); !i.is_last(); i.next())
		i.item()->draw(render_params);
}

void Game::draw_alpha(const RenderParams& render_params)
{
#if DEBUG
	Console::draw(render_params);

#if DEBUG_ENTITIES
	for (auto i = World::components<Debug>().iterator(); !i.is_last(); i.next())
		i.item()->draw(render_params);
#endif
#endif

	View::draw_alpha(render_params);

	for (int i = 0; i < draws.length; i++)
		(*draws[i])(render_params);
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
	if (strstr(cmd, "timescale") == cmd)
	{
		const char* delimiter = strchr(cmd, ' ');
		if (delimiter)
		{
			const char* number_string = delimiter + 1;
			char* end;
			float value = std::strtod(number_string, &end);
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
				schedule_load_level(level);
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
	for (auto i = World::list.iterator(); !i.is_last(); i.next())
		World::remove(i.item());
	Loader::transients_free();
	updates.length = 0;
	draws.length = 0;
	for (int i = 0; i < cleanups.length; i++)
		(*cleanups[i])();
	cleanups.length = 0;
	Skybox::set(1.0f, Vec3::zero, Vec3::zero, Vec3::zero, AssetNull, AssetNull, AssetNull);
	data.level = AssetNull;
}

template<typename T>
struct LevelLink
{
	Ref<T>* ref;
	const char* target_name;
};

Entity* EntityFinder::find(const char* name) const
{
	for (int j = 0; j < map.length; j++)
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

void Game::load_level(AssetID l)
{
	scheduled_load_level = AssetNull;

	unload_level();

	data.level = l;

	Physics::btWorld->setGravity(btVector3(0, -9.8f, 0));

	Array<Transform*> transforms;

	Array<LevelLink<Entity>> links;

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

		int parent = cJSON_GetObjectItem(element, "parent")->valueint;
		if (parent != -1)
			transforms[parent]->to_world(&absolute_pos, &absolute_rot);

		bool alpha = (bool)Json::get_int(element, "alpha");
		bool additive = (bool)Json::get_int(element, "additive");
		int order = Json::get_int(element, "order");

		if (cJSON_GetObjectItem(element, "StaticGeom"))
		{
			int reflective_index = Json::get_int(element, "reflective_index");
			cJSON* meshes = cJSON_GetObjectItem(element, "meshes");
			cJSON* mesh = meshes->child;

			// First count the meshes
			int mesh_count = 0;
			while (mesh)
			{
				mesh = mesh->next;
				mesh_count++;
			}
			bool has_reflective = cJSON_GetObjectItem(element, "reflective_index") || mesh_count > 0;

			// Start over from the beginning of the mesh list
			mesh = meshes->child;

			int mesh_index = 0;
			while (mesh)
			{
				int mesh_ref = mesh->valueint;

				AssetID mesh_id = Loader::mesh_ref_to_id(data.level, mesh_ref);

				Entity* m;
				if (mesh_index == reflective_index && has_reflective)
					m = World::create<StaticGeom>(mesh_id, absolute_pos, absolute_rot, CollisionReflective, CollisionReflectiveMask);
				else
					m = World::create<StaticGeom>(mesh_id, absolute_pos, absolute_rot);

				if (alpha || additive)
				{
					m->get<View>()->shader = Asset::Shader::flat;
					m->get<View>()->alpha(additive, order);
				}

				if (mesh_index == 0)
					entity = m;
				else
					m->get<Transform>()->reparent(entity->get<Transform>());

				mesh = mesh->next;
				mesh_index++;
			}
		}
		else if (cJSON_GetObjectItem(element, "World"))
		{
			AssetID texture = Loader::find(Json::get_string(element, "skybox_texture"), AssetLookup::Texture::names);
			Vec3 color = Json::get_vec3(element, "skybox_color");
			Vec3 ambient = Json::get_vec3(element, "ambient_color");
			Vec3 zenith = Json::get_vec3(element, "zenith_color");
			float far_plane = Json::get_float(element, "far_plane", 100.0f);
			Skybox::set(far_plane, color, ambient, zenith, Asset::Shader::skybox, Asset::Mesh::skybox, texture);
		}
		else if (cJSON_GetObjectItem(element, "PointLight"))
		{
			entity = World::create<Empty>();
			PointLight* light = entity->add<PointLight>();
			light->radius = Json::get_float(element, "radius");
			light->color = Json::get_vec3(element, "color");
		}
		else if (cJSON_GetObjectItem(element, "Prop"))
		{
			const char* name = Json::get_string(element, "Prop");

			bool alpha = (bool)Json::get_int(element, "alpha");
			bool additive = (bool)Json::get_int(element, "additive");
			int order = Json::get_int(element, "order");
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
					int mesh_ref = mesh->valueint;

					AssetID mesh_id = Loader::mesh_ref_to_id(data.level, mesh_ref);

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

	for (int i = 0; i < links.length; i++)
	{
		LevelLink<Entity>& link = links[i];
		*link.ref = finder.find(link.target_name);
	}

	AI::load_nav_mesh(data.level);

	Physics::sync_static();

	Loader::level_free(json);
}

}