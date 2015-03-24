#ifndef LOAD_H
#define LOAD_H

#include <glm/glm.hpp>
#include "array.hpp"
#include <GL/glew.h>

bool load_mdl(
	const char* path, 
	Array<int>& indices,
	Array<glm::vec3>& vertices,
	Array<glm::vec2>& uvs,
	Array<glm::vec3>& normals
);

GLuint load_png(const char* imagepath);

#endif
