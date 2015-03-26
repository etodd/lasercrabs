#pragma once

#include "types.h"
#include <GL/glew.h>
#include "data/mesh.h"
#include "asset.h"
#include "render/render.h"

struct Loader
{
	template<typename T>
	struct Entry
	{
		unsigned int refs;
		T data;
	};

	Swapper* swapper;
	Entry<Mesh> meshes[Asset::Model::count];
	MeshGL gl_meshes[Asset::Model::count];
	Entry<GLuint> textures[Asset::Texture::count];
	Entry<GLuint> shaders[Asset::Shader::count];

	Loader(Swapper*);

	Asset::ID mesh(Asset::ID);
	void unload_mesh(Asset::ID);
	Asset::ID texture(Asset::ID);
	void unload_texture(Asset::ID);
	Asset::ID shader(Asset::ID);
	void unload_shader(Asset::ID);
};
