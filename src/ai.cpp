#include "data/import_common.h"
#include "load.h"
#include "data/components.h"
#include "bullet/src/BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "game/entities.h"
#include "recast/Recast/Include/Recast.h"
#include "recast/Detour/Include/DetourNavMeshBuilder.h"
#include "recast/Detour/Include/DetourCommon.h"
#include "data/priority_queue.h"
#include "mersenne/mersenne-twister.h"
#include "render/ui.h"
#include "asset/shader.h"
#include "game/player.h"
#include "game/game.h"
#include "game/team.h"
#include "game/entities.h"
#include "game/drone.h"
#include "game/minion.h"

#define DEBUG_AUDIO 0

#if DEBUG_AUDIO
#include "render/views.h"
#include "asset/mesh.h"
#endif

namespace VI
{

namespace AI
{


Bitmask<nav_max_obstacles> obstacles;
SyncRingBuffer<SYNC_IN_SIZE> sync_in;
SyncRingBuffer<SYNC_OUT_SIZE> sync_out;
b8 render_meshes_dirty;
u32 callback_in_id = 1;
u32 callback_out_id = 1;
u32 record_id_current = 1;
Revision level_revision;
Revision level_revision_worker;
AssetID drone_render_mesh = AssetNull;
DroneNavMesh drone_nav_mesh;
Worker::DroneNavMeshKey drone_nav_mesh_key;
NavGameState nav_game_state;
Worker::AstarQueue astar_queue(&drone_nav_mesh_key);
Worker::DroneNavContext ctx =
{
	drone_nav_mesh,
	&drone_nav_mesh_key,
	nav_game_state,
	&astar_queue,
	0,
};

void init()
{
	drone_render_mesh = Loader::dynamic_mesh_permanent(1);
	Loader::dynamic_mesh_attrib(RenderDataType::Vec3);
}

void loop()
{
	Worker::loop();
}

void quit()
{
	sync_in.lock();
	sync_in.write(Op::Quit);
	sync_in.unlock();
}

#define RECTIFIER_UPDATE_INTERVAL 0.5f
r32 rectifier_timer = RECTIFIER_UPDATE_INTERVAL;

void update(const Update& u)
{
	rectifier_timer -= u.time.delta;
	if (rectifier_timer < 0.0f)
	{
		rectifier_timer += RECTIFIER_UPDATE_INTERVAL;

		NavGameState state;
		for (auto i = Rectifier::list.iterator(); !i.is_last(); i.next())
			state.rectifiers.add({ i.item()->get<Transform>()->absolute_pos(), i.item()->team });

		for (auto i = ForceField::list.iterator(); !i.is_last(); i.next())
			state.force_fields.add({ i.item()->get<Transform>()->absolute_pos(), i.item()->team });

		sync_in.lock();
		sync_in.write(Op::UpdateState);
		sync_in.write(state.rectifiers.length);
		sync_in.write(state.rectifiers.data, state.rectifiers.length);
		sync_in.write(state.force_fields.length);
		sync_in.write(state.force_fields.data, state.force_fields.length);
		sync_in.unlock();
	}

	sync_out.lock();
	while (sync_out.can_read())
	{
		Callback cb;
		sync_out.read(&cb);
		switch (cb)
		{
			case Callback::Path:
			{
				LinkEntryArg<const Result&> link;
				sync_out.read(&link);
				Result result;
				sync_out.read(&result.path);
				result.id = callback_out_id;
				callback_out_id++;
				if (level_revision == level_revision_worker) // prevent entity ID/revision collisions
					(&link)->fire(result);
				break;
			}
			case Callback::DronePath:
			{
				LinkEntryArg<const DroneResult&> link;
				sync_out.read(&link);
				DroneResult result;
				sync_out.read(&result.path);
				result.id = callback_out_id;
				callback_out_id++;
				if (level_revision == level_revision_worker) // prevent entity ID/revision collisions
					(&link)->fire(result);
				break;
			}
			case Callback::Point:
			{
				LinkEntryArg<const Vec3&> link;
				sync_out.read(&link);
				Vec3 result;
				sync_out.read(&result);
				callback_out_id++;
				if (level_revision == level_revision_worker) // prevent entity ID/revision collisions
					(&link)->fire(result);
				break;
			}
			case Callback::DronePoint:
			{
				LinkEntryArg<const DronePathNode&> link;
				sync_out.read(&link);
				DronePathNode result;
				sync_out.read(&result);
				callback_out_id++;
				if (level_revision == level_revision_worker) // prevent entity ID/revision collisions
					(&link)->fire(result);
				break;
			}
			case Callback::Load:
			{
				sync_out.read(&level_revision_worker);
				break;
			}
			case Callback::AudioPath:
			{
				Ref<AudioEntry> entry;
				s8 listener;
				r32 path_length;
				r32 straight_distance;
				sync_out.read(&entry);
				sync_out.read(&listener);
				sync_out.read(&path_length);
				sync_out.read(&straight_distance);
				callback_out_id++;
				if (level_revision == level_revision_worker) // prevent entity ID/revision collisions
				{
					if (entry.ref())
						entry.ref()->pathfind_result(listener, path_length, straight_distance);
				}
				break;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}
	}
	sync_out.unlock();
}

b8 match(Team t, TeamMask m)
{
	if (m == TeamNone)
		return t == TeamNone;
	else
		return t != TeamNone && (m & (1 << t));
}

u32 obstacle_add(const Vec3& pos, r32 radius, r32 height)
{
	if (obstacles.count() == nav_max_obstacles)
		return u32(nav_max_obstacles); // no room

	u32 id;
	b8 found = false;
	for (s32 i = 0; i < obstacles.count(); i++)
	{
		if (!obstacles.get(i))
		{
			id = u32(i);
			found = true;
			break;
		}
	}

	if (!found)
		id = obstacles.end;
	obstacles.set(id, true);

	sync_in.lock();
	sync_in.write(Op::ObstacleAdd);
	sync_in.write(id);
	sync_in.write(pos);
	sync_in.write(radius);
	sync_in.write(height);
	sync_in.unlock();

	return id;
}

void obstacle_remove(u32 id)
{
	if (id < nav_max_obstacles)
	{
		obstacles.set(id, false);
		sync_in.lock();
		sync_in.write(Op::ObstacleRemove);
		sync_in.write(id);
		sync_in.unlock();
	}
}

void load(AssetID id, const char* filename)
{
	sync_in.lock();
	sync_in.write(Op::Load);
	sync_in.write(id);
	{
		s32 length = filename ? s32(strlen(filename)) : 0;
		vi_assert(length <= MAX_PATH_LENGTH);
		sync_in.write(length);
		if (length > 0)
			sync_in.write(filename, length);
	}
	sync_in.unlock();
	level_revision++;

	drone_nav_mesh.~DroneNavMesh();
	new (&drone_nav_mesh) DroneNavMesh();
	drone_nav_mesh_key.~DroneNavMeshKey();
	new (&drone_nav_mesh_key) Worker::DroneNavMeshKey();

	if (filename)
	{
		FILE* f = fopen(filename, "rb");

		{
			// skip minion nav mesh
			Vec3 min;
			s32 width;
			s32 height;
			fread(&min, sizeof(Vec3), 1, f);
			fread(&width, sizeof(s32), 1, f);
			fread(&height, sizeof(s32), 1, f);
			s32 count = width * height;
			for (s32 i = 0; i < count; i++)
			{
				s32 layer_count;
				fread(&layer_count, sizeof(s32), 1, f);
				for (s32 j = 0; j < layer_count; j++)
				{
					s32 data_size;
					fread(&data_size, sizeof(s32), 1, f);
					fseek(f, data_size, SEEK_CUR);
				}
			}
		}

		drone_nav_mesh.read(f);
		fclose(f);
		drone_nav_mesh_key.resize(drone_nav_mesh);
	}

	{
		Array<Vec3> vertices;
		for (s32 chunk_index = 0; chunk_index < drone_nav_mesh.chunks.length; chunk_index++)
		{
			const DroneNavMeshChunk& chunk = drone_nav_mesh.chunks[chunk_index];
			for (s32 i = 0; i < chunk.vertices.length; i++)
				vertices.add(chunk.vertices[i]);
		}

		Array<s32> indices;
		for (s32 i = 0; i < vertices.length; i++)
			indices.add(i);

		RenderSync* sync = Loader::swapper->get();;

		sync->write(RenderOp::UpdateAttribBuffers);
		sync->write(drone_render_mesh);

		sync->write<s32>(vertices.length);
		sync->write<Vec3>(vertices.data, vertices.length);

		sync->write(RenderOp::UpdateIndexBuffer);
		sync->write(drone_render_mesh);

		sync->write<s32>(indices.length);
		sync->write<s32>(indices.data, indices.length);
	}

	render_meshes_dirty = true;
}

u32 random_path(const Vec3& pos, const Vec3& patrol_point, AI::Team team, r32 range, const LinkEntryArg<const Result&>& callback)
{
	u32 id = callback_in_id;
	callback_in_id++;

	sync_in.lock();
	sync_in.write(Op::RandomPath);
	sync_in.write(pos);
	sync_in.write(patrol_point);
	sync_in.write(team);
	sync_in.write(range);
	sync_in.write(callback);
	sync_in.unlock();

	return id;
}

u32 closest_walk_point(const Vec3& pos, const LinkEntryArg<const Vec3&>& callback)
{
	u32 id = callback_in_id;
	callback_in_id++;

	sync_in.lock();
	sync_in.write(Op::ClosestWalkPoint);
	sync_in.write(pos);
	sync_in.write(callback);
	sync_in.unlock();

	return id;
}

u32 drone_random_path(DroneAllow rule, Team team, const Vec3& pos, const Vec3& normal, const LinkEntryArg<const DroneResult&>& callback)
{
	return drone_pathfind(DronePathfind::Random, rule, team, pos, normal, Vec3::zero, Vec3::zero, callback);
}

u32 pathfind(AI::Team team, const Vec3& a, const Vec3& b, const LinkEntryArg<const Result&>& callback)
{
	u32 id = callback_in_id;
	callback_in_id++;

	sync_in.lock();
	sync_in.write(Op::Pathfind);
	sync_in.write(team);
	sync_in.write(a);
	sync_in.write(b);
	sync_in.write(callback);
	sync_in.unlock();

	return id;
}

u32 drone_pathfind(DronePathfind type, DroneAllow rule, Team team, const Vec3& a, const Vec3& a_normal, const Vec3& b, const Vec3& b_normal, const LinkEntryArg<const DroneResult&>& callback)
{
	u32 id = callback_in_id;
	callback_in_id++;

	sync_in.lock();
	sync_in.write(Op::DronePathfind);
	sync_in.write(type);
	sync_in.write(rule);
	sync_in.write(team);
	sync_in.write(callback);
	sync_in.write(a);
	sync_in.write(a_normal);
	if (type != DronePathfind::Random)
	{
		if (type != DronePathfind::Spawn)
			sync_in.write(b);
		if (type != DronePathfind::Target)
			sync_in.write(b_normal);
	}
	sync_in.unlock();
	
	return id;
}

r32 audio_pathfind(const Vec3& a, const Vec3& b)
{
	return Worker::audio_pathfind(ctx, a, b);
}

u32 audio_pathfind(const Vec3& a, const Vec3& b, AudioEntry* entry, s8 listener, r32 straight_distance)
{
	u32 id = callback_in_id;
	callback_in_id++;

	sync_in.lock();
	sync_in.write(Op::AudioPathfind);
	Ref<AudioEntry> ref = entry;
	sync_in.write(ref);
	sync_in.write(listener);
	sync_in.write(a);
	sync_in.write(b);
	sync_in.write(straight_distance);
	sync_in.unlock();
	
	return id;
}

void audio_reverb_calc(const Vec3& pos, ReverbCell* output)
{
	Worker::audio_reverb_calc(ctx, pos, output);
}

u32 drone_closest_point(const Vec3& pos, AI::Team team, const LinkEntryArg<const DronePathNode&>& callback)
{
	u32 id = callback_in_id;
	callback_in_id++;

	sync_in.lock();
	sync_in.write(Op::DroneClosestPoint);
	sync_in.write(callback);
	sync_in.write(team);
	sync_in.write(pos);
	sync_in.unlock();
	
	return id;
}

void drone_mark_adjacency_bad(DroneNavMeshNode a, DroneNavMeshNode b)
{
	sync_in.lock();
	sync_in.write(Op::DroneMarkAdjacencyBad);
	sync_in.write(a);
	sync_in.write(b);
	sync_in.unlock();
}

void NavGameState::clear()
{
	rectifiers.length = 0;
	force_fields.length = 0;
}

const PathZone* PathZone::get(const Vec3& pos, const Entity* target)
{
	for (s32 i = 0; i < Game::level.path_zones.length; i++)
	{
		const PathZone& zone = Game::level.path_zones[i];
		Vec3 min = zone.pos - zone.radius;
		Vec3 max = zone.pos + zone.radius;
		if (pos.x > min.x && pos.x < max.x
			&& pos.y > min.y && pos.y < max.y
			&& pos.z > min.z && pos.z < max.z)
		{
			for (s32 j = 0; j < zone.targets.length; j++)
			{
				if (zone.targets[j].ref() == target)
					return &zone;
			}
		}
	}
	return nullptr;
}

#if DEBUG
AssetID render_mesh = AssetNull;
void refresh_nav_render_meshes(const RenderParams& params)
{
	if (!render_meshes_dirty)
		return;

	std::this_thread::sleep_for(std::chrono::milliseconds(30));
	sync_in.lock();

	if (render_mesh == AssetNull)
	{
		render_mesh = Loader::dynamic_mesh_permanent(1);
		Loader::dynamic_mesh_attrib(RenderDataType::Vec3);
		Loader::shader_permanent(Asset::Shader::flat);
	}

	Array<Vec3> vertices;
	Array<s32> indices;

	// nav mesh
	{
		if (Worker::nav_mesh)
		{
			for (s32 tile_id = 0; tile_id < Worker::nav_mesh->getMaxTiles(); tile_id++)
			{
				const dtMeshTile* tile = ((const dtNavMesh*)Worker::nav_mesh)->getTile(tile_id);
				if (!tile->header)
					continue;

				for (s32 i = 0; i < tile->header->polyCount; i++)
				{
					const dtPoly* p = &tile->polys[i];
					if (p->getType() == DT_POLYTYPE_OFFMESH_CONNECTION)	// skip off-mesh links.
						continue;

					const dtPolyDetail* pd = &tile->detailMeshes[i];

					for (s32 j = 0; j < pd->triCount; j++)
					{
						const u8* t = &tile->detailTris[(pd->triBase + j) * 4];
						for (s32 k = 0; k < 3; k++)
						{
							if (t[k] < p->vertCount)
								memcpy(vertices.add(), &tile->verts[p->verts[t[k]] * 3], sizeof(Vec3));
							else
								memcpy(vertices.add(), &tile->detailVerts[(pd->vertBase + t[k] - p->vertCount) * 3], sizeof(Vec3));
							indices.add(indices.length);
						}
					}
				}
			}
		}

		params.sync->write(RenderOp::UpdateAttribBuffers);
		params.sync->write(render_mesh);

		params.sync->write<s32>(vertices.length);
		params.sync->write<Vec3>(vertices.data, vertices.length);

		params.sync->write(RenderOp::UpdateIndexBuffer);
		params.sync->write(render_mesh);

		params.sync->write<s32>(indices.length);
		params.sync->write<s32>(indices.data, indices.length);
	}

	sync_in.unlock();

	render_meshes_dirty = false;
}

void render_helper(const RenderParams& params, AssetID m, RenderPrimitiveMode primitive_mode, RenderFillMode fill_mode)
{
	if (m == AssetNull)
		return;

	Loader::shader_permanent(Asset::Shader::flat);

	params.sync->write(RenderOp::Shader);
	params.sync->write(Asset::Shader::flat);
	params.sync->write(params.technique);

	params.sync->write(RenderOp::Uniform);
	params.sync->write(Asset::Uniform::diffuse_color);
	params.sync->write(RenderDataType::Vec4);
	params.sync->write<s32>(1);
	params.sync->write(Vec4(1, 1, 1, 0.75f));

	params.sync->write(RenderOp::Uniform);
	params.sync->write(Asset::Uniform::mvp);
	params.sync->write(RenderDataType::Mat4);
	params.sync->write<s32>(1);
	params.sync->write<Mat4>(params.view_projection);

	params.sync->write(RenderOp::FillMode);
	params.sync->write(fill_mode);
	params.sync->write(RenderOp::LineWidth);
	params.sync->write<r32>(1 * UI::scale);

	params.sync->write(RenderOp::Mesh);
	params.sync->write(primitive_mode);
	params.sync->write(m);

	params.sync->write(RenderOp::FillMode);
	params.sync->write(RenderFillMode::Fill);
}

void debug_draw_nav_mesh(const RenderParams& params)
{
	refresh_nav_render_meshes(params);
	render_helper(params, render_mesh, RenderPrimitiveMode::Triangles, RenderFillMode::Point);
}

#endif

void draw_hollow(const RenderParams& params)
{
	if (!(params.camera->mask & RENDER_MASK_DEFAULT))
		return;

	Loader::shader_permanent(Asset::Shader::nav_dots);

	params.sync->write(RenderOp::Shader);
	params.sync->write(Asset::Shader::nav_dots);
	params.sync->write(params.technique);

	params.sync->write(RenderOp::Uniform);
	params.sync->write(Asset::Uniform::diffuse_color);
	params.sync->write(RenderDataType::Vec4);
	params.sync->write<s32>(1);
	params.sync->write(Vec4(1, 1, 1, 0.4f));

	params.sync->write(RenderOp::Uniform);
	params.sync->write(Asset::Uniform::mv);
	params.sync->write(RenderDataType::Mat4);
	params.sync->write<s32>(1);
	params.sync->write<Mat4>(params.view);

	params.sync->write(RenderOp::Uniform);
	params.sync->write(Asset::Uniform::p);
	params.sync->write(RenderDataType::Mat4);
	params.sync->write<s32>(1);
	params.sync->write<Mat4>(params.camera->projection);

	{
		Vec3 center;
		r32 range;
		if (params.camera->range > 0.0f)
		{
			range = params.camera->range;
			center = params.camera->range_center;
		}
		else
		{
			range = DRONE_MAX_DISTANCE * 2.0f;
			center = Vec3::zero;
		}

		params.sync->write(RenderOp::Uniform);
		params.sync->write(Asset::Uniform::range);
		params.sync->write(RenderDataType::R32);
		params.sync->write<s32>(1);
		params.sync->write<r32>(range);

		params.sync->write(RenderOp::Uniform);
		params.sync->write(Asset::Uniform::range_center);
		params.sync->write(RenderDataType::Vec3);
		params.sync->write<s32>(1);
		params.sync->write(center);
	}

	params.sync->write(RenderOp::FillMode);
	params.sync->write(RenderFillMode::Point);

	params.sync->write(RenderOp::Mesh);
	params.sync->write(RenderPrimitiveMode::Points);
	params.sync->write(drone_render_mesh);

	params.sync->write(RenderOp::FillMode);
	params.sync->write(RenderFillMode::Fill);
}

ComponentMask entity_mask = Rectifier::component_mask
	| Drone::component_mask
	| Minion::component_mask
	| Battery::component_mask
	| ForceField::component_mask
	| Bolt::component_mask
	| Grenade::component_mask
	| SpawnPoint::component_mask
	| Turret::component_mask
	| MinionSpawner::component_mask;

AI::Team entity_team(const Entity* e)
{
	if (e->has<AIAgent>())
		return e->get<AIAgent>()->team;
	else if (e->has<Battery>())
		return e->get<Battery>()->team;
	else if (e->has<Rectifier>())
		return e->get<Rectifier>()->team;
	else if (e->has<ForceField>())
		return e->get<ForceField>()->team;
	else if (e->has<Bolt>())
		return e->get<Bolt>()->team;
	else if (e->has<Grenade>())
		return e->get<Grenade>()->team;
	else if (e->has<SpawnPoint>())
		return e->get<SpawnPoint>()->team;
	else if (e->has<Turret>())
		return e->get<Turret>()->team;
	else if (e->has<MinionSpawner>())
		return e->get<MinionSpawner>()->team;
	else if (e->has<Flag>())
		return e->get<Flag>()->team;
	else
	{
		return TeamNone;
		vi_assert(false);
	}
}


}

}
