#pragma once

#include "types.h"
#include <GL/glew.h>
#include "data/import_common.h"
#include "render/render.h"
#include "cJSON.h"

struct dtNavMesh;

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
	static void init(RenderSync::Swapper*);
	static Array<Entry<Mesh> > meshes;
	static Array<Entry<Animation> > animations;
	static Array<Entry<Armature> > armatures;
	static Array<Entry<void*> > textures; // Nothing actually stored
	static Array<Entry<void*> > shaders; // Nothing actually stored
	static Array<Entry<Font> > fonts;
	static Array<Entry<void*> > dynamic_meshes; // Nothing actually stored
	static dtNavMesh* current_nav_mesh;
	static AssetID current_nav_mesh_id;

	static Mesh* mesh(AssetID);
	static Mesh* mesh_permanent(AssetID);
	static void mesh_free(int);

	static AssetID mesh_ref_to_id(AssetID, AssetRef);

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

	static bool has_metadata(AssetID, AssetID, AssetID);
	static bool get_metadata(AssetID, AssetID, int, AssetID&, float&);

	static void texture(AssetID);
	static void texture_permanent(AssetID);
	static void texture_free(AssetID);

	static void shader(AssetID);
	static void shader_permanent(AssetID);
	static void shader_free(AssetID);

	static Font* font(AssetID);
	static Font* font_permanent(AssetID);
	static void font_free(AssetID);

	static dtNavMesh* nav_mesh(AssetID);

	static cJSON* level(AssetID);
	static void level_free(cJSON*);

	static void transients_free();
};

}
