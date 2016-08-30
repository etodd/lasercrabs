#pragma once

#include "array.h"
#include "lmath.h"
#include <array>
#include "recast/DetourTileCache/Include/DetourTileCacheBuilder.h"
#include "pin_array.h"

struct cJSON;
struct rcPolyMesh;
struct rcPolyMeshDetail;

namespace VI
{

#define AWK_DASH_SPEED 20.0f
#define AWK_DASH_TIME 0.3f
#define AWK_DASH_DISTANCE (AWK_DASH_SPEED * AWK_DASH_TIME)
#define AWK_MAX_DISTANCE 25.0f
#define AWK_RADIUS 0.2f
#define MAX_BONE_WEIGHTS 4
#define AWK_VERTICAL_DOT_LIMIT 0.9998f
#define AWK_VERTICAL_ANGLE_LIMIT ((PI * 0.5f) - 0.021f)

namespace Json
{
	cJSON* load(const char*);
	void save(cJSON*, const char*);
	void json_free(cJSON*);
	Vec3 get_vec3(cJSON*, const char*, const Vec3& = Vec3::zero);
	Vec4 get_vec4(cJSON*, const char*, const Vec4& = Vec4::zero);
	Quat get_quat(cJSON*, const char*, const Quat& = Quat::identity);
	r32 get_r32(cJSON*, const char*, const r32 = 0.0f);
	s32 get_s32(cJSON*, const char*, const s32 = 0);
	const char* get_string(cJSON*, const char*, const char* = 0);
	s32 get_enum(cJSON*, const char*, const char**, const s32 = 0);
};

struct Bone
{
	Quat rot;
	Vec3 pos;
};

struct BodyEntry : Bone
{
	enum class Type
	{
		Box,
		Capsule,
		Sphere,
	};

	s32 bone;
	Vec3 size;
	Type type;
};

struct Armature
{
	Array<s32> hierarchy;
	Array<Bone> bind_pose;
	Array<Mat4> inverse_bind_pose;
	Array<Mat4> abs_bind_pose;
	Array<BodyEntry> bodies;
	Armature()
		: hierarchy(), bind_pose(), inverse_bind_pose(), abs_bind_pose(), bodies()
	{

	}
};

struct Mesh
{
	Array<s32> indices;
	Array<Vec3> vertices;
	Array<Vec3> normals;
	Armature armature;
	Vec3 bounds_min;
	Vec3 bounds_max;
	r32 bounds_radius;
	Vec4 color;
	b8 instanced;
	void reset()
	{
		indices.length = 0;
		vertices.length = 0;
		normals.length = 0;
		armature.hierarchy.length = 0;
		armature.bind_pose.length = 0;
		armature.inverse_bind_pose.length = 0;
		instanced = false;
	}
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
		char code;
		s32 index_start;
		s32 index_count;
		s32 vertex_start;
		s32 vertex_count;
		Vec2 min;
		Vec2 max;
	};
	Array<Character> characters;
	Array<s32> indices;
	Array<Vec3> vertices;
	Font()
		: characters(), indices(), vertices()
	{
	}

	const Character& get(const void*) const;
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
	Vec3 min;
	s32 width;
	s32 height;
	Array<TileCacheCell> cells;
	void free();
	~TileCacheData();
};

const r32 nav_mesh_max_error = 2.0f;
const r32 nav_agent_height = 2.0f;
const r32 nav_agent_max_climb = 0.5f;
const r32 nav_agent_radius = 0.4f;
const r32 nav_edge_max_length = 12.0f;
const r32 nav_min_region_size = 8.0f;
const r32 nav_merged_region_size = 20.0f;
const r32 nav_detail_sample_distance = 6.0f;
const r32 nav_detail_sample_max_error = 1.0f;
const r32 nav_resolution = 0.2f;
const r32 nav_walkable_slope = 45.0f; // degrees
const r32 nav_tile_size = 20.0f;
const s32 nav_max_layers = 32;
const s32 nav_expected_layers_per_tile = 4; // how many layers (or "floors") each navmesh tile is expected to have
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
		size.x = (s32)ceilf((bmax.x - bmin.x) / cell_size);
		size.y = (s32)ceilf((bmax.y - bmin.y) / cell_size);
		size.z = (s32)ceilf((bmax.z - bmin.z) / cell_size);
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
			(s32)((pos.x - vmin.x) / chunk_size),
			(s32)((pos.y - vmin.y) / chunk_size),
			(s32)((pos.z - vmin.z) / chunk_size),
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

#define AWK_NAV_MESH_ADJACENCY 48 // must be <= 64 because flags is a u64
struct AwkNavMeshNode
{
	u16 chunk;
	u16 vertex;

	inline b8 equals(const AwkNavMeshNode& other) const
	{
		return chunk == other.chunk && vertex == other.vertex;
	}
};

struct AwkNavMeshAdjacency
{
	// true = crawl
	// false = shoot
	u64 flags;
	StaticArray<AwkNavMeshNode, AWK_NAV_MESH_ADJACENCY> neighbors;
	b8 flag(s32) const;
	void flag(s32, b8);
	void remove(s32);
};

struct AwkNavMeshChunk
{
	Array<Vec3> vertices;
	Array<Vec3> normals;
	Array<AwkNavMeshAdjacency> adjacency;
};

struct AwkNavMesh : Chunks<AwkNavMeshChunk>
{
};


}
