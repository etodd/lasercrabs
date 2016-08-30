#include "ai.h"
#include "data/import_common.h"
#include "recast/Recast/Include/Recast.h"
#include "recast/Detour/Include/DetourNavMeshBuilder.h"
#include "recast/Detour/Include/DetourCommon.h"
#include "data/priority_queue.h"
#include "mersenne/mersenne-twister.h"

#define DEBUG_AI 0

#if DEBUG_AI
#include "platform/util.h"
#endif

namespace VI
{

namespace AI
{

namespace Worker
{


dtNavMesh* nav_mesh = nullptr;
AwkNavMesh awk_nav_mesh;
AwkNavMeshKey awk_nav_mesh_key;
PriorityQueue<AwkNavMeshNode, AwkNavMeshKey> astar_queue(&awk_nav_mesh_key);
dtTileCache* nav_tile_cache = nullptr;
dtTileCacheAlloc nav_tile_allocator;
FastLZCompressor nav_tile_compressor;
NavMeshProcess nav_tile_mesh_process;
dtNavMeshQuery* nav_mesh_query = nullptr;
dtQueryFilter default_query_filter = dtQueryFilter();
const r32 default_search_extents[] = { 15, 30, 15 };

dtPolyRef get_poly(const Vec3& pos, const r32* search_extents)
{
	dtPolyRef result;

	nav_mesh_query->findNearestPoly((r32*)&pos, search_extents, &default_query_filter, &result, 0);

	return result;
}

struct AstarScorer
{
	// calculate heuristic score for nav mesh vertex
	virtual r32 score(const Vec3&) = 0;
	// did we find what we're looking for?
	virtual b8 done(AwkNavMeshNode, const AwkNavMeshNodeData&) = 0;
};

// pathfind to a target vertex
struct PathfindScorer : AstarScorer
{
	AwkNavMeshNode end_vertex;
	Vec3 end_pos;

	virtual r32 score(const Vec3& pos)
	{
		return (end_pos - pos).length();
	}

	virtual b8 done(AwkNavMeshNode v, const AwkNavMeshNodeData&)
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
	r32 minimum_distance;

	virtual r32 score(const Vec3& pos)
	{
		// estimated distance to goal
		return vi_max(0.0f, minimum_distance - (away_pos - pos).length());
	}

	virtual b8 done(AwkNavMeshNode v, const AwkNavMeshNodeData& data)
	{
		if (v.equals(start_vertex)) // we need to go somewhere other than here
			return false;

		if (data.sensor_score <= 8.0f) // inside a friendly sensor zone or containment field
			return true;

		const Vec3& vertex = awk_nav_mesh.chunks[v.chunk].vertices[v.vertex];
		if ((vertex - away_pos).length_squared() < minimum_distance * minimum_distance)
			return false; // needs to be farther away

		const AwkNavMeshAdjacency& adjacency = awk_nav_mesh.chunks[away_vertex.chunk].adjacency[away_vertex.vertex];
		for (s32 i = 0; i < adjacency.neighbors.length; i++)
		{
			if (adjacency.neighbors[i].equals(v))
				return false;
		}
		return true; // the enemy can't get there, it's safe
	}
};

struct RandomScorer : AstarScorer
{
	AwkNavMeshNode start_vertex;
	Vec3 start_pos;
	Vec3 goal;
	r32 minimum_distance;

	virtual r32 score(const Vec3& pos)
	{
		return (goal - pos).length();
	}

	virtual b8 done(AwkNavMeshNode v, const AwkNavMeshNodeData& data)
	{
		return (start_pos - awk_nav_mesh.chunks[v.chunk].vertices[v.vertex]).length_squared() > (minimum_distance * minimum_distance);
	}
};

Array<SensorState> sensors;
Array<ContainmentFieldState> containment_fields;

// describes which enemy containment fields you are currently inside
u32 containment_field_hash(Team my_team, const Vec3& pos)
{
	u32 result = 0;
	for (s32 i = 0; i < containment_fields.length; i++)
	{
		const ContainmentFieldState& field = containment_fields[i];
		if (field.team != my_team && (pos - field.pos).length_squared() < CONTAINMENT_FIELD_RADIUS * CONTAINMENT_FIELD_RADIUS)
		{
			if (result == 0)
				result = 1;
			result += MAX_ENTITIES % (i + 37); // todo: learn how to math
		}
	}
	return result;
}

b8 containment_field_raycast(Team my_team, const Vec3& a, const Vec3& b)
{
	for (s32 i = 0; i < containment_fields.length; i++)
	{
		const ContainmentFieldState& field = containment_fields[i];
		if (field.team != my_team && LMath::ray_sphere_intersect(a, b, field.pos, CONTAINMENT_FIELD_RADIUS))
			return true;
	}
	return false;
}

r32 sensor_cost(Team team, const AwkNavMeshNode& node)
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

	r32 sensor_cost;

	if (in_enemy_zone)
		sensor_cost = 24.0f;
	else if (in_friendly_zone)
		sensor_cost = 0;
	else
		sensor_cost = 8.0f;

	r32 containment_field_cost = 8.0f;

	for (s32 i = 0; i < containment_fields.length; i++)
	{
		const ContainmentFieldState& field = containment_fields[i];
		if (field.team == team)
		{
			Vec3 to_field = field.pos - pos;
			if (to_field.length_squared() < CONTAINMENT_FIELD_RADIUS * CONTAINMENT_FIELD_RADIUS)
			{
				containment_field_cost = 0.0f;
				break;
			}
		}
	}

	return sensor_cost + containment_field_cost;
}

AwkNavMeshNode awk_closest_point(Team team, const Vec3& p, const Vec3& normal)
{
	AwkNavMesh::Coord chunk_coord = awk_nav_mesh.coord(p);
	r32 closest_distance = FLT_MAX;
	b8 found = false;
	AwkNavMeshNode closest;
	b8 ignore_normals = normal.dot(normal) == 0.0f;
	u32 desired_hash = containment_field_hash(team, p);
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
					if (adjacency.neighbors.length > 0) // ignore orphans
					{
						const Vec3& vertex = chunk.vertices[vertex_index];
						const Vec3& normal = chunk.normals[vertex_index];
						Vec3 to_vertex = vertex - p;
						if (to_vertex.dot(normal) < 0.0f)
						{
							r32 distance = to_vertex.length_squared();
							if (distance < closest_distance
								&& containment_field_hash(team, vertex) == desired_hash)
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
	}
	vi_assert(found);
	return closest;
}

// can we hit the target from the given nav mesh node?
b8 can_hit_from(AwkNavMeshNode start_vertex, const Vec3& target, r32 dot_threshold, r32* closest_dot = nullptr)
{
	const Vec3& start = awk_nav_mesh.chunks[start_vertex.chunk].vertices[start_vertex.vertex];
	const Vec3& start_normal = awk_nav_mesh.chunks[start_vertex.chunk].normals[start_vertex.vertex];
	Vec3 to_target = target - start;
	r32 target_distance_squared = to_target.length_squared();
	to_target /= sqrtf(target_distance_squared); // normalize
	const AwkNavMeshAdjacency& start_adjacency = awk_nav_mesh.chunks[start_vertex.chunk].adjacency[start_vertex.vertex];

	for (s32 i = 0; i < start_adjacency.neighbors.length; i++)
	{
		const AwkNavMeshNode adjacent_vertex = start_adjacency.neighbors[i];
		const Vec3& adjacent = awk_nav_mesh.chunks[adjacent_vertex.chunk].vertices[adjacent_vertex.vertex];

		Vec3 to_adjacent = adjacent - start;
		if (to_adjacent.dot(start_normal) > 0.07f) // must not be a neighbor for crawling (co-planar or around a corner)
		{
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
	}
	return false;
}

inline b8 awk_flags_match(b8 flags, AwkAllow mask)
{
	// true = crawl
	// false = shoot
	return (s32)mask & (s32)(flags ? AwkAllow::Crawl : AwkAllow::Shoot);
}

// A*
void awk_astar(AwkAllow rule, Team team, const AwkNavMeshNode& start_vertex, AstarScorer* scorer, AwkPath* path)
{
#if DEBUG_AI
	r64 start_time = platform::time();
#endif

	path->length = 0;

	const Vec3& start_pos = awk_nav_mesh.chunks[start_vertex.chunk].vertices[start_vertex.vertex];

	u32 start_field_hash = containment_field_hash(team, start_pos);

	awk_nav_mesh_key.reset();
	astar_queue.clear();
	astar_queue.push(start_vertex);

	{
		AwkNavMeshNodeData* start_data = &awk_nav_mesh_key.get(start_vertex);
		start_data->travel_score = 0;
		start_data->estimate_score = scorer->score(start_pos);
		start_data->sensor_score = sensor_cost(team, start_vertex);
		start_data->parent = { (u16)-1, (u16)-1 };
		start_data->in_queue = true;

#if DEBUG_AI
		vi_debug("estimate: %f - %s", start_data->estimate_score, typeid(*scorer).name());
#endif
	}

	while (astar_queue.size() > 0)
	{
		AwkNavMeshNode vertex_node = astar_queue.pop();

		AwkNavMeshNodeData* vertex_data = &awk_nav_mesh_key.get(vertex_node);

		vertex_data->visited = true;
		vertex_data->in_queue = false;

		const Vec3& vertex_pos = awk_nav_mesh.chunks[vertex_node.chunk].vertices[vertex_node.vertex];

		if (scorer->done(vertex_node, *vertex_data))
		{
			// reconstruct path
			AwkNavMeshNode n = vertex_node;
			while (true)
			{
				if (path->length == path->capacity())
					path->remove(path->length - 1);
				AwkPathNode* node = path->insert(0);
				*node =
				{
					awk_nav_mesh.chunks[n.chunk].vertices[n.vertex],
					awk_nav_mesh.chunks[n.chunk].normals[n.vertex],
					n,
				};
				if (n.equals(start_vertex))
					break;
				n = awk_nav_mesh_key.get(n).parent;
			}
			break; // done!
		}
		
		const AwkNavMeshAdjacency& adjacency = awk_nav_mesh.chunks[vertex_node.chunk].adjacency[vertex_node.vertex];
		for (s32 i = 0; i < adjacency.neighbors.length; i++)
		{
			// visit neighbors
			const AwkNavMeshNode adjacent_node = adjacency.neighbors[i];
			AwkNavMeshNodeData* adjacent_data = &awk_nav_mesh_key.get(adjacent_node);

			if (!adjacent_data->visited)
			{
				// hasn't been visited yet

				const Vec3& adjacent_pos = awk_nav_mesh.chunks[adjacent_node.chunk].vertices[adjacent_node.vertex];

				if (!awk_flags_match(adjacency.flag(i), rule)
					|| containment_field_raycast(team, vertex_pos, adjacent_pos))
				{
					// flags don't match or it's in a different containment field
					// therefore it's unreachable
					adjacent_data->visited = true;
				}
				else
				{
					r32 candidate_travel_score = vertex_data->travel_score
						+ vertex_data->sensor_score
						+ (adjacent_pos - vertex_pos).length()
						+ 4.0f; // bias toward longer shots

					if (adjacent_data->in_queue)
					{
						// it's already in the queue
						if (candidate_travel_score < adjacent_data->travel_score)
						{
							// this is a better path

							adjacent_data->parent = vertex_node;
							adjacent_data->travel_score = candidate_travel_score;

							// update its position in the queue due to the score change
							for (s32 j = 0; j < astar_queue.size(); j++)
							{
								if (astar_queue.heap[j].equals(adjacent_node))
								{
									astar_queue.update(j);
									break;
								}
							}
						}
					}
					else
					{
						// totally new node, not in queue yet
						adjacent_data->parent = vertex_node;
						adjacent_data->sensor_score = sensor_cost(team, adjacent_node);
						adjacent_data->travel_score = candidate_travel_score;
						adjacent_data->estimate_score = scorer->score(adjacent_pos);
						adjacent_data->in_queue = true;
						astar_queue.push(adjacent_node);
					}
				}
			}
		}
	}
#if DEBUG_AI
	vi_debug("%d nodes in %fs - %s", path->length, (r32)(platform::time() - start_time), typeid(*scorer).name());
#endif
}

// find a path from vertex a to vertex b
void awk_pathfind_internal(AwkAllow rule, Team team, const AwkNavMeshNode& start_vertex, const AwkNavMeshNode& end_vertex, AwkPath* path)
{
	PathfindScorer scorer;
	scorer.end_vertex = end_vertex;
	scorer.end_pos = awk_nav_mesh.chunks[end_vertex.chunk].vertices[end_vertex.vertex];
	const Vec3& start_pos = awk_nav_mesh.chunks[start_vertex.chunk].vertices[start_vertex.vertex];
	if (containment_field_hash(team, start_pos) != containment_field_hash(team, scorer.end_pos))
		path->length = 0; // in a different containment field; unreachable
	else
		awk_astar(rule, team, start_vertex, &scorer, path);
}

// find a path using vertices as close as possible to the given points
// find our way to a point from which we can shoot through the given target
void awk_pathfind_hit(AwkAllow rule, Team team, const Vec3& start, const Vec3& start_normal, const Vec3& target, AwkPath* path)
{
	if (containment_field_hash(team, start) != containment_field_hash(team, target))
	{
		// in a different containment field; unreachable
		path->length = 0;
		return;
	}

	AwkNavMeshNode target_closest_vertex = awk_closest_point(team, target, Vec3::zero);

	AwkNavMeshNode start_vertex = awk_closest_point(team, start, start_normal);

	// even if we are supposed to be able to hit the target from the start vertex, don't just return a 0-length path.
	// find another vertex where we can hit the target
	// if we actually CAN hit the target, the low-level AI will take care of it.
	// if not, we'll have a path to another vertex where we can hopefully hit the target
	// this prevents us from getting stuck at a point where we think we should be able to hit the target, but we actually can't

	if (!target_closest_vertex.equals(start_vertex) && can_hit_from(target_closest_vertex, target, 0.999f))
		awk_pathfind_internal(rule, team, start_vertex, target_closest_vertex, path);
	else
	{
		const AwkNavMeshAdjacency& target_adjacency = awk_nav_mesh.chunks[target_closest_vertex.chunk].adjacency[target_closest_vertex.vertex];
		r32 closest_distance = AWK_MAX_DISTANCE;
		r32 closest_dot = 0.7f;
		AwkNavMeshNode closest_vertex;
		for (s32 i = 0; i < target_adjacency.neighbors.length; i++)
		{
			const AwkNavMeshNode adjacent_vertex = target_adjacency.neighbors[i];
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
		{
			awk_pathfind_internal(rule, team, start_vertex, closest_vertex, path);
			if (path->length > 0 && path->length < path->capacity())
			{
				AwkPathNode* node = path->add();
				*node =
				{
					awk_nav_mesh.chunks[target_closest_vertex.chunk].vertices[target_closest_vertex.vertex],
					awk_nav_mesh.chunks[target_closest_vertex.chunk].normals[target_closest_vertex.vertex],
					target_closest_vertex,
				};
			}
		}
	}
}

void NavMeshProcess::process(struct dtNavMeshCreateParams* params, u8* polyAreas, u16* polyFlags)
{
	for (int i = 0; i < params->polyCount; i++)
		polyFlags[i] = 1;
}

void pathfind(const Vec3& a, const Vec3& b, dtPolyRef start_poly, dtPolyRef end_poly, Path* path)
{
	dtPolyRef path_polys[MAX_PATH_LENGTH];
	dtPolyRef path_parents[MAX_PATH_LENGTH];
	u8 path_straight_flags[MAX_PATH_LENGTH];
	dtPolyRef path_straight_polys[MAX_PATH_LENGTH];
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

		s32 path_length;
		nav_mesh_query->findStraightPath
		(
			(const r32*)&a, (const r32*)&end, path_polys, path_poly_count,
			(r32*)path->data, path_straight_flags,
			path_straight_polys, &path_length, MAX_PATH_LENGTH, 0
		);
		path->length = (u16)path_length;

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
#if DEBUG_AI
				vi_debug("Loading nav mesh...");
				r32 start_time = platform::time();
#endif
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
					awk_nav_mesh_key.~AwkNavMeshKey();
					new (&awk_nav_mesh_key) AwkNavMeshKey();

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

				awk_nav_mesh_key.resize(awk_nav_mesh);
				{
					// reserve space in the A* queue
					s32 vertex_count = 0;
					for (s32 i = 0; i < awk_nav_mesh.chunks.length; i++)
						vertex_count += awk_nav_mesh.chunks[i].adjacency.length;
					astar_queue.reserve(vertex_count);
				}

				sync_in.unlock();

#if DEBUG_AI
				vi_debug("Done in %fs.", (r32)(platform::time() - start_time));
#endif

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
				AwkPathfind type;
				AwkAllow rule;
				Team team;
				Vec3 start;
				Vec3 start_normal;
				sync_in.read(&type);
				sync_in.read(&rule);
				sync_in.read(&team);
				LinkEntryArg<AwkPath> callback;
				sync_in.read(&callback);
				sync_in.read(&start);
				sync_in.read(&start_normal);

				AwkPath path;

				switch (type)
				{
					case AwkPathfind::LongRange:
					{
						Vec3 end;
						sync_in.read(&end);
						Vec3 end_normal;
						sync_in.read(&end_normal);
						sync_in.unlock();
						awk_pathfind_internal(rule, team, awk_closest_point(team, start, start_normal), awk_closest_point(team, end, end_normal), &path);
						break;
					}
					case AwkPathfind::Target:
					{
						Vec3 end;
						sync_in.read(&end);
						sync_in.unlock();
						awk_pathfind_hit(rule, team, start, start_normal, end, &path);
						break;
					}
					case AwkPathfind::Random:
					{
						sync_in.unlock();

						RandomScorer scorer;
						scorer.start_vertex = awk_closest_point(team, start, start_normal);
						scorer.start_pos = start;
						scorer.minimum_distance = rule == AwkAllow::Crawl ? AWK_MAX_DISTANCE * 0.5f : AWK_MAX_DISTANCE * 3.0f;
						scorer.minimum_distance = vi_min(scorer.minimum_distance,
							vi_min(awk_nav_mesh.size.x, awk_nav_mesh.size.z) * awk_nav_mesh.chunk_size * 0.5f);
						scorer.goal = awk_nav_mesh.vmin +
						Vec3
						(
							mersenne::randf_co() * (awk_nav_mesh.size.x * awk_nav_mesh.chunk_size),
							mersenne::randf_co() * (awk_nav_mesh.size.y * awk_nav_mesh.chunk_size),
							mersenne::randf_co() * (awk_nav_mesh.size.z * awk_nav_mesh.chunk_size)
						);

						awk_astar(rule, team, scorer.start_vertex, &scorer, &path);

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
						scorer.start_vertex = awk_closest_point(team, start, start_normal);
						scorer.away_vertex = awk_closest_point(team, away, away_normal);
						scorer.away_pos = away;
						scorer.minimum_distance = rule == AwkAllow::Crawl ? AWK_MAX_DISTANCE * 0.5f : AWK_MAX_DISTANCE * 3.0f;
						scorer.minimum_distance = vi_min(scorer.minimum_distance,
							vi_min(awk_nav_mesh.size.x, awk_nav_mesh.size.z) * awk_nav_mesh.chunk_size * 0.5f);

						awk_astar(rule, team, scorer.start_vertex, &scorer, &path);
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
			case Op::AwkMarkAdjacencyBad:
			{
				AwkNavMeshNode a;
				sync_in.read(&a);
				AwkNavMeshNode b;
				sync_in.read(&b);
				sync_in.unlock();

				// remove b from a's adjacency list
				AwkNavMeshAdjacency* adjacency = &awk_nav_mesh.chunks[a.chunk].adjacency[a.vertex];
				for (s32 i = 0; i < adjacency->neighbors.length; i++)
				{
					if (adjacency->neighbors[i].equals(b))
					{
						adjacency->remove(i);
						break;
					}
				}

				break;
			}
			case Op::UpdateState:
			{
				s32 count;
				sync_in.read(&count);
				sensors.resize(count);
				sync_in.read(sensors.data, sensors.length);
				sync_in.read(&count);
				containment_fields.resize(count);
				sync_in.read(containment_fields.data, containment_fields.length);
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

void AwkNavMeshKey::resize(const AwkNavMesh& nav)
{
	data.chunk_size = nav.chunk_size;
	data.size.x = nav.size.x;
	data.size.y = nav.size.y;
	data.size.z = nav.size.z;
	data.vmin = nav.vmin;
	data.resize();
	for (s32 i = 0; i < data.chunks.length; i++)
		data.chunks[i].resize(nav.chunks[i].vertices.length);
}

void AwkNavMeshKey::reset()
{
	for (s32 i = 0; i < data.chunks.length; i++)
		memset(data.chunks[i].data, 0, sizeof(AwkNavMeshNodeData) * data.chunks[i].length);
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
