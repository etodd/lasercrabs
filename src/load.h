#pragma once

#include "types.h"
#include <GL/glew.h>

#include "array.h"

bool load_mdl(
	const char* path, 
	Array<int>& indices,
	Array<Vec3>& vertices,
	Array<Vec2>& uvs,
	Array<Vec3>& normals
);

GLuint load_png(const char* imagepath);

GLuint load_shader(const char* vertex_file_path, const char* fragment_file_path);
