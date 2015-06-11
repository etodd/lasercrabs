#pragma once

#include "types.h"
#include <GL/glew.h>
#include "data/mesh.h"
#include "render/render.h"
#include "asset.h"

struct Loader
{
	static Loader main;
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

	Loader();

	AssetID mesh(AssetID);
	AssetID mesh_permanent(AssetID);
	void unload_mesh(AssetID);
	AssetID texture(AssetID);
	AssetID texture_permanent(AssetID);
	void unload_texture(AssetID);
	AssetID shader(AssetID);
	AssetID shader_permanent(AssetID);
	void unload_shader(AssetID);
	void unload_transients();
};
