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
		size_t refs;
		T data;
		Entry()
			: refs(), data()
		{
		}
	};

	RenderSync::Swapper* swapper;
	Entry<Mesh> meshes[Asset::Model::count];
	Entry<void*> textures[Asset::Texture::count];
	Entry<void*> shaders[Asset::Shader::count];

	Loader(RenderSync::Swapper*);

	Asset::ID mesh(Asset::ID);
	void unload_mesh(Asset::ID);
	Asset::ID texture(Asset::ID);
	void unload_texture(Asset::ID);
	Asset::ID shader(Asset::ID);
	void unload_shader(Asset::ID);
};
