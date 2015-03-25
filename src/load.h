#pragma once

#include "types.h"
#include <GL/glew.h>
#include "data/mesh.h"
#include "asset.h"

struct Loader
{
	Loader();
	Mesh meshes[Asset::Model::count];
	GLuint textures[Asset::Texture::count];
	GLuint shaders[Asset::Shader::count];

	Mesh* mesh(Asset::ID);
	GLuint texture(Asset::ID);
	GLuint shader(Asset::ID);
};
