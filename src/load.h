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
	static Entry<Mesh> meshes[Asset::Model::count];
	static Entry<Animation> animations[Asset::Animation::count];
	static Entry<void*> textures[Asset::Texture::count]; // Nothing actually stored
	static Entry<void*> shaders[Asset::Shader::count]; // Nothing actually stored
	static Entry<Font> fonts[Asset::Font::count];

	static Mesh* mesh(AssetID);
	static Mesh* mesh_permanent(AssetID);
	static void mesh_unload(AssetID);
	static Animation* animation(AssetID);
	static Animation* animation_permanent(AssetID);
	static void animation_unload(AssetID);
	static void texture(AssetID);
	static void texture_permanent(AssetID);
	static void texture_unload(AssetID);
	static void shader(AssetID);
	static void shader_permanent(AssetID);
	static void shader_unload(AssetID);
	static Font* font(AssetID);
	static Font* font_permanent(AssetID);
	static void font_unload(AssetID);
	static void unload_transients();
};