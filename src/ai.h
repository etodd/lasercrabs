#pragma once
#include "types.h"
#include "render/render.h"

namespace VI
{

struct AI
{
	static AssetID render_mesh;
	static bool render_mesh_dirty;
	static Mesh* nav_mesh;
	static void init();
	static void load_nav_mesh(AssetID);
	static void draw(const RenderParams&);
};

}
