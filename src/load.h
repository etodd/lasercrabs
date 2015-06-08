#pragma once

#include "types.h"
#include <GL/glew.h>
#include "data/mesh.h"
#include "asset.h"
#include "render/render.h"

struct Loader
{
	enum AssetType { AssetNone, AssetTransient, AssetPermanent };
	template<typename T>
	struct Entry
	{
		AssetType type;
		T data;
		Entry()
			: type(), data()
		{
		}
	};

	RenderSync::Swapper* swapper;

	// First entry in each array is empty
	Entry<Mesh> meshes[Asset::Model::count];
	Entry<void*> textures[Asset::Texture::count];
	Entry<void*> shaders[Asset::Shader::count];

	Loader(RenderSync::Swapper*);

	Asset::ID mesh(Asset::ID);
	Asset::ID mesh_permanent(Asset::ID);
	void unload_mesh(Asset::ID);
	Asset::ID texture(Asset::ID);
	Asset::ID texture_permanent(Asset::ID);
	void unload_texture(Asset::ID);
	Asset::ID shader(Asset::ID);
	Asset::ID shader_permanent(Asset::ID);
	void unload_shader(Asset::ID);
	void unload_transients();
};
