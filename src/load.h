#pragma once

#include "types.h"
#include <GL/glew.h>
#include "data/mesh.h"
#include "render/render.h"
#include "asset.h"

namespace VI
{

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
	static Entry<Armature> armatures[Asset::Armature::count];
	static Entry<void*> textures[Asset::Texture::count]; // Nothing actually stored
	static Entry<void*> shaders[Asset::Shader::count]; // Nothing actually stored
	static Entry<Font> fonts[Asset::Font::count];
	static Array<Entry<void*>> dynamic_meshes; // Nothing actually stored

	static Mesh* mesh(AssetID);
	static Mesh* mesh_permanent(AssetID);
	static void mesh_free(int);

	static int dynamic_mesh(int);
	static void dynamic_mesh_attrib(RenderDataType, int = 1);
	static int dynamic_mesh_permanent(int);
	static void dynamic_mesh_free(int);

	static Animation* animation(AssetID);
	static Animation* animation_permanent(AssetID);
	static void animation_free(AssetID);

	static Armature* armature(AssetID);
	static Armature* armature_permanent(AssetID);
	static void armature_free(AssetID);

	static void texture(AssetID);
	static void texture_permanent(AssetID);
	static void texture_free(AssetID);

	static void shader(AssetID);
	static void shader_permanent(AssetID);
	static void shader_free(AssetID);

	static Font* font(AssetID);
	static Font* font_permanent(AssetID);
	static void font_free(AssetID);

	static void transients_free();
};

}
