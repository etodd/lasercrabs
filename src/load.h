#pragma once

#include "types.h"
#include <GL/glew.h>
#include "data/mesh.h"
#include "render/render.h"
#include "asset.h"

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

	static RenderSync::Swapper* swapper;
	// First entry in each array is empty
	static Entry<Mesh> meshes[Asset::Model::count];
	static Entry<void*> textures[Asset::Texture::count];
	static Entry<void*> shaders[Asset::Shader::count];

	static AssetID mesh(AssetID);
	static AssetID mesh_permanent(AssetID);
	static void unload_mesh(AssetID);
	static AssetID texture(AssetID);
	static AssetID texture_permanent(AssetID);
	static void unload_texture(AssetID);
	static AssetID shader(AssetID);
	static AssetID shader_permanent(AssetID);
	static void unload_shader(AssetID);
	static void unload_transients();
};
