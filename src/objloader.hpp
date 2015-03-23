#ifndef OBJLOADER_H
#define OBJLOADER_H
#include <vector>
#include <glm/glm.hpp>
#include "array.hpp"

bool loadAssImp(
	const char * path, 
	Array<unsigned short>& indices,
	Array<glm::vec3>& vertices,
	Array<glm::vec2>& uvs,
	Array<glm::vec3>& normals
);

#endif
