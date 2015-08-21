#pragma once
#include "types.h"
#include "render/render.h"

struct rcPolyMesh;

namespace VI
{

struct AI
{
	static rcPolyMesh* nav_mesh;
	static int render_mesh;
	static bool render_mesh_dirty;
	static void init();
	static bool build_nav_mesh();
	static void draw(const RenderParams&);
};

}
