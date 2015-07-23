#pragma once

#include "array.h"
#include "types.h"
#include <GL/glew.h>
#include "lmath.h"
#include <array>

#include <BulletCollision/CollisionShapes/btTriangleIndexVertexArray.h>

namespace VI
{

#define MAX_BONE_WEIGHTS 4


struct Mesh
{
	Array<int> indices;
	Array<Vec3> vertices;
	Array<Vec2> uvs;
	Array<Vec3> normals;
	Array<std::array<int, MAX_BONE_WEIGHTS> > bone_indices;
	Array<std::array<float, MAX_BONE_WEIGHTS> > bone_weights;
	Array<Mat4> inverse_bind_pose;
	Array<int> bone_hierarchy;
	btTriangleIndexVertexArray physics;
};

template<typename T>
struct Keyframe
{
	float time;
	T value;
};

struct Channel
{
	Array<Keyframe<Vec3> > positions;
	Array<Keyframe<Quat> > rotations;
	Array<Keyframe<Vec3> > scales;
	Mat4 current_transform;
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
		int start_index;
		int indices;
		Vec2 min;
		Vec2 max;
	};
	Character characters[sizeof(char)];
	Array<int> indices;
	Array<Vec3> vertices;
};

}