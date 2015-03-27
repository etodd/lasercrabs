#pragma once

#include "array.h"
#include "types.h"
#include <GL/glew.h>
#include "lmath.h"

#include <BulletCollision/CollisionShapes/btTriangleIndexVertexArray.h>

struct Mesh
{
	Array<int> indices;
	Array<Vec3> vertices;
	Array<Vec2> uvs;
	Array<Vec3> normals;
	btTriangleIndexVertexArray physics;
};

struct MeshGL
{
	struct Attrib
	{
		int element_size;
		GLuint type;
		GLuint handle;
	};

	Array<Attrib> attribs;
	GLuint index_buffer;
	GLuint vertex_array;
	size_t index_count;
};
