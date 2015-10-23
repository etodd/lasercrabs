#include "ai.h"
#include "data/import_common.h"
#include "load.h"
#include "asset/shader.h"
#include "data/components.h"
#include "physics.h"
#include "BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"

namespace VI
{

AssetID AI::render_mesh = AssetNull;
dtNavMesh* AI::nav_mesh = 0;
dtNavMeshQuery* AI::nav_mesh_query = 0;
dtQueryFilter AI::default_query_filter = dtQueryFilter();
const float AI::default_search_extents[] = { 8, 8, 8 };
bool AI::render_mesh_dirty = false;

const Vec4 AI::colors[] =
{
	Vec4(0.5f, 1, 0.75f, 1),
	Vec4(1, 0.5f, 0.5f, 1),
	Vec4(0, 1, 1, 1),
	Vec4(1, 0, 1, 1),
	Vec4(0.3f, 0.3f, 0.3f, 1),
};

static const unsigned short RC_MESH_NULL_IDX = 0xffff;

void AI::init()
{
	nav_mesh_query = dtAllocNavMeshQuery();
	default_query_filter.setIncludeFlags((unsigned short)-1);
	default_query_filter.setExcludeFlags(0);

	render_mesh = Loader::dynamic_mesh_permanent(1);
	Loader::dynamic_mesh_attrib(RenderDataType_Vec3);
	Loader::shader_permanent(Asset::Shader::flat);
}

void AI::load_nav_mesh(AssetID id)
{
	nav_mesh = Loader::nav_mesh(id);
	render_mesh_dirty = true;

	dtStatus status = nav_mesh_query->init(nav_mesh, 2048);
	vi_assert(!dtStatusFailed(status));
}

void AI::debug_draw(const RenderParams& p)
{
#if DEBUG
	if (render_mesh_dirty)
	{
		// Convert polygon navmesh to triangle mesh

		rcPolyMesh mesh;
		Loader::base_nav_mesh(Loader::current_nav_mesh_id, &mesh);

		p.sync->write(RenderOp_UpdateAttribBuffers);
		p.sync->write(render_mesh);

		p.sync->write<int>(mesh.nverts);
		for (int i = 0; i < mesh.nverts; i++)
		{
			const unsigned short* v = &mesh.verts[i * 3];
			Vec3 vertex;
			vertex.x = mesh.bmin[0] + v[0] * mesh.cs;
			vertex.y = mesh.bmin[1] + (v[1] + 1) * mesh.ch;
			vertex.z = mesh.bmin[2] + v[2] * mesh.cs;
			p.sync->write<Vec3>(vertex);
		}

		int num_triangles = 0;
		for (int i = 0; i < mesh.npolys; ++i)
		{
			const unsigned short* poly = &mesh.polys[i * mesh.nvp * 2];
			for (int j = 2; j < mesh.nvp; ++j)
			{
				if (poly[j] == RC_MESH_NULL_IDX)
					break;
				num_triangles++;
			}
		}

		p.sync->write(RenderOp_UpdateIndexBuffer);
		p.sync->write(render_mesh);

		p.sync->write<int>(num_triangles * 3);
		
		for (int i = 0; i < mesh.npolys; ++i)
		{
			const unsigned short* poly = &mesh.polys[i * mesh.nvp * 2];
			
			for (int j = 2; j < mesh.nvp; ++j)
			{
				if (poly[j] == RC_MESH_NULL_IDX)
					break;
				p.sync->write<int>(poly[0]);
				p.sync->write<int>(poly[j - 1]);
				p.sync->write<int>(poly[j]);
			}
		}

		render_mesh_dirty = false;

		Loader::base_nav_mesh_free(&mesh);
	}

	p.sync->write(RenderOp_Shader);
	p.sync->write(Asset::Shader::flat);
	p.sync->write(p.technique);

	p.sync->write(RenderOp_Uniform);
	p.sync->write(Asset::Uniform::diffuse_color);
	p.sync->write(RenderDataType_Vec4);
	p.sync->write<int>(1);
	p.sync->write(Vec4(0, 1, 0, 1));

	Mat4 mvp = p.view_projection;

	p.sync->write(RenderOp_Uniform);
	p.sync->write(Asset::Uniform::mvp);
	p.sync->write(RenderDataType_Mat4);
	p.sync->write<int>(1);
	p.sync->write<Mat4>(mvp);

	p.sync->write(RenderOp_Mesh);
	p.sync->write(render_mesh);
#endif
}

Entity* AI::get_enemy(const AI::Team& team, const Vec3& pos, const Vec3& forward, const float radius, const float angle, const ComponentMask component_mask)
{
	float angle_dot = cosf(angle);
	for (auto i = World::components<AIAgent>().iterator(); !i.is_last(); i.next())
	{
		AIAgent* agent = i.item();
		if (agent->team == team || !(agent->entity()->component_mask & component_mask))
			continue;

		Vec3 enemy_pos = agent->get<Transform>()->absolute_pos();

		Vec3 to_enemy = enemy_pos - pos;
		float distance_to_enemy = to_enemy.length();

		bool visible = false;
		if (distance_to_enemy < radius)
		{
			to_enemy /= distance_to_enemy;

			float dot = forward.dot(to_enemy);
			if (dot > angle_dot)
			{
				btCollisionWorld::ClosestRayResultCallback rayCallback(pos, enemy_pos);
				rayCallback.m_flags = btTriangleRaycastCallback::EFlags::kF_FilterBackfaces
					| btTriangleRaycastCallback::EFlags::kF_KeepUnflippedNormal;
				rayCallback.m_collisionFilterMask = rayCallback.m_collisionFilterGroup = ~CollisionTarget;

				Physics::btWorld->rayTest(pos, enemy_pos, rayCallback);

				if (!rayCallback.hasHit() || rayCallback.m_collisionObject->getUserIndex() == agent->entity_id)
					return agent->entity();
			}
		}
	}
	return nullptr;
}

dtPolyRef AI::get_poly(const Vec3& pos, const float* search_extents)
{
	dtPolyRef result;

	AI::nav_mesh_query->findNearestPoly((float*)&pos, search_extents, &default_query_filter, &result, 0);

	return result;
}

void AIAgent::awake()
{
}

}
