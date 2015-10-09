#pragma once

#include "types.h"
#include "data/import_common.h"
#include "render/render.h"

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

	static RenderSwapper* swapper;
	static void init(RenderSwapper*);
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
	static void base_nav_mesh(AssetID, rcPolyMesh*);
	static void base_nav_mesh_free(rcPolyMesh*);

	static cJSON* level(AssetID);
	static void level_free(cJSON*);

	static void transients_free();

	static AssetID find(const char*, const char**);
};

}
