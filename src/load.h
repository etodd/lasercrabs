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
	u16* verts;	///< The mesh vertices. [Form: (x, y, z) * #nverts]
	u16* polys;	///< Polygon and neighbor data. [Length: #maxpolys * 2 * #nvp]
	u16* regs;	///< The region id assigned to each polygon. [Length: #maxpolys]
	u16* flags;	///< The user defined flags for each polygon. [Length: #maxpolys]
	u8* areas;	///< The area id assigned to each polygon. [Length: #maxpolys]
	s32 nverts;				///< The number of vertices.
	s32 npolys;				///< The number of polygons.
	s32 maxpolys;			///< The number of allocated polygons.
	s32 nvp;				///< The maximum number of vertices per polygon.
	r32 bmin[3];			///< The minimum bounds in world space. [(x, y, z)]
	r32 bmax[3];			///< The maximum bounds in world space. [(x, y, z)]
	r32 cs;				///< The size of each cell. (On the xz-plane.)
	r32 ch;				///< The height of each cell. (The minimum increment along the y-axis.)
	s32 borderSize;			///< The AABB border size used to generate the source data from which the mesh was derived.
};

struct Settings
{
	struct Bindings
	{
		InputBinding forward;
		InputBinding backward;
		InputBinding left;
		InputBinding right;
		InputBinding jump;
		InputBinding primary;
		InputBinding secondary;
		InputBinding walk;
		InputBinding parkour;
	};

	Bindings bindings;
	b8 valid;
	s32 width;
	s32 height;
	b8 fullscreen;
	u8 sfx;
	u8 music;
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

	static s32 static_mesh_count;
	static s32 static_texture_count;
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

	static Mesh* mesh(AssetID);
	static Mesh* mesh_permanent(AssetID);
	static Mesh* mesh_instanced(AssetID);
	static void mesh_free(s32);

	static s32 dynamic_mesh(s32, b8 dynamic = true);
	static void dynamic_mesh_attrib(RenderDataType, s32 = 1);
	static s32 dynamic_mesh_permanent(s32, b8 dynamic = true);
	static void dynamic_mesh_free(s32);

	static Animation* animation(AssetID);
	static Animation* animation_permanent(AssetID);
	static void animation_free(AssetID);

	static Armature* armature(AssetID);
	static Armature* armature_permanent(AssetID);
	static void armature_free(AssetID);

	static void texture(AssetID, RenderTextureWrap = RenderTextureWrap::Repeat, RenderTextureFilter = RenderTextureFilter::Linear);
	static void texture_permanent(AssetID, RenderTextureWrap = RenderTextureWrap::Repeat, RenderTextureFilter = RenderTextureFilter::Linear);
	static void texture_free(AssetID);

	static s32 dynamic_texture(s32, s32, RenderDynamicTextureType, RenderTextureWrap = RenderTextureWrap::Clamp, RenderTextureFilter = RenderTextureFilter::Nearest, RenderTextureCompare = RenderTextureCompare::None);
	static s32 dynamic_texture_permanent(s32, s32, RenderDynamicTextureType, RenderTextureWrap = RenderTextureWrap::Clamp, RenderTextureFilter = RenderTextureFilter::Nearest, RenderTextureCompare = RenderTextureCompare::None);
	static void dynamic_texture_free(s32);

	static s32 framebuffer(s32);
	static void framebuffer_attach(RenderFramebufferAttachment, s32);
	static s32 framebuffer_permanent(s32);
	static void framebuffer_free(s32);

	static void shader(AssetID);
	static void shader_permanent(AssetID);
	static void shader_free(AssetID);

	static Font* font(AssetID);
	static Font* font_permanent(AssetID);
	static void font_free(AssetID);

	static dtNavMesh* nav_mesh(AssetID);
	static void base_nav_mesh(AssetID, rcPolyMesh*);
	static void base_nav_mesh_free(rcPolyMesh*);

	static cJSON* level(AssetID);
	static void level_free(cJSON*);

	static b8 soundbank(AssetID);
	static b8 soundbank_permanent(AssetID);
	static void soundbank_free(AssetID);
	
	static Settings& settings();
	static void settings_save();

	static void transients_free();

	static AssetID find(const char*, const char**);
};

}
