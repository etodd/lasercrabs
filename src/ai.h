#pragma once
#include "types.h"
#include "render/render.h"

struct dtNavMesh;

namespace VI
{

struct AI
{
	static AssetID render_mesh;
	static AssetID nav_mesh_id;
	static bool render_mesh_dirty;
	static void init();
	static void load_nav_mesh(AssetID);
	static void draw(const RenderParams&);
};

}
