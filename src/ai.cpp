#include "ai.h"
#include "data/import_common.h"
#include "load.h"
#include "asset/shader.h"
#include "data/components.h"
#include "physics.h"
#include "bullet/src/BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "game/entities.h"
#include "recast/Recast/Include/Recast.h"
#include "recast/Detour/Include/DetourNavMeshBuilder.h"
#include "data/priority_queue.h"
#include <Windows.h>

namespace VI
{

AssetID AI::render_mesh = AssetNull;
AssetID AI::awk_render_mesh = AssetNull;
dtNavMesh* AI::nav_mesh = nullptr;
AwkNavMesh* AI::awk_nav_mesh = nullptr;
AwkNavMeshKey AI::awk_nav_mesh_key;
dtTileCache* AI::nav_tile_cache = nullptr;
dtNavMeshQuery* AI::nav_mesh_query = nullptr;
dtQueryFilter AI::default_query_filter = dtQueryFilter();
const r32 AI::default_search_extents[] = { 8, 8, 8 };
b8 AI::render_mesh_dirty = false;

AI::Goal::Goal()
	: entity(), has_entity(true)
{
}

b8 AI::Goal::valid() const
{
	return !has_entity || entity.ref();
}

Vec3 AI::Goal::get_pos() const
{
	Entity* e = entity.ref();
	if (has_entity && e)
		return e->get<Transform>()->absolute_pos();
	else
		return pos;
}

void AI::Goal::set(const Vec3& p)
{
	pos = p;
	has_entity = false;
}

void AI::Goal::set(Entity* e)
{
	entity = e;
	has_entity = true;
}

void NavMeshProcess::process(struct dtNavMeshCreateParams* params, unsigned char* polyAreas, unsigned short* polyFlags)
{
	for (int i = 0; i < params->polyCount; i++)
		polyFlags[i] = 1;
}

AI::Team AI::other(AI::Team t)
{
	return t == AI::Team::A ? AI::Team::B : AI::Team::A;
}

static const u16 RC_MESH_NULL_IDX = 0xffff;

void AI::init()
{
	nav_mesh_query = dtAllocNavMeshQuery();
	default_query_filter.setIncludeFlags((u16)-1);
	default_query_filter.setExcludeFlags(0);

	render_mesh = Loader::dynamic_mesh_permanent(1);
	Loader::dynamic_mesh_attrib(RenderDataType::Vec3);
	Loader::shader_permanent(Asset::Shader::flat);

	awk_render_mesh = Loader::dynamic_mesh_permanent(1);
	Loader::dynamic_mesh_attrib(RenderDataType::Vec3);
}

void AI::load_nav_mesh(AssetID id)
{
	nav_mesh = Loader::nav_mesh(id);
	nav_tile_cache = Loader::nav_tile_cache;

	awk_nav_mesh = Loader::awk_nav_mesh(id);
	if (awk_nav_mesh)
		awk_nav_mesh_key.reset(*awk_nav_mesh);

	render_mesh_dirty = true;

	dtStatus status = nav_mesh_query->init(nav_mesh, 2048);
	vi_assert(dtStatusSucceed(status));
}

void AI::update(const Update& u)
{
	if (nav_tile_cache)
	{
		dtStatus status = nav_tile_cache->update(u.time.delta, nav_mesh);
		vi_assert(dtStatusSucceed(status));
	}
}

u32 AI::obstacle_add(const Vec3& pos, r32 radius, r32 height)
{
	render_mesh_dirty = true;
	vi_assert(nav_tile_cache);
	u32 id;
	dtStatus status = nav_tile_cache->addObstacle((r32*)&pos, radius, height, &id);
	return id;
}

void AI::obstacle_remove(u32 id)
{
	render_mesh_dirty = true;
	nav_tile_cache->removeObstacle(id);
}

void AI::refresh_nav_render_meshes(const RenderParams& params)
{
	if (render_mesh_dirty)
	{
		Array<Vec3> vertices;
		Array<s32> indices;

		// nav mesh
		{
			if (nav_mesh)
			{
				for (s32 tile_id = 0; tile_id < nav_mesh->getMaxTiles(); tile_id++)
				{
					const dtMeshTile* tile = ((const dtNavMesh*)nav_mesh)->getTile(tile_id);
					if (!tile->header)
						continue;

					for (s32 i = 0; i < tile->header->vertCount; i++)
					{
						memcpy(vertices.add(), &tile->verts[i * 3], sizeof(Vec3));
						indices.add(indices.length);
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

		vertices.length = 0;
		indices.length = 0;

		// awk nav mesh
		{
			if (awk_nav_mesh)
			{
				s32 vertex_count = 0;
				for (s32 chunk_index = 0; chunk_index < awk_nav_mesh->chunks.length; chunk_index++)
				{
					const AwkNavMeshChunk& chunk = awk_nav_mesh->chunks[chunk_index];

					for (s32 i = 0; i < chunk.vertices.length; i++)
						vertices.add(chunk.vertices[i]);

					for (s32 i = 0; i < chunk.adjacency.length; i++)
					{
						const AwkNavMeshAdjacency& adjacency = chunk.adjacency[i];
						for (s32 j = 0; j < adjacency.length; j++)
						{
							const AwkNavMeshNode& neighbor = adjacency[j];
							indices.add(vertex_count + i);

							s32 neighbor_chunk_vertex_index = 0;
							for (s32 k = 0; k < neighbor.chunk; k++)
								neighbor_chunk_vertex_index += awk_nav_mesh->chunks[k].vertices.length;
							indices.add(neighbor_chunk_vertex_index + neighbor.vertex);
						}
					}

					vertex_count += chunk.vertices.length;
				}
			}

			params.sync->write(RenderOp::UpdateAttribBuffers);
			params.sync->write(awk_render_mesh);

			params.sync->write<s32>(vertices.length);
			params.sync->write<Vec3>(vertices.data, vertices.length);

			params.sync->write(RenderOp::UpdateIndexBuffer);
			params.sync->write(awk_render_mesh);

			params.sync->write<s32>(indices.length);
			params.sync->write<s32>(indices.data, indices.length);
		}

		render_mesh_dirty = false;
	}
}

#if DEBUG
void render_helper(const RenderParams& params, AssetID m, RenderPrimitiveMode primitive_mode)
{
	params.sync->write(RenderOp::Shader);
	params.sync->write(Asset::Shader::flat);
	params.sync->write(params.technique);

	params.sync->write(RenderOp::Uniform);
	params.sync->write(Asset::Uniform::diffuse_color);
	params.sync->write(RenderDataType::Vec4);
	params.sync->write<s32>(1);
	params.sync->write(Vec4(0, 1, 0, 0.5f));

	Mat4 mvp = params.view_projection;

	params.sync->write(RenderOp::Uniform);
	params.sync->write(Asset::Uniform::mvp);
	params.sync->write(RenderDataType::Mat4);
	params.sync->write<s32>(1);
	params.sync->write<Mat4>(mvp);

	params.sync->write(RenderOp::FillMode);
	params.sync->write(RenderFillMode::Point);
	params.sync->write(RenderOp::PointSize);
	params.sync->write<r32>(4 * UI::scale);
	params.sync->write(RenderOp::CullMode);
	params.sync->write(RenderCullMode::None);

	params.sync->write(RenderOp::Mesh);
	params.sync->write(primitive_mode);
	params.sync->write(m);

	params.sync->write(RenderOp::FillMode);
	params.sync->write(RenderFillMode::Fill);
	params.sync->write(RenderOp::CullMode);
	params.sync->write(RenderCullMode::Back);
}
#endif

void AI::debug_draw_nav_mesh(const RenderParams& params)
{
#if DEBUG
	refresh_nav_render_meshes(params);
	render_helper(params, render_mesh, RenderPrimitiveMode::Triangles);
#endif
}

void AI::debug_draw_awk_nav_mesh(const RenderParams& params)
{
#if DEBUG
	refresh_nav_render_meshes(params);
	render_helper(params, awk_render_mesh, RenderPrimitiveMode::Lines);
#endif
}

b8 AI::vision_check(const Vec3& pos, const Vec3& enemy_pos, const Entity* a, const Entity* b)
{
	btCollisionWorld::ClosestRayResultCallback rayCallback(pos, enemy_pos);
	rayCallback.m_flags = btTriangleRaycastCallback::EFlags::kF_FilterBackfaces
		| btTriangleRaycastCallback::EFlags::kF_KeepUnflippedNormal;
	rayCallback.m_collisionFilterMask = rayCallback.m_collisionFilterGroup = ~CollisionTarget;

	Physics::btWorld->rayTest(pos, enemy_pos, rayCallback);

	if (!rayCallback.hasHit())
		return true;

	ID id = rayCallback.m_collisionObject->getUserIndex();
	return (a && id == a->id()) || (b && id == b->id());
}

Entity* AI::vision_query(const AIAgent* queryer, const Vec3& pos, const Vec3& forward, r32 radius, r32 angle, r32 max_height_diff, ComponentMask component_mask)
{
	r32 angle_dot = cosf(angle);
	for (auto i = AIAgent::list.iterator(); !i.is_last(); i.next())
	{
		AIAgent* agent = i.item();
		if (agent->team == queryer->team || !(agent->entity()->component_mask & component_mask))
			continue;

		Vec3 enemy_pos = agent->get<Transform>()->absolute_pos();

		Vec3 to_enemy = enemy_pos - pos;
		if (max_height_diff > 0.0f && to_enemy.y > max_height_diff)
			continue;

		r32 distance_to_enemy = to_enemy.length();

		if (distance_to_enemy < radius)
		{
			to_enemy /= distance_to_enemy;

			r32 dot = forward.dot(to_enemy);
			if (dot > angle_dot && vision_check(pos, enemy_pos, queryer->entity(), agent->entity()))
				return agent->entity();
		}
	}

	return nullptr;
}

Entity* AI::sound_query(AI::Team team, const Vec3& pos, ComponentMask component_mask)
{
	for (auto i = Shockwave::list.iterator(); !i.is_last(); i.next())
	{
		r32 radius = i.item()->radius();
		if ((i.item()->get<Transform>()->absolute_pos() - pos).length_squared() < radius * radius)
		{
			Entity* owner = i.item()->owner.ref();
			if (owner && (owner->component_mask & component_mask) && owner->has<AIAgent>())
			{
				AIAgent* agent = owner->get<AIAgent>();
				if (agent->team != team)
					return owner;
			}
		}
	}
	return nullptr;
}

dtPolyRef AI::get_poly(const Vec3& pos, const r32* search_extents)
{
	dtPolyRef result;

	AI::nav_mesh_query->findNearestPoly((r32*)&pos, search_extents, &default_query_filter, &result, 0);

	return result;
}

// Awk nav mesh stuff

void AwkNavMeshKey::reset(const AwkNavMesh& nav)
{
	data.chunk_size = nav.chunk_size;
	data.size.x = nav.size.x;
	data.size.y = nav.size.y;
	data.size.z = nav.size.z;
	data.vmin = nav.vmin;
	data.resize();
	for (s32 i = 0; i < data.chunks.length; i++)
	{
		data.chunks[i].resize(nav.chunks[i].vertices.length);
		memset(data.chunks[i].data, 0, sizeof(AwkNavMeshNodeData) * data.chunks[i].length);
	}
}

r32 AwkNavMeshKey::priority(const AwkNavMeshNode& a)
{
	const AwkNavMeshNodeData& data = get(a);
	return data.travel_score + data.estimate_score;
}

AwkNavMeshNodeData& AwkNavMeshKey::get(const AwkNavMeshNode& node)
{
	return data.chunks[node.chunk][node.vertex];
}

AwkNavMeshNode AI::awk_closest_point(const Vec3& p)
{
	AwkNavMesh::Coord chunk_coord = awk_nav_mesh->coord(p);
	r32 closest_distance = FLT_MAX;
	b8 found = false;
	AwkNavMeshNode closest;
	for (s32 chunk_x = vi_max(chunk_coord.x - 1, 0); chunk_x < vi_min(chunk_coord.x + 2, awk_nav_mesh->size.x); chunk_x++)
	{
		for (s32 chunk_y = vi_max(chunk_coord.y - 1, 0); chunk_y < vi_min(chunk_coord.y + 2, awk_nav_mesh->size.y); chunk_y++)
		{
			for (s32 chunk_z = vi_max(chunk_coord.z - 1, 0); chunk_z < vi_min(chunk_coord.z + 2, awk_nav_mesh->size.z); chunk_z++)
			{
				s32 chunk_index = awk_nav_mesh->index({ chunk_x, chunk_y, chunk_z });
				const AwkNavMeshChunk& chunk = awk_nav_mesh->chunks[chunk_index];
				for (s32 vertex_index = 0; vertex_index < chunk.vertices.length; vertex_index++)
				{
					const Vec3& vertex = chunk.vertices[vertex_index];
					r32 distance = (vertex - p).length_squared();
					if (distance < closest_distance)
					{
						closest_distance = distance;
						closest = { (u16)chunk_index, (u16)vertex_index };
						found = true;
					}
				}
			}
		}
	}
	vi_assert(found);
	return closest;
}

void AI::awk_pathfind(const Vec3& start, const Vec3& end, Array<Vec3>* path)
{
	path->length = 0;

	awk_nav_mesh_key.reset(*awk_nav_mesh);

	AwkNavMeshNode start_vertex = awk_closest_point(start);
	AwkNavMeshNode end_vertex = awk_closest_point(end);

	PriorityQueue<AwkNavMeshNode, AwkNavMeshKey> queue(&awk_nav_mesh_key);
	queue.push(start_vertex);

	AwkNavMeshNodeData& start_data = awk_nav_mesh_key.get(start_vertex);
	start_data.travel_score = 0;
	start_data.estimate_score = (end - start).length();
	start_data.visited = true;
	start_data.parent = { (u16)-1, (u16)-1 };

	while (queue.size() > 0)
	{
		AwkNavMeshNode node = queue.pop();

		if (node.equals(end_vertex))
		{
			// reconstruct path
			AwkNavMeshNode n = node;
			while (true)
			{
				if (n.equals(start_vertex))
					break;
				path->add(awk_nav_mesh->chunks[n.chunk].vertices[n.vertex]);
				n = awk_nav_mesh_key.get(n).parent;
			}
			break; // done!
		}

		AwkNavMeshNodeData& data = awk_nav_mesh_key.get(node);
		const Vec3& pos = awk_nav_mesh->chunks[node.chunk].vertices[node.vertex];
		
		const AwkNavMeshAdjacency& adjacency = awk_nav_mesh->chunks[node.chunk].adjacency[node.vertex];
		for (s32 i = 0; i < adjacency.length; i++)
		{
			// visit neighbors
			const AwkNavMeshNode& adjacent_node = adjacency[i];
			AwkNavMeshNodeData& adjacent_data = awk_nav_mesh_key.get(adjacent_node);
			const Vec3& adjacent_pos = awk_nav_mesh->chunks[adjacent_node.chunk].vertices[adjacent_node.vertex];
			r32 candidate_travel_score = data.travel_score + (adjacent_pos - pos).length();
			if (adjacent_data.visited)
			{
				if (adjacent_data.travel_score > candidate_travel_score)
				{
					adjacent_data.parent = node;
					adjacent_data.travel_score = candidate_travel_score;

					// since we've modified the score, if the node is already in queue,
					// we need to update its position in the queue
					for (s32 j = 0; j < queue.size(); j++)
					{
						if (queue.heap[j].equals(adjacent_node))
						{
							// remove it and re-add it
							queue.remove(j);
							queue.push(adjacent_node);
							break;
						}
					}
				}
			}
			else
			{
				// hasn't been visited yet
				adjacent_data.visited = true;
				adjacent_data.parent = node;
				adjacent_data.travel_score = candidate_travel_score;
				adjacent_data.estimate_score = (end - adjacent_pos).length();
				queue.push(adjacent_node);
			}
		}
	}
}


}