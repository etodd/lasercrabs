#pragma once

#include "array.h"
#include "types.h"
#include <GL/glew.h>
#include "lmath.h"

#include <BulletCollision/CollisionShapes/btTriangleIndexVertexArray.h>

#define MAX_BONE_WEIGHTS 4

struct Mesh
{
	Array<int> indices;
	Array<Vec3> vertices;
	Array<Vec2> uvs;
	Array<Vec3> normals;
	Array<int> bone_indices[MAX_BONE_WEIGHTS];
	Array<float> bone_weights[MAX_BONE_WEIGHTS];
	Array<Mat4> inverse_bind_pose;
	Array<int> bone_hierarchy;
	btTriangleIndexVertexArray physics;
};
