#pragma once

#include "types.h"
#include "data/import_common.h"
#include "render/render.h"
#include <AK/SoundEngine/Common/AkTypes.h>

struct dtNavMesh;
struct cJSON;

namespace VI
{

/// Represents a polygon mesh suitable for use in building a navigation mesh. 
struct rcPolyMesh
{
	unsigned short* verts;	///< The mesh vertices. [Form: (x, y, z) * #nverts]
	unsigned short* polys;	///< Polygon and neighbor data. [Length: #maxpolys * 2 * #nvp]
	unsigned short* regs;	///< The region id assigned to each polygon. [Length: #maxpolys]
	unsigned short* flags;	///< The user defined flags for each polygon. [Length: #maxpolys]
	unsigned char* areas;	///< The area id assigned to each polygon. [Length: #maxpolys]
	int nverts;				///< The number of vertices.
	int npolys;				///< The number of polygons.
	int maxpolys;			///< The number of allocated polygons.
	int nvp;				///< The maximum number of vertices per polygon.
	float bmin[3];			///< The minimum bounds in world space. [(x, y, z)]
	float bmax[3];			///< The maximum bounds in world space. [(x, y, z)]
	float cs;				///< The size of each cell. (On the xz-plane.)
	float ch;				///< The height of each cell. (The minimum increment along the y-axis.)
	int borderSize;			///< The AABB border size used to generate the source data from which the mesh was derived.
};

struct Settings
{
	struct Bindings
	{
		InputBinding forward;
		InputBinding backward;
		InputBinding left;
		InputBinding right;
		InputBinding up;
		InputBinding down;
		InputBinding primary;
		InputBinding secondary;
	};

	Bindings bindings;
	bool valid;
	int width;
	int height;
	bool fullscreen;
};

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

	static int static_mesh_count;
	static int static_texture_count;
	static LoopSwapper* swapper;
	static void init(LoopSwapper*);
	static Array<Entry<Mesh> > meshes;
	static Array<Entry<Animation> > animations;
	static Array<Entry<Armature> > armatures;
	static Array<Entry<void*> > textures; // Nothing actually stored
	static Array<Entry<void*> > shaders; // Nothing actually stored
	static Array<Entry<Font> > fonts;
	static Array<Entry<void*> > dynamic_meshes; // Nothing actually stored
	static Array<Entry<void*> > dynamic_textures; // Nothing actually stored
	static Array<Entry<void*> > framebuffers; // Nothing actually stored
	static Array<Entry<AkBankID> > soundbanks;
	static Settings settings_data;
	static dtNavMesh* current_nav_mesh;
	static AssetID current_nav_mesh_id;

	static Mesh* mesh(const AssetID);
	static Mesh* mesh_permanent(const AssetID);
	static Mesh* mesh_instanced(const AssetID);
	static void mesh_free(const int);

	static int dynamic_mesh(const int, const bool dynamic = true);
	static void dynamic_mesh_attrib(RenderDataType, const int = 1);
	static int dynamic_mesh_permanent(const int, const bool dynamic = true);
	static void dynamic_mesh_free(const int);

	static Animation* animation(const AssetID);
	static Animation* animation_permanent(const AssetID);
	static void animation_free(const AssetID);

	static Armature* armature(const AssetID);
	static Armature* armature_permanent(const AssetID);
	static void armature_free(const AssetID);

	static void texture(const AssetID);
	static void texture_permanent(const AssetID);
	static void texture_free(const AssetID);

	static int dynamic_texture(const int, const int, const RenderDynamicTextureType, const RenderTextureFilter = RenderTextureFilter::Nearest);
	static int dynamic_texture_permanent(const int, const int, const RenderDynamicTextureType, const RenderTextureFilter = RenderTextureFilter::Nearest);
	static void dynamic_texture_free(const int);

	static int framebuffer(const int);
	static void framebuffer_attach(const RenderFramebufferAttachment, const int);
	static int framebuffer_permanent(const int);
	static void framebuffer_free(const int);

	static void shader(const AssetID);
	static void shader_permanent(const AssetID);
	static void shader_free(const AssetID);

	static Font* font(const AssetID);
	static Font* font_permanent(const AssetID);
	static void font_free(const AssetID);

	static dtNavMesh* nav_mesh(const AssetID);
	static void base_nav_mesh(const AssetID, rcPolyMesh*);
	static void base_nav_mesh_free(rcPolyMesh*);

	static cJSON* level(const AssetID);
	static void level_free(cJSON*);

	static bool soundbank(const AssetID);
	static bool soundbank_permanent(const AssetID);
	static void soundbank_free(const AssetID);
	
	static Settings& settings();
	static void settings_save();

	static void transients_free();

	static AssetID find(const char*, const char**);
};

}
