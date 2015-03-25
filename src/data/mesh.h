#pragma once

#include "array.h"
#include "types.h"
#include <GL/glew.h>

#include <BulletCollision/CollisionShapes/btTriangleIndexVertexArray.h>

struct Mesh
{
	struct GL
	{
		struct Attrib
		{
			int element_size;
			GLuint type;
			GLuint buffer;
		};

		GL();
		~GL();
		Array<Attrib> attribs;
		GLuint index_buffer;
		GLuint vertex_array;
		int index_count;

		template<typename T> void add_attrib(Array<T>* data, GLuint type)
		{
			Attrib a;
			a.element_size = sizeof(T) / 4;
			a.type = type;
			glBindVertexArray(vertex_array);
			glGenBuffers(1, &a.buffer);
			glBindBuffer(GL_ARRAY_BUFFER, a.buffer);
			glBufferData(GL_ARRAY_BUFFER, data->length * sizeof(T), data->data, GL_STATIC_DRAW);
			attribs.add(a);
		}

		void set_indices(Array<int>* indices);

	};

	Array<int> indices;
	Array<Vec3> vertices;
	Array<Vec2> uvs;
	Array<Vec3> normals;
	btTriangleIndexVertexArray physics;
	GL gl;
};
