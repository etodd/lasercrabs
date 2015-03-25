#pragma once

#include "types.h"
#include <GL/glew.h>
#include "data/mesh.h"
#include "asset.h"

struct Loader
{
	template<typename T>
	struct Entry
	{
		unsigned int refs;
		T data;
	};

	Loader();
	void clear();
	Entry<Mesh> meshes[Asset::Model::count];
	Entry<GLuint> textures[Asset::Texture::count];
	Entry<GLuint> shaders[Asset::Shader::count];

	Mesh* mesh(Asset::ID);
	void unload_mesh(Asset::ID);
	GLuint texture(Asset::ID);
	void unload_texture(Asset::ID);
	GLuint shader(Asset::ID);
	void unload_shader(Asset::ID);
};
