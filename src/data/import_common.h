#pragma once

#include "array.h"
#include "lmath.h"
#include <array>

struct cJSON;

namespace VI
{

#define MAX_BONE_WEIGHTS 4

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

// Can't have more than X meshes parented to a bone in a .arm file
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

	Character& get(const void*);
};

}
