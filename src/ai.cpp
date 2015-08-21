#include "ai.h"
#include "data/import_common.h"
#include "load.h"
#include "asset/shader.h"
#include "DetourNavMesh.h"

#define DRAW_NAV_MESH 0

namespace VI
{

AssetID AI::render_mesh = AssetNull;
AssetID AI::nav_mesh_id = AssetNull;
bool AI::render_mesh_dirty = false;

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
	nav_mesh_id = id;
	Loader::nav_mesh(id);
	render_mesh_dirty = true;
}

void AI::draw(const RenderParams& p)
{
#if DRAW_NAV_MESH
	if (render_mesh_dirty)
	{
		// Convert polygon navmesh to triangle mesh

		dtNavMesh* nav_mesh = Loader::nav_mesh(nav_mesh_id);

		Array<Vec3>(nav_mesh->nverts);
		for (int i = 0; i < nav_mesh->nverts; i++)
		{
			const unsigned short* v = &nav_mesh->verts[i * 3];
			Vec3 vertex;
			vertex.x = mesh_origin[0] + v[0] * nav_mesh->cs;
			vertex.y = mesh_origin[1] + (v[1] + 1) * nav_mesh->ch;
			vertex.z = mesh_origin[2] + v[2] * nav_mesh->cs;
			output.vertices.add(vertex);
		}

		int num_triangles = 0;
		for (int i = 0; i < nav_mesh->npolys; ++i)
		{
			const unsigned short* poly = &nav_mesh->polys[i * nav_mesh->nvp * 2];
			for (int j = 2; j < nav_mesh->nvp; ++j)
			{
				if (poly[j] == RC_MESH_NULL_IDX)
					break;
				num_triangles++;
			}
		}

		output.indices.reserve(num_triangles * 3);
		for (int i = 0; i < nav_mesh->npolys; ++i)
		{
			const unsigned short* poly = &nav_mesh->polys[i * nav_mesh->nvp * 2];
			
			for (int j = 2; j < nav_mesh->nvp; ++j)
			{
				if (poly[j] == RC_MESH_NULL_IDX)
					break;
				output.indices.add(poly[0]);
				output.indices.add(poly[j - 1]);
				output.indices.add(poly[j]);
			}
		}

		// Allocate space for the normals, but just leave them all zeroes.
		output.normals.resize(output.vertices.length);

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
