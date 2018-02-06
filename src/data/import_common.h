#pragma once

#include "array.h"
#include "lmath.h"
#include "recast/DetourTileCache/Include/DetourTileCacheBuilder.h"
#include "game/constants.h"
#include "render/glvm.h"
#include <unordered_map>

struct rcPolyMesh;
struct rcPolyMeshDetail;

namespace VI
{


struct Bone
{
	Quat rot;
	Vec3 pos;
};

struct BodyEntry : Bone
{
	enum class Type : s8
	{
		Box,
		Capsule,
		Sphere,
		count,
	};

	Vec3 size;
	s32 bone;
	Type type;
};

struct Armature
{
	Array<Bone> bind_pose;
	Array<Mat4> inverse_bind_pose;
	Array<Mat4> abs_bind_pose;
	Array<BodyEntry> bodies;
	Array<s32> hierarchy;
	Armature();
};

struct Mesh
{
	struct Attrib
	{
		RenderDataType type;
		s32 count;
		Array<char> data;
	};

	static void read(Mesh*, const char*, Array<Attrib>* = nullptr);
	Armature armature;
	Array<Vec3> vertices;
	Array<Vec3> normals;
	Array<s32> indices;
	Array<s32> edge_indices;
	Vec4 color;
	Vec3 bounds_min;
	Vec3 bounds_max;
	r32 bounds_radius;
	b8 instanced;

	void reset();
};

template<typename T>
struct Keyframe
{
	r32 time;
	T value;
};

struct Channel
{
	s32 bone_index;
	Array<Keyframe<Vec3> > positions;
	Array<Keyframe<Quat> > rotations;
	Array<Keyframe<Vec3> > scales;
};

struct Animation
{
	r32 duration;
	Array<Channel> channels;
};

struct Font
{
	struct Character
	{
		Vec2 min;
		Vec2 max;
		s32 index_start;
		s32 index_count;
		s32 vertex_start;
		s32 vertex_count;
		s32 codepoint;

		Character();
	};

	std::unordered_map<s32, Character> characters;
	Array<s32> indices;
	Array<Vec3> vertices;

	const Character& get(const char*) const;
};

struct FastLZCompressor : public dtTileCacheCompressor
{
	int maxCompressedSize(const int);
	dtStatus compress(const unsigned char*, const int, unsigned char*, const int, int*);
	dtStatus decompress(const unsigned char*, const int, unsigned char*, const int, int*);
};

struct TileCacheLayer
{
	u8* data;
	s32 data_size;
};

struct TileCacheCell
{
	Array<TileCacheLayer> layers;
};

struct TileCacheData
{
	Array<TileCacheCell> cells;
	Vec3 min;
	s32 width;
	s32 height;

	TileCacheData();
	void free();
	~TileCacheData();
};

const r32 nav_mesh_max_error = 1.5f;
const r32 nav_agent_height = 2.0f;
const r32 nav_agent_max_climb = 0.2f;
const r32 nav_agent_radius = 0.4f;
const r32 nav_edge_max_length = 20.0f;
const r32 nav_min_region_size = 8.0f;
const r32 nav_merged_region_size = 20.0f;
const r32 nav_detail_sample_distance = 8.0f;
const r32 nav_detail_sample_max_error = 0.5f;
const r32 nav_resolution = 0.2f;
const r32 nav_walkable_slope = 45.0f; // degrees
const r32 nav_tile_size = 20.0f;
const s32 nav_max_layers = 32;
const s32 nav_expected_layers_per_tile = 12; // how many layers (or "floors") each navmesh tile is expected to have
const s32 nav_max_obstacles = 128;

template<typename T> struct Chunks
{
	struct Coord
	{
		s32 x;
		s32 y;
		s32 z;
	};

	Vec3 vmin;
	r32 chunk_size;
	Coord size;
	Array<T> chunks;

	void resize()
	{
		s32 new_length = size.x * size.y * size.z;
		if (new_length < chunks.length)
		{
			for (s32 i = new_length; i < chunks.length; i++)
				chunks[i].~T();
			memset(&chunks[new_length], 0, sizeof(T) * chunks.length - new_length);
		}
		chunks.resize(new_length);
	}

	void resize(const Vec3& bmin, const Vec3& bmax, r32 cell_size)
	{
		vmin = bmin;
		chunk_size = cell_size;
		size.x = s32(ceilf((bmax.x - bmin.x) / cell_size));
		size.y = s32(ceilf((bmax.y - bmin.y) / cell_size));
		size.z = s32(ceilf((bmax.z - bmin.z) / cell_size));
		resize();
	}

	b8 contains(const Coord& c) const
	{
		return c.x >= 0 && c.y >= 0 && c.z >= 0
			&& c.x < size.x && c.y < size.y && c.z < size.z;
	}

	inline s32 index(const Coord& c) const
	{
		return c.x + (c.z * size.x) + (c.y * (size.x * size.z));
	}

	inline T* get(const Coord& c)
	{
		return &chunks[index(c)];
	}

	inline const T& get(const Coord& c) const
	{
		return chunks[index(c)];
	}

	Coord coord(Vec3 pos) const
	{
		return
		{
			s32((pos.x - vmin.x) / chunk_size),
			s32((pos.y - vmin.y) / chunk_size),
			s32((pos.z - vmin.z) / chunk_size),
		};
	}

	Coord coord(s32 i) const
	{
		Coord c;
		s32 xz = size.x * size.z;
		c.y = i / xz;
		s32 y_start = c.y * xz;
		c.z = (i - y_start) / size.x;
		c.x = i - (y_start + c.z * size.x);
		return c;
	}

	Vec3 pos(Coord coord) const
	{
		Vec3 offset(r32(coord.x), r32(coord.y), r32(coord.z));
		return vmin + (offset + Vec3(0.5f)) * chunk_size;
	}

	Vec3 pos(s32 i) const
	{
		return pos(coord(i));
	}

	Coord clamped_coord(Coord c) const
	{
		return
		{
			vi_max(vi_min(c.x, size.x - 1), 0),
			vi_max(vi_min(c.y, size.y - 1), 0),
			vi_max(vi_min(c.z, size.z - 1), 0),
		};
	}

	~Chunks()
	{
		for (s32 i = 0; i < chunks.length; i++)
			chunks[i].~T();
	}
};

#define DRONE_NAV_MESH_ADJACENCY 48 // must be <= 64 because flags is a u64
struct DroneNavMeshNode
{
	s16 chunk;
	s16 vertex;

	inline b8 equals(const DroneNavMeshNode& other) const
	{
		return chunk == other.chunk && vertex == other.vertex;
	}

	s32 hash() const
	{
		return (chunk << 16) | vertex;
	}
};

#define DRONE_NAV_MESH_NODE_NONE { -1, -1 }

struct DroneNavMeshAdjacency
{
	u64 flags;
	StaticArray<DroneNavMeshNode, DRONE_NAV_MESH_ADJACENCY> neighbors;
	b8 flag(s32) const;
	void flag(s32, b8);
	void remove(s32);
};

struct DroneNavMeshChunk
{
	Array<Vec3> vertices;
	Array<Vec3> normals;
	Array<DroneNavMeshAdjacency> adjacency;
};

struct ReverbCell
{
	r32 data[MAX_REVERBS];
	r32 outdoor;
};

struct ReverbVoxel : Chunks<ReverbCell>
{
	void read(FILE*);
};

struct DroneNavMesh : Chunks<DroneNavMeshChunk>
{
	ReverbVoxel reverb;

	void read(FILE*);
};

template<typename T>
void clean_name(T& name)
{
	for (s32 i = 0; ; i++)
	{
		char c = name[i];
		if (c == 0)
			break;
		if ((c < 'A' || c > 'Z')
			&& (c < 'a' || c > 'z')
			&& (i == 0 || c < '0' || c > '9')
			&& c != '_')
			name[i] = '_';
	}
}


}
