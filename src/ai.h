#pragma once
#include "types.h"
#include "render/render.h"

struct dtNavMesh;
struct dtNavMeshQuery;

namespace VI
{

struct AI
{
	static AssetID render_mesh;
	static dtNavMesh* nav_mesh;
	static dtNavMeshQuery* nav_mesh_query;
	static bool render_mesh_dirty;
	static void init();
	static void load_nav_mesh(AssetID);
	static void draw(const RenderParams&);
};

}
