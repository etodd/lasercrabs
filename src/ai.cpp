#include "ai.h"
#include "data/import_common.h"
#include "load.h"
#include "asset/shader.h"

#define DRAW_NAV_MESH 1

namespace VI
{

AssetID AI::render_mesh = AssetNull;
bool AI::render_mesh_dirty = false;
Mesh* AI::nav_mesh = 0;

void AI::init()
{
#if DRAW_NAV_MESH
	render_mesh = Loader::dynamic_mesh_permanent(1);
	Loader::dynamic_mesh_attrib(RenderDataType_Vec3);
	Loader::shader_permanent(Asset::Shader::flat);
#endif
}

void AI::load_nav_mesh(AssetID id)
{
	nav_mesh = Loader::nav_mesh(id);
	if (nav_mesh)
		render_mesh_dirty = true;
}

void AI::draw(const RenderParams& p)
{
#if DRAW_NAV_MESH
	if (render_mesh_dirty)
	{
		p.sync->write(RenderOp_UpdateAttribBuffers);
		p.sync->write(render_mesh);

		p.sync->write<int>(nav_mesh->vertices.length);
		p.sync->write<Vec3>(nav_mesh->vertices.data, nav_mesh->vertices.length);

		p.sync->write(RenderOp_UpdateIndexBuffer);
		p.sync->write(render_mesh);

		p.sync->write<int>(nav_mesh->indices.length);
		p.sync->write<int>(nav_mesh->indices.data, nav_mesh->indices.length);
		
		render_mesh_dirty = false;
	}

	p.sync->write(RenderOp_Mesh);
	p.sync->write(render_mesh);
	p.sync->write(Asset::Shader::flat);

	p.sync->write<int>(2); // Uniform count

	p.sync->write(Asset::Uniform::diffuse_color);
	p.sync->write(RenderDataType_Vec4);
	p.sync->write<int>(1);
	p.sync->write(Vec4(0, 1, 0, 1));

	Mat4 mvp = p.view_projection;

	p.sync->write(Asset::Uniform::mvp);
	p.sync->write(RenderDataType_Mat4);
	p.sync->write<int>(1);
	p.sync->write(&mvp);
#endif
}

}
