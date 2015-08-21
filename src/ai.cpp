#include "ai.h"
#include "Recast.h"
#include <cmath>
#include <cstring>
#include "physics.h"
#include "data/mesh.h"
#include "data/components.h"
#include "load.h"
#include "asset/shader.h"

#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>

#define DRAW_NAV_MESH 0

namespace VI
{

int AI::render_mesh = AssetNull;
bool AI::render_mesh_dirty = false;
rcPolyMesh* AI::nav_mesh = 0;

void AI::init()
{
#if DRAW_NAV_MESH
	render_mesh = Loader::dynamic_mesh_permanent(1);
	Loader::dynamic_mesh_attrib(RenderDataType_Vec3);
	Loader::shader_permanent(Asset::Shader::flat);
#endif
}

bool AI::build_nav_mesh()
{
	const float agent_height = 2.0f;
	const float agent_max_climb = 0.9f;
	const float agent_radius = 0.6f;
	const float edge_max_length = 12.0f;
	const float min_region_size = 8.0f;
	const float merged_region_size = 20.0f;
	const float detail_sample_distance = 6.0f;
	const float detail_sample_max_error = 1.0f;

	rcConfig cfg;
	memset(&cfg, 0, sizeof(cfg));
	cfg.cs = 0.2f;
	cfg.ch = 0.2f;
	cfg.walkableSlopeAngle = 45.0f;
	cfg.walkableHeight = (int)ceilf(agent_height / cfg.ch);
	cfg.walkableClimb = (int)floorf(agent_max_climb / cfg.ch);
	cfg.walkableRadius = (int)ceilf(agent_radius / cfg.cs);
	cfg.maxEdgeLen = (int)(edge_max_length / cfg.cs);
	cfg.maxSimplificationError = 2;
	cfg.minRegionArea = (int)rcSqr(min_region_size);		// Note: area = size*size
	cfg.mergeRegionArea = (int)rcSqr(merged_region_size);	// Note: area = size*size
	cfg.maxVertsPerPoly = 6;
	cfg.detailSampleDist = detail_sample_distance < 0.9f ? 0 : cfg.cs * detail_sample_distance;
	cfg.detailSampleMaxError = cfg.ch * detail_sample_max_error;

	Vec3 bounds_min(FLT_MAX, FLT_MAX, FLT_MAX);
	Vec3 bounds_max(FLT_MIN, FLT_MIN, FLT_MIN);

	Array<Vec3> verts;
	Array<int> indices;

	int current_index = 0;
	for (auto i = World::components<RigidBody>().iterator(); !i.is_last(); i.next())
	{
		btRigidBody* body = i.item()->btBody;
		if (body->isActive())
		{
			if (body->isStaticOrKinematicObject())
			{
				btBvhTriangleMeshShape* shape = dynamic_cast<btBvhTriangleMeshShape*>(body->getCollisionShape());
				if (shape)
				{
					btVector3 min, max;
					body->getAabb(min, max);
					bounds_min.x = fmin(min.getX(), bounds_min.x);
					bounds_min.y = fmin(min.getY(), bounds_min.y);
					bounds_min.z = fmin(min.getZ(), bounds_min.z);
					bounds_max.x = fmax(max.getX(), bounds_max.x);
					bounds_max.y = fmax(max.getY(), bounds_max.y);
					bounds_max.z = fmax(max.getZ(), bounds_max.z);

					Mat4 mat;
					i.item()->get<Transform>()->mat(&mat);

					Mesh* mesh = Loader::mesh(shape->getUserIndex());
					verts.reserve(verts.length + mesh->vertices.length);
					indices.reserve(indices.length + mesh->indices.length);

					for (int i = 0; i < mesh->vertices.length; i++)
					{
						Vec3 v = mesh->vertices[i];
						Vec4 v2 = mat * Vec4(v.x, v.y, v.z, 1);
						verts.add(Vec3(v2.x, v2.y, v2.z));
					}
					for (int i = 0; i < mesh->indices.length; i++)
						indices.add(current_index + mesh->indices[i]);
					current_index = verts.length;
				}
			}
		}
	}

	rcVcopy(cfg.bmin, (float*)&bounds_min);
	rcVcopy(cfg.bmax, (float*)&bounds_max);
	rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);

	rcContext ctx(false);

	rcHeightfield* heightfield = rcAllocHeightfield();
	if (!heightfield)
		return false;

	if (!rcCreateHeightfield(&ctx, *heightfield, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch))
		return false;

	// Rasterize input polygon soup.
	// Find triangles which are walkable based on their slope and rasterize them.
	{
		Array<unsigned char> tri_areas(indices.length / 3, indices.length / 3);
		rcMarkWalkableTriangles(&ctx, cfg.walkableSlopeAngle, (float*)verts.data, verts.length, indices.data, indices.length / 3, tri_areas.data);
		rcRasterizeTriangles(&ctx, (float*)verts.data, verts.length, indices.data, tri_areas.data, indices.length / 3, *heightfield, cfg.walkableClimb);
	}

	// Once all geoemtry is rasterized, we do initial pass of filtering to
	// remove unwanted overhangs caused by the conservative rasterization
	// as well as filter spans where the character cannot possibly stand.
	rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *heightfield);
	rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *heightfield);
	rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *heightfield);

	// Partition walkable surface to simple regions.

	// Compact the heightfield so that it is faster to handle from now on.
	rcCompactHeightfield* compact_heightfield = rcAllocCompactHeightfield();
	if (!compact_heightfield)
		return false;
	if (!rcBuildCompactHeightfield(&ctx, cfg.walkableHeight, cfg.walkableClimb, *heightfield, *compact_heightfield))
		return false;
	rcFreeHeightField(heightfield);

	// Erode the walkable area by agent radius.
	if (!rcErodeWalkableArea(&ctx, cfg.walkableRadius, *compact_heightfield))
		return false;

	// Monotone region partition
	if (!rcBuildRegionsMonotone(&ctx, *compact_heightfield, 0, cfg.minRegionArea, cfg.mergeRegionArea))
		return false;

	// Trace and simplify region contours.
	
	// Create contours.
	rcContourSet* contour_set = rcAllocContourSet();
	if (!contour_set)
		return false;

	if (!rcBuildContours(&ctx, *compact_heightfield, cfg.maxSimplificationError, cfg.maxEdgeLen, *contour_set))
		return false;
	
	// Build polygon navmesh from the contours.
	nav_mesh = rcAllocPolyMesh();
	if (!nav_mesh)
		return false;

	if (!rcBuildPolyMesh(&ctx, *contour_set, cfg.maxVertsPerPoly, *nav_mesh))
		return false;
	
	/*
	// Create detail mesh which allows to access approximate height on each polygon.
	
	rcPolyMeshDetail* detail_mesh = rcAllocPolyMeshDetail();
	if (!detail_mesh)
		return false;

	if (!rcBuildPolyMeshDetail(&ctx, *nav_mesh, *compact_heightfield, cfg.detailSampleDist, cfg.detailSampleMaxError, *detail_mesh))
		return false;
	*/

	rcFreeCompactHeightfield(compact_heightfield);
	rcFreeContourSet(contour_set);

	render_mesh_dirty = true;

	return true;
}

void AI::draw(const RenderParams& p)
{
#if DRAW_NAV_MESH
	if (render_mesh_dirty)
	{
		p.sync->write(RenderOp_UpdateAttribBuffers);
		p.sync->write(render_mesh);

		p.sync->write<int>(nav_mesh->nverts);
		const float* mesh_origin = nav_mesh->bmin;

		for (int i = 0; i < nav_mesh->nverts; i++)
		{
			const unsigned short* v = &nav_mesh->verts[i * 3];
			Vec3 vertex;
			vertex.x = mesh_origin[0] + v[0] * nav_mesh->cs;
			vertex.y = mesh_origin[1] + (v[1] + 1) * nav_mesh->ch;
			vertex.z = mesh_origin[2] + v[2] * nav_mesh->cs;
			p.sync->write(vertex);
		}

		p.sync->write(RenderOp_UpdateIndexBuffer);
		p.sync->write(render_mesh);

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

		p.sync->write<int>(num_triangles * 3); // Number of indices
		for (int i = 0; i < nav_mesh->npolys; ++i)
		{
			const unsigned short* poly = &nav_mesh->polys[i * nav_mesh->nvp * 2];
			
			for (int j = 2; j < nav_mesh->nvp; ++j)
			{
				if (poly[j] == RC_MESH_NULL_IDX)
					break;
				p.sync->write<int>(poly[0]);
				p.sync->write<int>(poly[j - 1]);
				p.sync->write<int>(poly[j]);
			}
		}
		
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
