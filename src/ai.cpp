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
#include "recast/Detour/Include/DetourCommon.h"
#include "data/priority_queue.h"
#include "mersenne/mersenne-twister.h"
#include <Windows.h>

namespace VI
{

namespace AI
{


void NavMeshProcess::process(struct dtNavMeshCreateParams* params, u8* polyAreas, u16* polyFlags)
{
	for (int i = 0; i < params->polyCount; i++)
		polyFlags[i] = 1;
}

Team other(Team t)
{
	return t == Team::A ? Team::B : Team::A;
}

Array<b8> obstacles;
SyncRingBuffer<SYNC_IN_SIZE> sync_in;
SyncRingBuffer<SYNC_OUT_SIZE> sync_out;

void loop()
{
	Internal::loop();
}

void quit()
{
	sync_in.lock();
	sync_in.write(Op::Quit);
	sync_in.unlock();
}

void update(const Update& u)
{
	sync_out.lock();
	while (sync_out.can_read())
	{
		Callback cb;
		sync_out.read(&cb);
		switch (cb)
		{
			case Callback::Path:
			{
				LinkEntryArg<const Path&> link;
				sync_out.read(&link);
				Path path;
				sync_out.read(&path);
				(&link)->fire(path);
				break;
			}
			case Callback::AwkPath:
			{
				LinkEntryArg<const Path&> link;
				sync_out.read(&link);
				Path path;
				sync_out.read(&path);
				(&link)->fire(path);
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

b8 vision_check(const Vec3& pos, const Vec3& enemy_pos, const Entity* a, const Entity* b)
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

Entity* vision_query(const AIAgent* queryer, const Vec3& pos, const Vec3& forward, r32 radius, r32 angle, r32 max_height_diff, ComponentMask component_mask)
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

Entity* sound_query(Team team, const Vec3& pos, ComponentMask component_mask)
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

u32 obstacle_add(const Vec3& pos, r32 radius, r32 height)
{
	u32 id;
	b8 found = false;
	for (s32 i = 0; i < obstacles.length; i++)
	{
		if (!obstacles[i])
		{
			id = (u32)i;
			found = true;
			break;
		}
	}

	if (found)
		obstacles[id] = true;
	else
	{
		id = obstacles.length;
		obstacles.add(true);
	}

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
	obstacles[id] = false;
	sync_in.lock();
	sync_in.write(Op::ObstacleRemove);
	sync_in.write(id);
	sync_in.unlock();
}

void load(const u8* data, s32 length)
{
	sync_in.lock();
	sync_in.write(Op::Load);
	sync_in.write(data, length);
	sync_in.unlock();
}

void random_path(const Vec3& pos, const LinkEntryArg<const Path&>& callback)
{
	sync_in.lock();
	sync_in.write(Op::RandomPath);
	sync_in.write(pos);
	sync_in.write(callback);
	sync_in.unlock();
}

void awk_random_path(const Vec3& pos, const LinkEntryArg<const Path&>& callback)
{
	sync_in.lock();
	sync_in.write(Op::AwkRandomPath);
	sync_in.write(pos);
	sync_in.write(callback);
	sync_in.unlock();
}

void pathfind(const Vec3& a, const Vec3& b, const LinkEntryArg<const Path&>& callback)
{
	sync_in.lock();
	sync_in.write(Op::Pathfind);
	sync_in.write(a);
	sync_in.write(b);
	sync_in.write(callback);
	sync_in.unlock();
}

void awk_pathfind(const Vec3& a, const Vec3& b, const LinkEntryArg<const Path&>& callback)
{
	sync_in.lock();
	sync_in.write(Op::AwkPathfind);
	sync_in.write(a);
	sync_in.write(b);
	sync_in.write(callback);
	sync_in.unlock();
}

namespace Internal
{


dtNavMesh* nav_mesh = nullptr;
AwkNavMesh awk_nav_mesh;
AwkNavMeshKey awk_nav_mesh_key;
dtTileCache* nav_tile_cache = nullptr;
dtTileCacheAlloc nav_tile_allocator;
FastLZCompressor nav_tile_compressor;
NavMeshProcess nav_tile_mesh_process;
dtNavMeshQuery* nav_mesh_query = nullptr;
dtQueryFilter default_query_filter = dtQueryFilter();
const r32 default_search_extents[] = { 8, 8, 8 };

void pathfind(const Vec3& a, const Vec3& b, dtPolyRef start_poly, dtPolyRef end_poly, Path* path)
{
	dtPolyRef path_polys[AI::MAX_PATH_LENGTH];
	dtPolyRef path_parents[AI::MAX_PATH_LENGTH];
	u8 path_straight_flags[AI::MAX_PATH_LENGTH];
	dtPolyRef path_straight_polys[AI::MAX_PATH_LENGTH];
	s32 path_poly_count;

	nav_mesh_query->findPath(start_poly, end_poly, (r32*)&a, (r32*)&b, &default_query_filter, path_polys, &path_poly_count, MAX_PATH_LENGTH);
	if (path_poly_count)
	{
		// In case of partial path, make sure the end point is clamped to the last polygon.
		Vec3 epos = b;
		if (path_polys[path_poly_count - 1] != end_poly)
			nav_mesh_query->closestPointOnPoly(path_polys[path_poly_count - 1], (r32*)&b, (r32*)&epos, 0);

		s32 point_count;
		nav_mesh_query->findStraightPath((r32*)&a, (r32*)&b, path_polys, path_poly_count,
			(r32*)path->data, path_straight_flags,
			path_straight_polys, &point_count, MAX_PATH_LENGTH, 0);
		path->length = point_count;
	}
}

void loop()
{
	nav_mesh_query = dtAllocNavMeshQuery();
	default_query_filter.setIncludeFlags((u16)-1);
	default_query_filter.setExcludeFlags(0);

	Array<u32> obstacle_recast_ids;

	b8 run = true;
	while (run)
	{
		sync_in.lock_wait_read();
		Op op;
		sync_in.read(&op);
		switch (op)
		{
			case Op::Load:
			{
				// free old data if necessary
				{
					if (nav_mesh)
					{
						dtFreeNavMesh(nav_mesh);
						nav_mesh = nullptr;
					}
					if (nav_tile_cache)
					{
						dtFreeTileCache(nav_tile_cache);
						nav_tile_cache = nullptr;
					}
					awk_nav_mesh.~AwkNavMesh();
					memset(&awk_nav_mesh, 0, sizeof(AwkNavMesh));
				}

				TileCacheData tiles;

				{
					// read tile data
					sync_in.read(&tiles.min);
					sync_in.read(&tiles.width);
					sync_in.read(&tiles.height);
					tiles.cells.resize(tiles.width * tiles.height);
					for (s32 i = 0; i < tiles.cells.length; i++)
					{
						TileCacheCell& cell = tiles.cells[i];
						s32 layer_count;
						sync_in.read(&layer_count);
						cell.layers.resize(layer_count);
						for (s32 j = 0; j < layer_count; j++)
						{
							TileCacheLayer& layer = cell.layers[j];
							sync_in.read(&layer.data_size);
							layer.data = (u8*)dtAlloc(layer.data_size, dtAllocHint::DT_ALLOC_PERM);
							sync_in.read<u8>(layer.data, layer.data_size);
						}
					}
				}

				{
					// Create Detour navmesh

					nav_mesh = dtAllocNavMesh();
					vi_assert(nav_mesh);

					dtNavMeshParams params;
					memset(&params, 0, sizeof(params));
					rcVcopy(params.orig, (r32*)&tiles.min);
					params.tileWidth = nav_tile_size * nav_resolution;
					params.tileHeight = nav_tile_size * nav_resolution;

					s32 tileBits = rcMin((s32)dtIlog2(dtNextPow2(tiles.width * tiles.height * nav_expected_layers_per_tile)), 14);
					if (tileBits > 14) tileBits = 14;
					s32 polyBits = 22 - tileBits;
					params.maxTiles = 1 << tileBits;
					params.maxPolys = 1 << polyBits;

					{
						dtStatus status = nav_mesh->init(&params);
						vi_assert(dtStatusSucceed(status));
					}

					// Create Detour tile cache

					dtTileCacheParams tcparams;
					memset(&tcparams, 0, sizeof(tcparams));
					memcpy(&tcparams.orig, &tiles.min, sizeof(tiles.min));
					tcparams.cs = nav_resolution;
					tcparams.ch = nav_resolution;
					tcparams.width = (s32)nav_tile_size;
					tcparams.height = (s32)nav_tile_size;
					tcparams.walkableHeight = nav_agent_height;
					tcparams.walkableRadius = nav_agent_radius;
					tcparams.walkableClimb = nav_agent_max_climb;
					tcparams.maxSimplificationError = nav_mesh_max_error;
					tcparams.maxTiles = tiles.width * tiles.height * nav_expected_layers_per_tile;
					tcparams.maxObstacles = nav_max_obstacles;

					nav_tile_cache = dtAllocTileCache();
					vi_assert(nav_tile_cache);
					{
						dtStatus status = nav_tile_cache->init(&tcparams, &nav_tile_allocator, &nav_tile_compressor, &nav_tile_mesh_process);
						vi_assert(!dtStatusFailed(status));
					}

					for (s32 ty = 0; ty < tiles.height; ty++)
					{
						for (s32 tx = 0; tx < tiles.width; tx++)
						{
							TileCacheCell& cell = tiles.cells[tx + ty * tiles.width];
							for (s32 i = 0; i < cell.layers.length; i++)
							{
								TileCacheLayer& tile = cell.layers[i];
								dtStatus status = nav_tile_cache->addTile(tile.data, tile.data_size, DT_COMPRESSEDTILE_FREE_DATA, 0);
								vi_assert(dtStatusSucceed(status));
							}
						}
					}

					// Build initial meshes
					for (s32 ty = 0; ty < tiles.height; ty++)
					{
						for (s32 tx = 0; tx < tiles.width; tx++)
						{
							dtStatus status = nav_tile_cache->buildNavMeshTilesAt(tx, ty, nav_mesh);
							vi_assert(dtStatusSucceed(status));
						}
					}

					dtStatus status = nav_mesh_query->init(nav_mesh, 2048);
					vi_assert(dtStatusSucceed(status));
				}

				// Awk nav mesh
				{
					r32 chunk_size;
					sync_in.read(&chunk_size);
					new (&awk_nav_mesh) AwkNavMesh();

					awk_nav_mesh.chunk_size = chunk_size;
					sync_in.read(&awk_nav_mesh.vmin);
					sync_in.read(&awk_nav_mesh.size);
					awk_nav_mesh.resize();

					for (s32 i = 0; i < awk_nav_mesh.chunks.length; i++)
					{
						AwkNavMeshChunk* chunk = &awk_nav_mesh.chunks[i];
						s32 vertex_count;
						sync_in.read(&vertex_count);
						chunk->vertices.resize(vertex_count);
						sync_in.read(chunk->vertices.data, vertex_count);
						chunk->adjacency.resize(vertex_count);
						sync_in.read(chunk->adjacency.data, vertex_count);
					}
				}

				awk_nav_mesh_key.reset(awk_nav_mesh);

				break;
			}
			case Op::ObstacleAdd:
			{
				u32 id;
				sync_in.read(&id);
				if ((s32)id > obstacle_recast_ids.length - 1)
					obstacle_recast_ids.resize(id + 1);

				vi_assert(nav_tile_cache);
				u32 recast_id;
				Vec3 pos;
				sync_in.read(&pos);
				r32 radius;
				sync_in.read(&radius);
				r32 height;
				sync_in.read(&height);
				dtStatus status = nav_tile_cache->addObstacle((r32*)&pos, radius, height, &recast_id);
				obstacle_recast_ids[id] = recast_id;

				nav_tile_cache->update(0.0f, nav_mesh); // todo: batch obstacle API calls together
				break;
			}
			case Op::ObstacleRemove:
			{
				u32 id;
				sync_in.read(&id);
				u32 recast_id = obstacle_recast_ids[id];
				nav_tile_cache->removeObstacle(recast_id);

				nav_tile_cache->update(0.0f, nav_mesh); // todo: batch obstacle API calls together
				break;
			}
			case Op::Pathfind:
			{
				Vec3 a;
				Vec3 b;
				sync_in.read(&a);
				sync_in.read(&b);
				LinkEntryArg<Path> callback;
				sync_in.read(&callback);

				dtPolyRef start_poly = get_poly(a, default_search_extents);
				dtPolyRef end_poly = get_poly(b, default_search_extents);

				Path path;

				if (start_poly && end_poly)
					pathfind(a, b, start_poly, end_poly, &path);
				
				sync_out.lock();
				sync_out.write(Callback::Path);
				sync_out.write(callback);
				sync_out.write(path);
				sync_out.unlock();

				break;
			}
			case Op::RandomPath:
			{
				Vec3 start;
				sync_in.read(&start);
				dtPolyRef start_poly = get_poly(start, default_search_extents);

				LinkEntryArg<Path> callback;
				sync_in.read(&callback);

				Vec3 end;
				dtPolyRef end_poly;
				Vec3 target;
				nav_mesh_query->findRandomPoint(&default_query_filter, mersenne::randf_co, &end_poly, (r32*)&end);

				Path path;

				if (start_poly && end_poly)
					pathfind(start, end, start_poly, end_poly, &path);

				sync_out.lock();
				sync_out.write(Callback::Path);
				sync_out.write(callback);
				sync_out.write(path);
				sync_out.unlock();

				break;
			}
			case Op::AwkPathfind:
			{
				Vec3 a;
				Vec3 b;
				sync_in.read(&a);
				sync_in.read(&b);
				LinkEntryArg<Path> callback;
				sync_in.read(&callback);

				Path path;
				awk_pathfind(a, b, &path);

				sync_out.lock();
				sync_out.write(Callback::AwkPath);
				sync_out.write(callback);
				sync_out.write(path);
				sync_out.unlock();
				break;
			}
			case Op::AwkRandomPath:
			{
				LinkEntryArg<Vec3> callback;
				sync_in.read(&callback);

				Array<s32> chunks_with_vertices(awk_nav_mesh.chunks.length);
				for (s32 i = 0; i < awk_nav_mesh.chunks.length; i++)
				{
					if (awk_nav_mesh.chunks[i].vertices.length > 0)
						chunks_with_vertices.add(i);
				}
				s32 chunk_index = chunks_with_vertices[mersenne::randf_co() * chunks_with_vertices.length];
				const AwkNavMeshChunk& chunk = awk_nav_mesh.chunks[chunk_index];
				const Vec3& point = chunk.vertices[mersenne::randf_co() * chunk.vertices.length - 1];

				sync_out.lock();
				sync_out.write(Callback::AwkPath);
				sync_out.write(callback);
				sync_out.write(point);
				sync_out.unlock();
				break;
			}
			case Op::Quit:
			{
				run = false;
				break;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}
		sync_in.unlock();
	}
}

// todo: re-enable AI debug visualizations
/*
#if DEBUG
void refresh_nav_render_meshes(const RenderParams& params)
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

		params.sync_in->write(RenderOp::UpdateAttribBuffers);
		params.sync_in->write(render_mesh);

		params.sync_in->write<s32>(vertices.length);
		params.sync_in->write<Vec3>(vertices.data, vertices.length);

		params.sync_in->write(RenderOp::UpdateIndexBuffer);
		params.sync_in->write(render_mesh);

		params.sync_in->write<s32>(indices.length);
		params.sync_in->write<s32>(indices.data, indices.length);
	}

	vertices.length = 0;
	indices.length = 0;

	// awk nav mesh
	{
		s32 vertex_count = 0;
		for (s32 chunk_index = 0; chunk_index < awk_nav_mesh.chunks.length; chunk_index++)
		{
			const AwkNavMeshChunk& chunk = awk_nav_mesh.chunks[chunk_index];

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
						neighbor_chunk_vertex_index += awk_nav_mesh.chunks[k].vertices.length;
					indices.add(neighbor_chunk_vertex_index + neighbor.vertex);
				}
			}

			vertex_count += chunk.vertices.length;
		}

		params.sync_in->write(RenderOp::UpdateAttribBuffers);
		params.sync_in->write(awk_render_mesh);

		params.sync_in->write<s32>(vertices.length);
		params.sync_in->write<Vec3>(vertices.data, vertices.length);

		params.sync_in->write(RenderOp::UpdateIndexBuffer);
		params.sync_in->write(awk_render_mesh);

		params.sync_in->write<s32>(indices.length);
		params.sync_in->write<s32>(indices.data, indices.length);
	}
}

void render_helper(const RenderParams& params, AssetID m, RenderPrimitiveMode primitive_mode)
{
	params.sync_in->write(RenderOp::Shader);
	params.sync_in->write(Asset::Shader::flat);
	params.sync_in->write(params.technique);

	params.sync_in->write(RenderOp::Uniform);
	params.sync_in->write(Asset::Uniform::diffuse_color);
	params.sync_in->write(RenderDataType::Vec4);
	params.sync_in->write<s32>(1);
	params.sync_in->write(Vec4(0, 1, 0, 0.5f));

	Mat4 mvp = params.view_projection;

	params.sync_in->write(RenderOp::Uniform);
	params.sync_in->write(Asset::Uniform::mvp);
	params.sync_in->write(RenderDataType::Mat4);
	params.sync_in->write<s32>(1);
	params.sync_in->write<Mat4>(mvp);

	params.sync_in->write(RenderOp::FillMode);
	params.sync_in->write(RenderFillMode::Point);
	params.sync_in->write(RenderOp::PointSize);
	params.sync_in->write<r32>(4 * UI::scale);
	params.sync_in->write(RenderOp::CullMode);
	params.sync_in->write(RenderCullMode::None);

	params.sync_in->write(RenderOp::Mesh);
	params.sync_in->write(primitive_mode);
	params.sync_in->write(m);

	params.sync_in->write(RenderOp::FillMode);
	params.sync_in->write(RenderFillMode::Fill);
	params.sync_in->write(RenderOp::CullMode);
	params.sync_in->write(RenderCullMode::Back);
}
#endif
*/

dtPolyRef get_poly(const Vec3& pos, const r32* search_extents)
{
	dtPolyRef result;

	nav_mesh_query->findNearestPoly((r32*)&pos, search_extents, &default_query_filter, &result, 0);

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

AwkNavMeshNode awk_closest_point(const Vec3& p)
{
	AwkNavMesh::Coord chunk_coord = awk_nav_mesh.coord(p);
	r32 closest_distance = FLT_MAX;
	b8 found = false;
	AwkNavMeshNode closest;
	for (s32 chunk_x = vi_max(chunk_coord.x - 1, 0); chunk_x < vi_min(chunk_coord.x + 2, awk_nav_mesh.size.x); chunk_x++)
	{
		for (s32 chunk_y = vi_max(chunk_coord.y - 1, 0); chunk_y < vi_min(chunk_coord.y + 2, awk_nav_mesh.size.y); chunk_y++)
		{
			for (s32 chunk_z = vi_max(chunk_coord.z - 1, 0); chunk_z < vi_min(chunk_coord.z + 2, awk_nav_mesh.size.z); chunk_z++)
			{
				s32 chunk_index = awk_nav_mesh.index({ chunk_x, chunk_y, chunk_z });
				const AwkNavMeshChunk& chunk = awk_nav_mesh.chunks[chunk_index];
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

void awk_pathfind(const Vec3& start, const Vec3& end, Path* path)
{
	path->length = 0;

	awk_nav_mesh_key.reset(awk_nav_mesh);

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
				path->add(awk_nav_mesh.chunks[n.chunk].vertices[n.vertex]);
				n = awk_nav_mesh_key.get(n).parent;
			}
			break; // done!
		}

		AwkNavMeshNodeData& data = awk_nav_mesh_key.get(node);
		const Vec3& pos = awk_nav_mesh.chunks[node.chunk].vertices[node.vertex];
		
		const AwkNavMeshAdjacency& adjacency = awk_nav_mesh.chunks[node.chunk].adjacency[node.vertex];
		for (s32 i = 0; i < adjacency.length; i++)
		{
			// visit neighbors
			const AwkNavMeshNode& adjacent_node = adjacency[i];
			AwkNavMeshNodeData& adjacent_data = awk_nav_mesh_key.get(adjacent_node);
			const Vec3& adjacent_pos = awk_nav_mesh.chunks[adjacent_node.chunk].vertices[adjacent_node.vertex];
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

}


}