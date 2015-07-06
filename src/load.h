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
	static Entry<Animation> animations[Asset::Animation::count];
	static Entry<void*> textures[Asset::Texture::count]; // Nothing actually stored
	static Entry<void*> shaders[Asset::Shader::count]; // Nothing actually stored

	static Mesh* mesh(AssetID);
	static Mesh* mesh_permanent(AssetID);
	static void unload_mesh(AssetID);
	static Animation* animation(AssetID);
	static Animation* animation_permanent(AssetID);
	static void unload_animation(AssetID);
	static void texture(AssetID);
	static void texture_permanent(AssetID);
	static void unload_texture(AssetID);
	static void shader(AssetID);
	static void shader_permanent(AssetID);
	static void unload_shader(AssetID);
	static void unload_transients();
};