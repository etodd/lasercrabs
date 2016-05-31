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

namespace Worker
{


dtPolyRef get_poly(const Vec3& pos, const r32* search_extents)
{
	dtPolyRef result;

	nav_mesh_query->findNearestPoly((r32*)&pos, search_extents, &default_query_filter, &result, 0);

	return result;
}

struct AstarScorer
{
	// calculate heuristic score for nav mesh vertex
	virtual r32 score(const Vec3&) const = 0;
	// did we find what we're looking for?
	virtual b8 done(AwkNavMeshNode, const AwkNavMeshNodeData&) const = 0;
};

// pathfind to a target vertex
struct PathfindScorer : AstarScorer
{
	AwkNavMeshNode end_vertex;
	Vec3 end_pos;

	virtual r32 score(const Vec3& pos) const
	{
		return (end_pos - pos).length();
	}

	virtual b8 done(AwkNavMeshNode v, const AwkNavMeshNodeData&) const
	{
		return v.equals(end_vertex);
	}
};

// run away from an enemy
struct AwayScorer : AstarScorer
{
	AwkNavMeshNode start_vertex;
	AwkNavMeshNode away_vertex;
	Vec3 away_pos;

	virtual r32 score(const Vec3& pos) const
	{
		return AWK_MAX_DISTANCE - (away_pos - pos).length();
	}

	virtual b8 done(AwkNavMeshNode v, const AwkNavMeshNodeData& data) const
	{
		if (v.equals(start_vertex)) // we need to go somewhere other than here
			return false;

		if (data.sensor_score == 0.0f) // inside a friendly sensor zone
			return true;

		const AwkNavMeshAdjacency& adjacency = awk_nav_mesh.chunks[away_vertex.chunk].adjacency[away_vertex.vertex];
		for (s32 i = 0; i < adjacency.length; i++)
		{
			if (adjacency[i].equals(v))
				return false;
		}
		return true; // the enemy can't get there, it's safe
	}
};

AwkNavMeshNode awk_closest_point(const Vec3& p, const Vec3& normal)
{
	AwkNavMesh::Coord chunk_coord = awk_nav_mesh.coord(p);
	r32 closest_distance = FLT_MAX;
	b8 found = false;
	AwkNavMeshNode closest;
	b8 ignore_normals = normal.dot(normal) == 0.0f;
	s32 end_x = vi_min(vi_max(chunk_coord.x + 2, 1), awk_nav_mesh.size.x);
	for (s32 chunk_x = vi_min(vi_max(chunk_coord.x - 1, 0), awk_nav_mesh.size.x - 1); chunk_x < end_x; chunk_x++)
	{
		s32 end_y = vi_min(vi_max(chunk_coord.y + 2, 1), awk_nav_mesh.size.y);
		for (s32 chunk_y = vi_min(vi_max(chunk_coord.y - 1, 0), awk_nav_mesh.size.y - 1); chunk_y < end_y; chunk_y++)
		{
			s32 end_z = vi_min(vi_max(chunk_coord.z + 2, 1), awk_nav_mesh.size.z);
			for (s32 chunk_z = vi_min(vi_max(chunk_coord.z - 1, 0), awk_nav_mesh.size.z - 1); chunk_z < end_z; chunk_z++)
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
							const Vec3& vertex_normal = chunk.normals[vertex_index];
							if (ignore_normals || normal.dot(vertex_normal) > 0.8f) // make sure it's roughly facing the right way
							{
								closest_distance = distance;
								closest = { (u16)chunk_index, (u16)vertex_index };
								found = true;
							}
							else if (!found) // the normal is wrong, but we'll use it in an emergency
							{
								closest = { (u16)chunk_index, (u16)vertex_index };
								found = true;
							}
						}
					}
				}
			}
		}
	}
	vi_assert(found);
	return closest;
}

// can we hit the target from the given nav mesh node?
b8 can_hit_from(AwkNavMeshNode start_vertex, const Vec3& target, r32 dot_threshold, r32* closest_dot = nullptr)
{
	const Vec3& start = awk_nav_mesh.chunks[start_vertex.chunk].vertices[start_vertex.vertex];
	Vec3 to_target = target - start;
	r32 target_distance_squared = to_target.length_squared();
	to_target /= sqrtf(target_distance_squared); // normalize
	const AwkNavMeshAdjacency& start_adjacency = awk_nav_mesh.chunks[start_vertex.chunk].adjacency[start_vertex.vertex];

	for (s32 i = 0; i < start_adjacency.length; i++)
	{
		const AwkNavMeshNode adjacent_vertex = start_adjacency[i];
		const Vec3& adjacent = awk_nav_mesh.chunks[adjacent_vertex.chunk].vertices[adjacent_vertex.vertex];

		Vec3 to_adjacent = adjacent - start;
		r32 adjacent_distance_squared = to_adjacent.length_squared();
		if (adjacent_distance_squared > target_distance_squared) // make sure the target is in between us and the adjacent vertex
		{
			to_adjacent /= sqrtf(adjacent_distance_squared); // normalize
			r32 dot = to_adjacent.dot(to_target);
			if (dot > dot_threshold) // make sure the target is lined up with us and the adjacent vertex
			{
				if (closest_dot)
					*closest_dot = dot;
				return true;
			}
		}
	}
	return false;
}

Array<SensorState> sensors;

r32 sensor_cost(AI::Team team, const AwkNavMeshNode& node)
{
	const Vec3& pos = awk_nav_mesh.chunks[node.chunk].vertices[node.vertex];
	const Vec3& normal = awk_nav_mesh.chunks[node.chunk].normals[node.vertex];
	b8 in_friendly_zone = false;
	b8 in_enemy_zone = false;
	for (s32 i = 0; i < sensors.length; i++)
	{
		Vec3 to_sensor = sensors[i].pos - pos;
		if (to_sensor.length_squared() < SENSOR_RANGE * SENSOR_RANGE)
		{
			if (normal.dot(to_sensor) > 0.0f)
			{
				if (sensors[i].team == team)
					in_friendly_zone = true;
				else
				{
					in_enemy_zone = true;
					break;
				}
			}
		}
	}
	if (in_enemy_zone)
		return 24.0f;
	else if (in_friendly_zone)
		return 0;
	else
		return 8.0f;
}

// A*
void awk_astar(AI::Team team, const AwkNavMeshNode& start_vertex, const AstarScorer* scorer, Path* path)
{
	path->length = 0;

	const Vec3& start_pos = awk_nav_mesh.chunks[start_vertex.chunk].vertices[start_vertex.vertex];

	awk_nav_mesh_key.reset(awk_nav_mesh);

	PriorityQueue<AwkNavMeshNode, AwkNavMeshKey> queue(&awk_nav_mesh_key);
	queue.push(start_vertex);

	{
		AwkNavMeshNodeData* start_data = &awk_nav_mesh_key.get(start_vertex);
		start_data->travel_score = 0;
		start_data->estimate_score = scorer->score(start_pos);
		start_data->sensor_score = sensor_cost(team, start_vertex);
		start_data->parent = { (u16)-1, (u16)-1 };
	}

	while (queue.size() > 0)
	{
		AwkNavMeshNode vertex_node = queue.pop();

		AwkNavMeshNodeData* vertex_data = &awk_nav_mesh_key.get(vertex_node);

		if (scorer->done(vertex_node, *vertex_data))
		{
			// reconstruct path
			AwkNavMeshNode n = vertex_node;
			while (true)
			{
				path->insert(0, awk_nav_mesh.chunks[n.chunk].vertices[n.vertex]);
				if (n.equals(start_vertex))
					break;
				n = awk_nav_mesh_key.get(n).parent;
			}
			break; // done!
		}

		vertex_data->visited = true;
		const Vec3& vertex_pos = awk_nav_mesh.chunks[vertex_node.chunk].vertices[vertex_node.vertex];
		
		const AwkNavMeshAdjacency& adjacency = awk_nav_mesh.chunks[vertex_node.chunk].adjacency[vertex_node.vertex];
		for (s32 i = 0; i < adjacency.length; i++)
		{
			// visit neighbors
			const AwkNavMeshNode adjacent_node = adjacency[i];
			AwkNavMeshNodeData* adjacent_data = &awk_nav_mesh_key.get(adjacent_node);

			if (!adjacent_data->visited)
			{
				// hasn't been visited yet; check if this node is already in the queue
				s32 existing_queue_index = -1;
				for (s32 j = 0; j < queue.size(); j++)
				{
					if (queue.heap[j].equals(adjacent_node))
					{
						existing_queue_index = j;
						break;
					}
				}

				const Vec3& adjacent_pos = awk_nav_mesh.chunks[adjacent_node.chunk].vertices[adjacent_node.vertex];

				r32 candidate_travel_score = vertex_data->travel_score
					+ vertex_data->sensor_score
					+ (adjacent_pos - vertex_pos).length()
					+ 10.0f; // bias toward fewer, longer shots

				if (existing_queue_index == -1)
				{
					// totally new node
					adjacent_data->parent = vertex_node;
					adjacent_data->sensor_score = sensor_cost(team, adjacent_node);
					adjacent_data->travel_score = candidate_travel_score;
					adjacent_data->estimate_score = scorer->score(adjacent_pos);
					queue.push(adjacent_node);
				}
				else
				{
					// it's already in the queue
					if (candidate_travel_score < adjacent_data->travel_score)
					{
						// this is a better path
						adjacent_data->parent = vertex_node;
						adjacent_data->travel_score = candidate_travel_score;
						// update its position in the queue due to the score change
						queue.update(existing_queue_index);
					}
				}
			}
		}
	}
}

// find a path from vertex a to vertex b
void awk_pathfind_internal(AI::Team team, const AwkNavMeshNode& start_vertex, const AwkNavMeshNode& end_vertex, Path* path)
{
	PathfindScorer scorer;
	scorer.end_vertex = end_vertex;
	scorer.end_pos = awk_nav_mesh.chunks[end_vertex.chunk].vertices[end_vertex.vertex];
	awk_astar(team, start_vertex, &scorer, path);
}

// find a path using vertices as close as possible to the given points
// find our way to a point from which we can shoot through the given target
void awk_pathfind_hit(AI::Team team, const Vec3& start, const Vec3& start_normal, const Vec3& target, Path* path)
{
	AwkNavMeshNode target_closest_vertex = awk_closest_point(target, Vec3::zero);

	AwkNavMeshNode start_vertex = awk_closest_point(start, start_normal);

	// even if we are supposed to be able to hit the target from the start vertex, don't just return a 0-length path.
	// find another vertex where we can hit the target
	// if we actually CAN hit the target, the low-level AI will take care of it.
	// if not, we'll have a path to another vertex where we can hopefully hit the target
	// this prevents us from getting stuck at a point where we think we should be able to hit the target, but we actually can't

	if (!target_closest_vertex.equals(start_vertex) && can_hit_from(target_closest_vertex, target, 0.999f))
		awk_pathfind_internal(team, start_vertex, target_closest_vertex, path);
	else
	{
		const AwkNavMeshAdjacency& target_adjacency = awk_nav_mesh.chunks[target_closest_vertex.chunk].adjacency[target_closest_vertex.vertex];
		r32 closest_distance = AWK_MAX_DISTANCE;
		r32 closest_dot = 0.7f;
		AwkNavMeshNode closest_vertex;
		for (s32 i = 0; i < target_adjacency.length; i++)
		{
			const AwkNavMeshNode adjacent_vertex = target_adjacency[i];
			if (!adjacent_vertex.equals(start_vertex))
			{
				const Vec3& adjacent = awk_nav_mesh.chunks[adjacent_vertex.chunk].vertices[adjacent_vertex.vertex];
				r32 distance = (adjacent - start).length_squared();
				r32 dot = 0.0f;
				can_hit_from(adjacent_vertex, target, 0.99f, &dot);
				if ((dot > 0.999f && distance > closest_distance) || (closest_dot < 0.999f && dot > closest_dot))
				{
					closest_distance = distance;
					closest_dot = dot;
					closest_vertex = adjacent_vertex;
				}
			}
		}
		if (closest_distance == AWK_MAX_DISTANCE)
			path->length = 0; // can't find a path to hit this thing
		else
			awk_pathfind_internal(team, start_vertex, closest_vertex, path);
	}
}

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
const r32 default_search_extents[] = { 15, 30, 15 };

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

		Vec3 start;
		nav_mesh_query->closestPointOnPoly(path_polys[0], (r32*)&a, (r32*)&start, 0);

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

					sensors.length = 0;
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
							chunk->normals.resize(vertex_count);
							sync_in.read(chunk->normals.data, vertex_count);
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

				if (nav_tile_cache)
				{
					if ((s32)id > obstacle_recast_ids.length - 1)
						obstacle_recast_ids.resize(id + 1);
					dtStatus status = nav_tile_cache->addObstacle((r32*)&pos, radius, height, &recast_id);
					obstacle_recast_ids[id] = recast_id;

					nav_tile_cache->update(0.0f, nav_mesh); // todo: batch obstacle API calls together
				}
				break;
			}
			case Op::ObstacleRemove:
			{
				u32 id;
				sync_in.read(&id);
				sync_in.unlock();

				if (nav_tile_cache)
				{
					u32 recast_id = obstacle_recast_ids[id];
					nav_tile_cache->removeObstacle(recast_id);

					nav_tile_cache->update(0.0f, nav_mesh); // todo: batch obstacle API calls together
				}
				break;
			}
			case Op::Pathfind:
			{
				Vec3 a;
				Vec3 b;
				LinkEntryArg<Path> callback;
				sync_in.read(&a);
				sync_in.read(&b);
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
				AI::AwkPathfind type;
				AI::Team team;
				Vec3 start;
				Vec3 start_normal;
				sync_in.read(&type);
				sync_in.read(&team);
				LinkEntryArg<Path> callback;
				sync_in.read(&callback);
				sync_in.read(&start);
				sync_in.read(&start_normal);

				Path path;

				switch (type)
				{
					case AwkPathfind::LongRange:
					{
						Vec3 end;
						sync_in.read(&end);
						Vec3 end_normal;
						sync_in.read(&end_normal);
						sync_in.unlock();
						awk_pathfind_internal(team, awk_closest_point(start, start_normal), awk_closest_point(end, end_normal), &path);
						break;
					}
					case AwkPathfind::Target:
					{
						Vec3 end;
						sync_in.read(&end);
						sync_in.unlock();
						awk_pathfind_hit(team, start, start_normal, end, &path);
						break;
					}
					case AwkPathfind::Random:
					{
						sync_in.unlock();

						Array<s32> chunks_with_vertices(awk_nav_mesh.chunks.length);
						for (s32 i = 0; i < awk_nav_mesh.chunks.length; i++)
						{
							if (awk_nav_mesh.chunks[i].vertices.length > 0)
								chunks_with_vertices.add(i);
						}
						s32 chunk_index = chunks_with_vertices[mersenne::randf_co() * chunks_with_vertices.length];
						const AwkNavMeshChunk& chunk = awk_nav_mesh.chunks[chunk_index];
						AwkNavMeshNode end_vertex;
						while (true)
						{
							s32 vertex_index = mersenne::randf_co() * (u16)chunk.vertices.length;
							if (chunk.adjacency[vertex_index].length > 0) // make sure it's not an orphan
							{
								end_vertex = { (u16)chunk_index, (u16)vertex_index };
								break;
							}
						}

						awk_pathfind_internal(team, awk_closest_point(start, start_normal), end_vertex, &path);
						break;
					}
					case AwkPathfind::Away:
					{
						Vec3 away;
						sync_in.read(&away);
						Vec3 away_normal;
						sync_in.read(&away_normal);
						sync_in.unlock();

						AwayScorer scorer;
						scorer.start_vertex = awk_closest_point(start, start_normal);
						scorer.away_vertex = awk_closest_point(away, away_normal);
						scorer.away_pos = away;

						awk_astar(team, scorer.start_vertex, &scorer, &path);
						break;
					}
					default:
					{
						vi_assert(false);
						break;
					}
				}

				sync_out.lock();
				sync_out.write(Callback::AwkPath);
				sync_out.write(callback);
				sync_out.write(path);
				sync_out.unlock();
				break;
			}
			case Op::UpdateSensors:
			{
				s32 count;
				sync_in.read(&count);
				sensors.resize(count);
				sync_in.read(sensors.data, sensors.length);
				sync_in.unlock();
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
	return data.travel_score + data.estimate_score + data.sensor_score;
}

AwkNavMeshNodeData& AwkNavMeshKey::get(const AwkNavMeshNode& node)
{
	return data.chunks[node.chunk][node.vertex];
}


}

}

}
