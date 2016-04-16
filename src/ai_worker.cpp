#include "ai.h"
#include "data/import_common.h"
#include "recast/Recast/Include/Recast.h"
#include "recast/Detour/Include/DetourNavMeshBuilder.h"
#include "recast/Detour/Include/DetourCommon.h"
#include "data/priority_queue.h"
#include "mersenne/mersenne-twister.h"

namespace VI
{

namespace AI
{

namespace Internal
{


void NavMeshProcess::process(struct dtNavMeshCreateParams* params, u8* polyAreas, u16* polyFlags)
{
	for (int i = 0; i < params->polyCount; i++)
		polyFlags[i] = 1;
}

dtNavMesh* nav_mesh = nullptr;
AwkNavMesh awk_nav_mesh;
AwkNavMeshKey awk_nav_mesh_key;
dtTileCache* nav_tile_cache = nullptr;
dtTileCacheAlloc nav_tile_allocator;
FastLZCompressor nav_tile_compressor;
NavMeshProcess nav_tile_mesh_process;
dtNavMeshQuery* nav_mesh_query = nullptr;
dtQueryFilter default_query_filter = dtQueryFilter();
const r32 default_search_extents[] = { 20, 50, 20 };

void pathfind(const Vec3& a, const Vec3& b, dtPolyRef start_poly, dtPolyRef end_poly, Path* path)
{
	dtPolyRef path_polys[AI::MAX_PATH_LENGTH];
	dtPolyRef path_parents[AI::MAX_PATH_LENGTH];
	u8 path_straight_flags[AI::MAX_PATH_LENGTH];
	dtPolyRef path_straight_polys[AI::MAX_PATH_LENGTH];
	s32 path_poly_count;

	nav_mesh_query->findPath(start_poly, end_poly, (r32*)&a, (r32*)&b, &default_query_filter, path_polys, &path_poly_count, MAX_PATH_LENGTH);
	if (path_poly_count == 0)
		path->length = 0;
	else
	{
		// In case of partial path, make sure the end point is clamped to the last polygon.
		Vec3 end;
		if (path_polys[path_poly_count - 1] == end_poly)
			end = b;
		else
			nav_mesh_query->closestPointOnPoly(path_polys[path_poly_count - 1], (r32*)&b, (r32*)&end, 0);

		nav_mesh_query->findStraightPath
		(
			(const r32*)&a, (const r32*)&end, path_polys, path_poly_count,
			(r32*)path->data, path_straight_flags,
			path_straight_polys, &path->length, MAX_PATH_LENGTH, 0
		);
		if (path->length > 1)
			path->remove_ordered(0);
	}
}

void loop()
{
	nav_mesh_query = dtAllocNavMeshQuery();
	default_query_filter.setIncludeFlags((u16)-1);
	default_query_filter.setExcludeFlags(0);

	Array<u32> obstacle_recast_ids;

	b8 run = true;
	Op op;
	while (run)
	{
		sync_in.lock_wait_read();
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
					new (&awk_nav_mesh) AwkNavMesh();
				}

				s32 data_length;
				sync_in.read(&data_length);
				if (data_length > 0)
				{
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
						sync_in.read(&awk_nav_mesh.chunk_size);
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
				}

				awk_nav_mesh_key.reset(awk_nav_mesh);

				sync_in.unlock();

				break;
			}
			case Op::ObstacleAdd:
			{
				u32 id;
				sync_in.read(&id);
				u32 recast_id;
				Vec3 pos;
				sync_in.read(&pos);
				r32 radius;
				sync_in.read(&radius);
				r32 height;
				sync_in.read(&height);
				sync_in.unlock();

				if ((s32)id > obstacle_recast_ids.length - 1)
					obstacle_recast_ids.resize(id + 1);
				vi_assert(nav_tile_cache);
				dtStatus status = nav_tile_cache->addObstacle((r32*)&pos, radius, height, &recast_id);
				obstacle_recast_ids[id] = recast_id;

				nav_tile_cache->update(0.0f, nav_mesh); // todo: batch obstacle API calls together
				break;
			}
			case Op::ObstacleRemove:
			{
				u32 id;
				sync_in.read(&id);
				sync_in.unlock();

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
				sync_in.unlock();

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
				LinkEntryArg<Path> callback;
				sync_in.read(&callback);
				sync_in.unlock();

				dtPolyRef start_poly = get_poly(start, default_search_extents);

				Vec3 end;
				dtPolyRef end_poly;
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
				sync_in.unlock();

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
				LinkEntryArg<Path> callback;
				Vec3 start;
				sync_in.read(&start);
				sync_in.read(&callback);
				sync_in.unlock();

				Array<s32> chunks_with_vertices(awk_nav_mesh.chunks.length);
				for (s32 i = 0; i < awk_nav_mesh.chunks.length; i++)
				{
					if (awk_nav_mesh.chunks[i].vertices.length > 0)
						chunks_with_vertices.add(i);
				}
				s32 chunk_index = chunks_with_vertices[mersenne::randf_co() * chunks_with_vertices.length];
				const AwkNavMeshChunk& chunk = awk_nav_mesh.chunks[chunk_index];
				const Vec3& end = chunk.vertices[mersenne::randf_co() * chunk.vertices.length - 1];

				Path path;
				awk_pathfind(start, end, &path);

				sync_out.lock();
				sync_out.write(Callback::AwkPath);
				sync_out.write(callback);
				sync_out.write(path);
				sync_out.unlock();
				break;
			}
			case Op::Quit:
			{
				sync_in.unlock();
				run = false;
				break;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}
	}
}

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
					const AwkNavMeshAdjacency& adjacency = chunk.adjacency[vertex_index];
					if (adjacency.length > 0) // ignore orphans
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

	{
		AwkNavMeshNodeData* start_data = &awk_nav_mesh_key.get(start_vertex);
		start_data->travel_score = 0;
		start_data->estimate_score = (end - start).length();
		start_data->visited = true;
		start_data->parent = { (u16)-1, (u16)-1 };
	}

	while (queue.size() > 0)
	{
		AwkNavMeshNode vertex_node = queue.pop();

		if (vertex_node.equals(end_vertex))
		{
			// reconstruct path
			AwkNavMeshNode n = vertex_node;
			while (true)
			{
				if (n.equals(start_vertex))
					break;
				path->insert(0, awk_nav_mesh.chunks[n.chunk].vertices[n.vertex]);
				n = awk_nav_mesh_key.get(n).parent;
			}
			break; // done!
		}

		AwkNavMeshNodeData* vertex_data = &awk_nav_mesh_key.get(vertex_node);
		const Vec3& vertex_pos = awk_nav_mesh.chunks[vertex_node.chunk].vertices[vertex_node.vertex];
		
		const AwkNavMeshAdjacency& adjacency = awk_nav_mesh.chunks[vertex_node.chunk].adjacency[vertex_node.vertex];
		for (s32 i = 0; i < adjacency.length; i++)
		{
			// visit neighbors
			const AwkNavMeshNode& adjacent_node = adjacency[i];
			AwkNavMeshNodeData* adjacent_data = &awk_nav_mesh_key.get(adjacent_node);
			const Vec3& adjacent_pos = awk_nav_mesh.chunks[adjacent_node.chunk].vertices[adjacent_node.vertex];
			r32 candidate_travel_score = vertex_data->travel_score + (adjacent_pos - vertex_pos).length();
			if (adjacent_data->visited)
			{
				if (adjacent_data->travel_score > candidate_travel_score)
				{
					// since we've modified the score, if the node is already in queue,
					// we need to update its position in the queue
					adjacent_data->parent = vertex_node;
					adjacent_data->travel_score = candidate_travel_score;
					for (s32 j = 0; j < queue.size(); j++)
					{
						if (queue.heap[j].equals(adjacent_node))
						{
							// it's in the queue; update its position due to the score change
							queue.update(j);
							break;
						}
					}
				}
			}
			else
			{
				// hasn't been visited yet
				adjacent_data->visited = true;
				adjacent_data->parent = vertex_node;
				adjacent_data->travel_score = candidate_travel_score;
				adjacent_data->estimate_score = (end - adjacent_pos).length();
				queue.push(adjacent_node);
			}
		}
	}
}

}

}


}