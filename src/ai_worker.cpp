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
DroneNavMesh drone_nav_mesh;
DroneNavMeshKey drone_nav_mesh_key;
PriorityQueue<DroneNavMeshNode, DroneNavMeshKey> astar_queue(&drone_nav_mesh_key);
dtTileCache* nav_tile_cache = nullptr;
dtTileCacheAlloc nav_tile_allocator;
FastLZCompressor nav_tile_compressor;
NavMeshProcess nav_tile_mesh_process;
dtNavMeshQuery* nav_mesh_query = nullptr;
dtQueryFilter default_query_filter = dtQueryFilter();
const r32 default_search_extents[] = { 15, 30, 15 };
Revision level_revision;
Array<RecordedLife> records;

dtPolyRef get_poly(const Vec3& pos, const r32* search_extents)
{
	dtPolyRef result;

	nav_mesh_query->findNearestPoly((r32*)(&pos), search_extents, &default_query_filter, &result, 0);

	return result;
}

struct AstarScorer
{
	// calculate heuristic score for nav mesh vertex
	// higher score = worse
	virtual r32 score(const Vec3&) = 0;
	// did we find what we're looking for?
	virtual b8 done(DroneNavMeshNode, const DroneNavMeshNodeData&) = 0;
};

// pathfind to a target vertex
struct PathfindScorer : AstarScorer
{
	DroneNavMeshNode end_vertex;
	Vec3 end_pos;

	virtual r32 score(const Vec3& pos)
	{
		return (end_pos - pos).length();
	}

	virtual b8 done(DroneNavMeshNode v, const DroneNavMeshNodeData&)
	{
		return v.equals(end_vertex);
	}
};

// run away from an enemy
struct AwayScorer : AstarScorer
{
	DroneNavMeshNode start_vertex;
	DroneNavMeshNode away_vertex;
	Vec3 away_pos;
	r32 minimum_distance;

	virtual r32 score(const Vec3& pos)
	{
		// estimated distance to goal
		return vi_max(0.0f, minimum_distance - (away_pos - pos).length());
	}

	virtual b8 done(DroneNavMeshNode v, const DroneNavMeshNodeData& data)
	{
		if (v.equals(start_vertex)) // we need to go somewhere other than here
			return false;

		if (data.sensor_score <= 8.0f) // inside a friendly sensor zone or force field
			return true;

		const Vec3& vertex = drone_nav_mesh.chunks[v.chunk].vertices[v.vertex];
		if ((vertex - away_pos).length_squared() < minimum_distance * minimum_distance)
			return false; // needs to be farther away

		const DroneNavMeshAdjacency& adjacency = drone_nav_mesh.chunks[away_vertex.chunk].adjacency[away_vertex.vertex];
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
	Vec3 start_pos;
	Vec3 goal;
	r32 minimum_distance;
	DroneNavMeshNode start_vertex;

	virtual r32 score(const Vec3& pos)
	{
		return (goal - pos).length();
	}

	virtual b8 done(DroneNavMeshNode v, const DroneNavMeshNodeData& data)
	{
		return drone_nav_mesh.chunks[v.chunk].adjacency[v.vertex].neighbors.length == DRONE_NAV_MESH_ADJACENCY // end goal must be a highly accessible location
			&& (start_pos - drone_nav_mesh.chunks[v.chunk].vertices[v.vertex]).length_squared() > (minimum_distance * minimum_distance);
	}
};

struct SpawnScorer : AstarScorer
{
	Vec3 start_pos;
	Vec3 dir;
	DroneNavMeshNode start_vertex;

	virtual r32 score(const Vec3& pos)
	{
		// want a vertex that is in the desired direction from the start position
		return 5.0f * (1.0f - dir.dot(pos - start_pos));
	}

	virtual b8 done(DroneNavMeshNode v, const DroneNavMeshNodeData& data)
	{
		return !v.equals(start_vertex);
	}
};

Array<SensorState> sensors;
Array<ForceFieldState> force_fields;

// describes which enemy force fields you are currently inside
u32 force_field_hash(Team my_team, const Vec3& pos)
{
	u32 result = 0;
	for (s32 i = 0; i < force_fields.length; i++)
	{
		const ForceFieldState& field = force_fields[i];
		if (field.team != my_team && (pos - field.pos).length_squared() < FORCE_FIELD_RADIUS * FORCE_FIELD_RADIUS)
		{
			if (result == 0)
				result = 1;
			result += MAX_ENTITIES % (i + 37); // todo: learn how to math
		}
	}
	return result;
}

b8 force_field_raycast(Team my_team, const Vec3& a, const Vec3& b)
{
	for (s32 i = 0; i < force_fields.length; i++)
	{
		const ForceFieldState& field = force_fields[i];
		if (field.team != my_team && LMath::ray_sphere_intersect(a, b, field.pos, FORCE_FIELD_RADIUS))
			return true;
	}
	return false;
}

r32 sensor_cost(Team team, const DroneNavMeshNode& node)
{
	const Vec3& pos = drone_nav_mesh.chunks[node.chunk].vertices[node.vertex];
	const Vec3& normal = drone_nav_mesh.chunks[node.chunk].normals[node.vertex];
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

	r32 force_field_cost = 8.0f;

	for (s32 i = 0; i < force_fields.length; i++)
	{
		const ForceFieldState& field = force_fields[i];
		if (field.team == team)
		{
			Vec3 to_field = field.pos - pos;
			if (to_field.length_squared() < FORCE_FIELD_RADIUS * FORCE_FIELD_RADIUS)
			{
				force_field_cost = 0.0f;
				break;
			}
		}
	}

	return sensor_cost + force_field_cost;
}

DroneNavMeshNode drone_closest_point(Team team, const Vec3& p, const Vec3& normal = Vec3::zero)
{
	DroneNavMesh::Coord chunk_coord = drone_nav_mesh.coord(p);
	r32 closest_distance = FLT_MAX;
	b8 found = false;
	DroneNavMeshNode closest = DRONE_NAV_MESH_NODE_NONE;
	b8 ignore_normals = normal.dot(normal) == 0.0f;
	u32 desired_hash = force_field_hash(team, p);
	s32 end_x = vi_min(vi_max(chunk_coord.x + 2, 1), drone_nav_mesh.size.x);
	for (s32 chunk_x = vi_min(vi_max(chunk_coord.x - 1, 0), drone_nav_mesh.size.x - 1); chunk_x < end_x; chunk_x++)
	{
		s32 end_y = vi_min(vi_max(chunk_coord.y + 2, 1), drone_nav_mesh.size.y);
		for (s32 chunk_y = vi_min(vi_max(chunk_coord.y - 1, 0), drone_nav_mesh.size.y - 1); chunk_y < end_y; chunk_y++)
		{
			s32 end_z = vi_min(vi_max(chunk_coord.z + 2, 1), drone_nav_mesh.size.z);
			for (s32 chunk_z = vi_min(vi_max(chunk_coord.z - 1, 0), drone_nav_mesh.size.z - 1); chunk_z < end_z; chunk_z++)
			{
				s32 chunk_index = drone_nav_mesh.index({ chunk_x, chunk_y, chunk_z });
				const DroneNavMeshChunk& chunk = drone_nav_mesh.chunks[chunk_index];
				for (s32 vertex_index = 0; vertex_index < chunk.vertices.length; vertex_index++)
				{
					const DroneNavMeshAdjacency& adjacency = chunk.adjacency[vertex_index];
					if (adjacency.neighbors.length > 0) // ignore orphans
					{
						const Vec3& vertex = chunk.vertices[vertex_index];
						const Vec3& normal = chunk.normals[vertex_index];
						Vec3 to_vertex = vertex - p;
						if (to_vertex.dot(normal) < 0.0f)
						{
							r32 distance = to_vertex.length_squared();
							if (distance < closest_distance
								&& force_field_hash(team, vertex) == desired_hash)
							{
								const Vec3& vertex_normal = chunk.normals[vertex_index];
								if (ignore_normals || normal.dot(vertex_normal) > 0.8f) // make sure it's roughly facing the right way
								{
									closest_distance = distance;
									closest = { s16(chunk_index), s16(vertex_index) };
									found = true;
								}
								else if (!found) // the normal is wrong, but we'll use it in an emergency
								{
									closest = { s16(chunk_index), s16(vertex_index) };
									found = true;
								}
							}
						}
					}
				}
			}
		}
	}
	return closest;
}

// can we hit the target from the given nav mesh node?
b8 can_hit_from(DroneNavMeshNode start_vertex, const Vec3& target, r32 dot_threshold, r32* closest_dot = nullptr)
{
	const Vec3& start = drone_nav_mesh.chunks[start_vertex.chunk].vertices[start_vertex.vertex];
	const Vec3& start_normal = drone_nav_mesh.chunks[start_vertex.chunk].normals[start_vertex.vertex];
	Vec3 to_target = target - start;
	r32 target_distance_squared = to_target.length_squared();
	to_target /= sqrtf(target_distance_squared); // normalize
	const DroneNavMeshAdjacency& start_adjacency = drone_nav_mesh.chunks[start_vertex.chunk].adjacency[start_vertex.vertex];

	for (s32 i = 0; i < start_adjacency.neighbors.length; i++)
	{
		const DroneNavMeshNode adjacent_vertex = start_adjacency.neighbors[i];
		const Vec3& adjacent = drone_nav_mesh.chunks[adjacent_vertex.chunk].vertices[adjacent_vertex.vertex];

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

inline b8 drone_flags_match(b8 flags, DroneAllow mask)
{
	// true = crawl
	// false = shoot
	return (s32)mask & (s32)(flags ? DroneAllow::Crawl : DroneAllow::Shoot);
}

// A*
void drone_astar(DroneAllow rule, Team team, const DroneNavMeshNode& start_vertex, AstarScorer* scorer, DronePath* path)
{
	path->length = 0;

	if (start_vertex.equals(DRONE_NAV_MESH_NODE_NONE))
		return;

#if DEBUG_AI
	r64 start_time = platform::time();
#endif

	const Vec3& start_pos = drone_nav_mesh.chunks[start_vertex.chunk].vertices[start_vertex.vertex];

	u32 start_field_hash = force_field_hash(team, start_pos);

	drone_nav_mesh_key.reset();
	astar_queue.clear();
	astar_queue.push(start_vertex);

	{
		DroneNavMeshNodeData* start_data = &drone_nav_mesh_key.get(start_vertex);
		start_data->travel_score = 0;
		start_data->estimate_score = scorer->score(start_pos);
		start_data->sensor_score = sensor_cost(team, start_vertex);
		start_data->parent = DRONE_NAV_MESH_NODE_NONE;
		start_data->crawled_from_parent = true;
		start_data->in_queue = true;
		start_data->visited = false;

#if DEBUG_AI
		vi_debug("estimate: %f - %s", start_data->estimate_score, typeid(*scorer).name());
#endif
	}

	while (astar_queue.size() > 0)
	{
		DroneNavMeshNode vertex_node = astar_queue.pop();

		DroneNavMeshNodeData* vertex_data = &drone_nav_mesh_key.get(vertex_node);

		vertex_data->visited = true;
		vertex_data->in_queue = false;

		const Vec3& vertex_pos = drone_nav_mesh.chunks[vertex_node.chunk].vertices[vertex_node.vertex];

		if (scorer->done(vertex_node, *vertex_data))
		{
			// reconstruct path
			DroneNavMeshNode n = vertex_node;
			while (true)
			{
				if (path->length == path->capacity())
					path->remove(path->length - 1);
				const DroneNavMeshNodeData& data = drone_nav_mesh_key.get(n);
				DronePathNode* node = path->insert(0);
				*node =
				{
					drone_nav_mesh.chunks[n.chunk].vertices[n.vertex],
					drone_nav_mesh.chunks[n.chunk].normals[n.vertex],
					n,
					data.crawled_from_parent, // crawl flag
				};
				if (n.equals(start_vertex))
					break;
				n = data.parent;
			}
			break; // done!
		}
		
		const DroneNavMeshAdjacency& adjacency = drone_nav_mesh.chunks[vertex_node.chunk].adjacency[vertex_node.vertex];
		for (s32 i = 0; i < adjacency.neighbors.length; i++)
		{
			// visit neighbors
			const DroneNavMeshNode adjacent_node = adjacency.neighbors[i];
			DroneNavMeshNodeData* adjacent_data = &drone_nav_mesh_key.get(adjacent_node);

			if (!adjacent_data->visited)
			{
				// hasn't been visited yet

				const Vec3& adjacent_pos = drone_nav_mesh.chunks[adjacent_node.chunk].vertices[adjacent_node.vertex];

				if (!drone_flags_match(adjacency.flag(i), rule)
					|| force_field_raycast(team, vertex_pos, adjacent_pos))
				{
					// flags don't match or it's in a different force field
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

							adjacent_data->crawled_from_parent = adjacency.flag(i);
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
						adjacent_data->crawled_from_parent = adjacency.flag(i);
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
void drone_pathfind_internal(DroneAllow rule, Team team, const DroneNavMeshNode& start_vertex, const DroneNavMeshNode& end_vertex, DronePath* path)
{
	path->length = 0;
	if (start_vertex.equals(DRONE_NAV_MESH_NODE_NONE) || end_vertex.equals(DRONE_NAV_MESH_NODE_NONE))
		return;
	PathfindScorer scorer;
	scorer.end_vertex = end_vertex;
	scorer.end_pos = drone_nav_mesh.chunks[end_vertex.chunk].vertices[end_vertex.vertex];
	const Vec3& start_pos = drone_nav_mesh.chunks[start_vertex.chunk].vertices[start_vertex.vertex];
	if (force_field_hash(team, start_pos) != force_field_hash(team, scorer.end_pos))
		return; // in a different force field; unreachable
	else
		drone_astar(rule, team, start_vertex, &scorer, path);
}

// find a path using vertices as close as possible to the given points
// find our way to a point from which we can shoot through the given target
void drone_pathfind_hit(DroneAllow rule, Team team, const Vec3& start, const Vec3& start_normal, const Vec3& target, DronePath* path)
{
	path->length = 0;
	if (force_field_hash(team, start) != force_field_hash(team, target))
		return; // in a different force field; unreachable

	DroneNavMeshNode target_closest_vertex = drone_closest_point(team, target, Vec3::zero);
	if (target_closest_vertex.equals(DRONE_NAV_MESH_NODE_NONE))
		return;

	DroneNavMeshNode start_vertex = drone_closest_point(team, start, start_normal);
	if (start_vertex.equals(DRONE_NAV_MESH_NODE_NONE))
		return;

	// even if we are supposed to be able to hit the target from the start vertex, don't just return a 0-length path.
	// find another vertex where we can hit the target
	// if we actually CAN hit the target, the low-level AI will take care of it.
	// if not, we'll have a path to another vertex where we can hopefully hit the target
	// this prevents us from getting stuck at a point where we think we should be able to hit the target, but we actually can't

	if (!target_closest_vertex.equals(start_vertex) && can_hit_from(target_closest_vertex, target, 0.999f))
		drone_pathfind_internal(rule, team, start_vertex, target_closest_vertex, path);
	else
	{
		const DroneNavMeshAdjacency& target_adjacency = drone_nav_mesh.chunks[target_closest_vertex.chunk].adjacency[target_closest_vertex.vertex];
		r32 closest_distance = DRONE_MAX_DISTANCE;
		r32 closest_dot = 0.7f;
		DroneNavMeshNode closest_vertex;
		for (s32 i = 0; i < target_adjacency.neighbors.length; i++)
		{
			const DroneNavMeshNode adjacent_vertex = target_adjacency.neighbors[i];
			if (!adjacent_vertex.equals(start_vertex))
			{
				const Vec3& adjacent = drone_nav_mesh.chunks[adjacent_vertex.chunk].vertices[adjacent_vertex.vertex];
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
		if (closest_distance == DRONE_MAX_DISTANCE)
			path->length = 0; // can't find a path to hit this thing
		else
		{
			drone_pathfind_internal(rule, team, start_vertex, closest_vertex, path);
			if (path->length > 0 && path->length < path->capacity())
			{
				DronePathNode* node = path->add();
				*node =
				{
					drone_nav_mesh.chunks[target_closest_vertex.chunk].vertices[target_closest_vertex.vertex],
					drone_nav_mesh.chunks[target_closest_vertex.chunk].normals[target_closest_vertex.vertex],
					target_closest_vertex,
					false, // crawl flag; we're shooting, so... no
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
	dtPolyRef path_polys[AI_MAX_PATH_LENGTH];
	dtPolyRef path_parents[AI_MAX_PATH_LENGTH];
	u8 path_straight_flags[AI_MAX_PATH_LENGTH];
	dtPolyRef path_straight_polys[AI_MAX_PATH_LENGTH];
	s32 path_poly_count;

	nav_mesh_query->findPath(start_poly, end_poly, (r32*)&a, (r32*)&b, &default_query_filter, path_polys, &path_poly_count, AI_MAX_PATH_LENGTH);
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
			path_straight_polys, &path_length, AI_MAX_PATH_LENGTH, 0
		);
		path->length = u16(path_length);

		if (path->length > 1)
			path->remove_ordered(0);
	}
}

void loop()
{
	nav_mesh_query = dtAllocNavMeshQuery();
	default_query_filter.setIncludeFlags(u16(-1));
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
					drone_nav_mesh.~DroneNavMesh();
					new (&drone_nav_mesh) DroneNavMesh();
					drone_nav_mesh_key.~DroneNavMeshKey();
					new (&drone_nav_mesh_key) DroneNavMeshKey();

					sensors.length = 0;

					for (s32 i = 0; i < records.length; i++)
						records[i].~RecordedLife();
					records.length = 0;
				}

				AssetID level_id;
				sync_in.read(&level_id);

				// get filenames of nav mesh and records and load them
				FILE* f = nullptr;
				s32 data_length = 0;
				{
					// nav mesh path
					char path[MAX_PATH_LENGTH];
					s32 path_length;
					sync_in.read(&path_length);
					vi_assert(path_length < MAX_PATH_LENGTH);
					sync_in.read(path, path_length);
					path[path_length] = '\0';

					// read records
					{
						char record_path[MAX_PATH_LENGTH];
						s32 record_path_length;
						sync_in.read(&record_path_length);
						vi_assert(record_path_length < MAX_PATH_LENGTH);
						sync_in.read(record_path, record_path_length);
						record_path[record_path_length] = '\0';
						sync_in.unlock();
						if (record_path_length > 0)
						{
							FILE* f = fopen(record_path, "rb");
							if (f)
							{
								fseek(f, 0, SEEK_END);
								s32 end = ftell(f);
								fseek(f, 0, SEEK_SET);
								while (ftell(f) != end)
								{
									RecordedLife* record = records.add();
									new (record) RecordedLife();
									record->serialize(f, &RecordedLife::custom_fread);
								}
								fclose(f);
							}
							else
								fprintf(stderr, "Can't open air file '%s'\n", record_path);
						}
					}

					// open nav mesh file
					if (path_length > 0)
					{
						f = fopen(path, "rb");
						if (!f)
						{
							fprintf(stderr, "Can't open nav file '%s'\n", path);
							vi_assert(false);
						}

						fseek(f, 0, SEEK_END);
						data_length = ftell(f);
						fseek(f, 0, SEEK_SET);
					}
				}


				if (data_length > 0)
				{
#if DEBUG_AI
					vi_debug("%d bytes", data_length);
#endif
					TileCacheData tiles;

					{
						// read tile data
						fread(&tiles.min, sizeof(Vec3), 1, f);
						fread(&tiles.width, sizeof(s32), 1, f);
						fread(&tiles.height, sizeof(s32), 1, f);
						tiles.cells.resize(tiles.width * tiles.height);
						for (s32 i = 0; i < tiles.cells.length; i++)
						{
							TileCacheCell& cell = tiles.cells[i];
							s32 layer_count;
							fread(&layer_count, sizeof(s32), 1, f);
							cell.layers.resize(layer_count);
							for (s32 j = 0; j < layer_count; j++)
							{
								TileCacheLayer& layer = cell.layers[j];
								fread(&layer.data_size, sizeof(s32), 1, f);
								layer.data = (u8*)dtAlloc(layer.data_size, dtAllocHint::DT_ALLOC_PERM);
								fread(layer.data, sizeof(u8), layer.data_size, f);
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

					// Drone nav mesh
					{
						fread(&drone_nav_mesh.chunk_size, sizeof(r32), 1, f);
						fread(&drone_nav_mesh.vmin, sizeof(Vec3), 1, f);
						fread(&drone_nav_mesh.size, sizeof(Chunks<DroneNavMeshChunk>::Coord), 1, f);
						drone_nav_mesh.resize();

						for (s32 i = 0; i < drone_nav_mesh.chunks.length; i++)
						{
							DroneNavMeshChunk* chunk = &drone_nav_mesh.chunks[i];
							s32 vertex_count;
							fread(&vertex_count, sizeof(s32), 1, f);
							chunk->vertices.resize(vertex_count);
							fread(chunk->vertices.data, sizeof(Vec3), vertex_count, f);
							chunk->normals.resize(vertex_count);
							fread(chunk->normals.data, sizeof(Vec3), vertex_count, f);
							chunk->adjacency.resize(vertex_count);
							fread(chunk->adjacency.data, sizeof(DroneNavMeshAdjacency), vertex_count, f);
						}
					}
				}

				if (f)
					fclose(f);

				drone_nav_mesh_key.resize(drone_nav_mesh);
				{
					// reserve space in the A* queue
					s32 vertex_count = 0;
					for (s32 i = 0; i < drone_nav_mesh.chunks.length; i++)
						vertex_count += drone_nav_mesh.chunks[i].adjacency.length;
					astar_queue.reserve(vertex_count);
				}

#if DEBUG_AI
				vi_debug("Done in %fs.", r32(platform::time() - start_time));
#endif

				{
					level_revision++;
					sync_out.lock();
					sync_out.write(Callback::Load);
					sync_out.write(level_revision);
					sync_out.unlock();
				}
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
#if DEBUG_AI
				r64 start_time = platform::time();
				vi_debug("Walk pathfind...");
#endif

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

#if DEBUG_AI
				vi_debug("%d nodes in %fs", path.length, r32(platform::time() - start_time));
#endif

				break;
			}
			case Op::RandomPath:
			{
#if DEBUG_AI
				r64 start_time = platform::time();
				vi_debug("Walk random path...");
#endif
				Vec3 start;
				sync_in.read(&start);
				Vec3 patrol_point;
				sync_in.read(&patrol_point);
				r32 range;
				sync_in.read(&range);
				LinkEntryArg<Path> callback;
				sync_in.read(&callback);
				sync_in.unlock();

				dtPolyRef start_poly = get_poly(start, default_search_extents);
				dtPolyRef patrol_point_poly = get_poly(patrol_point, default_search_extents);

				Vec3 end;
				dtPolyRef end_poly;
				nav_mesh_query->findRandomPointAroundCircle(patrol_point_poly, (r32*)&patrol_point, range * (0.5f + mersenne::randf_co() * 0.5f), &default_query_filter, mersenne::randf_co, &end_poly, (r32*)&end);

				Path path;

				if (start_poly && end_poly)
					pathfind(start, end, start_poly, end_poly, &path);

				sync_out.lock();
				sync_out.write(Callback::Path);
				sync_out.write(callback);
				sync_out.write(path);
				sync_out.unlock();

#if DEBUG_AI
				vi_debug("%d nodes in %fs", path.length, r32(platform::time() - start_time));
#endif

				break;
			}
			case Op::ClosestWalkPoint:
			{
#if DEBUG_AI
				r64 start_time = platform::time();
				vi_debug("Walkable point query...");
#endif
				LinkEntryArg<Path> callback;
				Vec3 pos;
				sync_in.read(&pos);
				sync_in.read(&callback);
				sync_in.unlock();

				dtPolyRef poly = get_poly(pos, default_search_extents);
				Vec3 closest;
				nav_mesh_query->closestPointOnPoly(poly, (r32*)&pos, (r32*)&closest, 0);

				sync_out.lock();
				sync_out.write(Callback::Point);
				sync_out.write(callback);
				sync_out.write(closest);
				sync_out.unlock();

#if DEBUG_AI
				vi_debug("Done in %fs", r32(platform::time() - start_time));
#endif
				break;
			}
			case Op::DronePathfind:
			{
				DronePathfind type;
				DroneAllow rule;
				Team team;
				Vec3 start;
				Vec3 start_normal;
				sync_in.read(&type);
				sync_in.read(&rule);
				sync_in.read(&team);
				LinkEntryArg<DronePath> callback;
				sync_in.read(&callback);
				sync_in.read(&start);
				sync_in.read(&start_normal);

				DronePath path;

				switch (type)
				{
					case DronePathfind::LongRange:
					{
						Vec3 end;
						sync_in.read(&end);
						Vec3 end_normal;
						sync_in.read(&end_normal);
						sync_in.unlock();
						drone_pathfind_internal(rule, team, drone_closest_point(team, start, start_normal), drone_closest_point(team, end, end_normal), &path);
						break;
					}
					case DronePathfind::Target:
					{
						Vec3 end;
						sync_in.read(&end);
						sync_in.unlock();
						drone_pathfind_hit(rule, team, start, start_normal, end, &path);
						break;
					}
					case DronePathfind::Spawn:
					{
						Vec3 dir;
						sync_in.read(&dir);
						sync_in.unlock();

						SpawnScorer scorer;
						scorer.dir = dir;
						scorer.start_pos = start;
						scorer.start_vertex = drone_closest_point(team, start, start_normal);

						drone_astar(rule, team, scorer.start_vertex, &scorer, &path);
						
						break;
					}
					case DronePathfind::Random:
					{
						sync_in.unlock();

						RandomScorer scorer;
						scorer.start_vertex = drone_closest_point(team, start, start_normal);
						scorer.start_pos = start;
						scorer.minimum_distance = rule == DroneAllow::Crawl ? DRONE_MAX_DISTANCE * 0.5f : DRONE_MAX_DISTANCE * 3.0f;
						scorer.minimum_distance = vi_min(scorer.minimum_distance,
							vi_min(drone_nav_mesh.size.x, drone_nav_mesh.size.z) * drone_nav_mesh.chunk_size * 0.5f);
						scorer.goal = drone_nav_mesh.vmin +
						Vec3
						(
							mersenne::randf_co() * (drone_nav_mesh.size.x * drone_nav_mesh.chunk_size),
							mersenne::randf_co() * (drone_nav_mesh.size.y * drone_nav_mesh.chunk_size),
							mersenne::randf_co() * (drone_nav_mesh.size.z * drone_nav_mesh.chunk_size)
						);

						drone_astar(rule, team, scorer.start_vertex, &scorer, &path);

						break;
					}
					case DronePathfind::Away:
					{
						Vec3 away;
						sync_in.read(&away);
						Vec3 away_normal;
						sync_in.read(&away_normal);
						sync_in.unlock();

						AwayScorer scorer;
						scorer.start_vertex = drone_closest_point(team, start, start_normal);
						scorer.away_vertex = drone_closest_point(team, away, away_normal);
						if (!scorer.away_vertex.equals(DRONE_NAV_MESH_NODE_NONE))
						{
							scorer.away_pos = away;
							scorer.minimum_distance = rule == DroneAllow::Crawl ? DRONE_MAX_DISTANCE * 0.5f : DRONE_MAX_DISTANCE * 3.0f;
							scorer.minimum_distance = vi_min(scorer.minimum_distance,
								vi_min(drone_nav_mesh.size.x, drone_nav_mesh.size.z) * drone_nav_mesh.chunk_size * 0.5f);

							drone_astar(rule, team, scorer.start_vertex, &scorer, &path);
						}
						break;
					}
					default:
					{
						vi_assert(false);
						break;
					}
				}

				sync_out.lock();
				sync_out.write(Callback::DronePath);
				sync_out.write(callback);
				sync_out.write(path);
				sync_out.unlock();
				break;
			}
			case Op::DroneClosestPoint:
			{
				LinkEntryArg<DronePathNode> callback;
				sync_in.read(&callback);
				AI::Team team;
				sync_in.read(&team);
				Vec3 search_pos;
				sync_in.read(&search_pos);
				sync_in.unlock();

				DronePathNode result;
				result.ref = drone_closest_point(team, search_pos);
				if (result.ref.equals(DRONE_NAV_MESH_NODE_NONE))
				{
					result.pos = search_pos;
					result.normal = Vec3(0, 1, 0);
				}
				else
				{
					result.pos = drone_nav_mesh.chunks[result.ref.chunk].vertices[result.ref.vertex];
					result.normal = drone_nav_mesh.chunks[result.ref.chunk].normals[result.ref.vertex];
				}

				sync_out.lock();
				sync_out.write(Callback::DronePoint);
				sync_out.write(callback);
				sync_out.write(result);
				sync_out.unlock();
				break;
			}
			case Op::DroneMarkAdjacencyBad:
			{
				DroneNavMeshNode a;
				sync_in.read(&a);
				DroneNavMeshNode b;
				sync_in.read(&b);
				sync_in.unlock();

				// remove b from a's adjacency list
				DroneNavMeshAdjacency* adjacency = &drone_nav_mesh.chunks[a.chunk].adjacency[a.vertex];
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
				force_fields.resize(count);
				sync_in.read(force_fields.data, force_fields.length);
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

// Drone nav mesh stuff

void DroneNavMeshKey::resize(const DroneNavMesh& nav)
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

void DroneNavMeshKey::reset()
{
	for (s32 i = 0; i < data.chunks.length; i++)
		memset(data.chunks[i].data, 0, sizeof(DroneNavMeshNodeData) * data.chunks[i].length);
}

r32 DroneNavMeshKey::priority(const DroneNavMeshNode& a)
{
	const DroneNavMeshNodeData& data = get(a);
	return data.travel_score + data.estimate_score + data.sensor_score;
}

DroneNavMeshNodeData& DroneNavMeshKey::get(const DroneNavMeshNode& node)
{
	return data.chunks[node.chunk][node.vertex];
}


}

}

}
