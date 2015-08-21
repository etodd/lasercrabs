#pragma once

#include "array.h"
#include "types.h"
#include <GL/glew.h>
#include "lmath.h"
#include <array>

namespace VI
{

#define MAX_BONE_WEIGHTS 4

enum RenderDataType
{
	RenderDataType_Float,
	RenderDataType_Vec2,
	RenderDataType_Vec3,
	RenderDataType_Vec4,
	RenderDataType_Int,
	RenderDataType_Mat4,
	RenderDataType_Texture,
};

inline size_t render_data_type_size(RenderDataType type)
{
	switch (type)
	{
		case RenderDataType_Float:
			return sizeof(float);
		case RenderDataType_Vec2:
			return sizeof(Vec2);
		case RenderDataType_Vec3:
			return sizeof(Vec3);
		case RenderDataType_Vec4:
			return sizeof(Vec4);
		case RenderDataType_Int:
			return sizeof(int);
		case RenderDataType_Mat4:
			return sizeof(Mat4);
		case RenderDataType_Texture:
			return sizeof(int);
	}
	vi_assert(false);
	return 0;
}

// Can't have more than X meshes parented to a bone in a .arm file
struct Bone
{
	Quat rot;
	Vec3 pos;
};

struct Armature
{
	Array<int> hierarchy;
	Array<Bone> bind_pose;
	Armature()
		: hierarchy(), bind_pose()
	{

	}
};

struct Mesh
{
	Array<int> indices;
	Array<Vec3> vertices;
	Array<Vec3> normals;
	Array<Mat4> inverse_bind_pose;
	Armature armature;
	Vec3 bounds_min;
	Vec3 bounds_max;
	Vec4 color;
	void reset()
	{
		indices.length = 0;
		vertices.length = 0;
		normals.length = 0;
		inverse_bind_pose.length = 0;
		armature.hierarchy.length = 0;
		armature.bind_pose.length = 0;
	}
};

template<typename T>
struct Keyframe
{
	float time;
	T value;
};

struct Channel
{
	int bone_index;
	Array<Keyframe<Vec3> > positions;
	Array<Keyframe<Quat> > rotations;
	Array<Keyframe<Vec3> > scales;
};

struct Animation
{
	float duration;
	Array<Channel> channels;
};

struct Font
{
	struct Character
	{
		char code;
		int index_start;
		int index_count;
		int vertex_start;
		int vertex_count;
		Vec2 min;
		Vec2 max;
	};
	Array<Character> characters;
	Array<int> indices;
	Array<Vec3> vertices;
	Font()
		: characters(), indices(), vertices()
	{
	}
};

}
