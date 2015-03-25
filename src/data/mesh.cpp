#include "mesh.h"
#include <GL/glew.h>

Mesh::GL::GL()
	: attribs()
{
	glGenVertexArrays(1, &vertex_array);
	glBindVertexArray(vertex_array);
	glGenBuffers(1, &index_buffer);
}

Mesh::GL::~GL()
{
	for (int i = 0; i < attribs.length; i++)
		glDeleteBuffers(1, &attribs.data[i].buffer);
	glDeleteVertexArrays(1, &vertex_array);
}

void Mesh::GL::set_indices(Array<int>* indices)
{
	glBindVertexArray(vertex_array);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices->length * sizeof(int), indices->data, GL_STATIC_DRAW);
	index_count = indices->length;
}
