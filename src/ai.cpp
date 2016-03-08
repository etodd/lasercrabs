#include "ai.h"
#include "data/import_common.h"
#include "load.h"
#include "asset/shader.h"
#include "data/components.h"
#include "physics.h"
#include "bullet/src/BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "game/entities.h"
#include "recast/Recast/Include/Recast.h"
#include "recast/Detour/Include/DetourNavMeshBuilder.h"

namespace VI
{

AssetID AI::render_mesh = AssetNull;
dtNavMesh* AI::nav_mesh = nullptr;
dtTileCache* AI::nav_tile_cache = nullptr;
dtNavMeshQuery* AI::nav_mesh_query = nullptr;
dtQueryFilter AI::default_query_filter = dtQueryFilter();
const r32 AI::default_search_extents[] = { 8, 8, 8 };
b8 AI::render_mesh_dirty = false;

AI::Goal::Goal()
	: entity(), has_entity(true)
{
}

b8 AI::Goal::valid() const
{
	return !has_entity || entity.ref();
}

Vec3 AI::Goal::get_pos() const
{
	Entity* e = entity.ref();
	if (has_entity && e)
		return e->get<Transform>()->absolute_pos();
	else
		return pos;
}

void AI::Goal::set(const Vec3& p)
{
	pos = p;
	has_entity = false;
}

void AI::Goal::set(Entity* e)
{
	entity = e;
	has_entity = true;
}

void NavMeshProcess::process(struct dtNavMeshCreateParams* params, unsigned char* polyAreas, unsigned short* polyFlags)
{
	for (int i = 0; i < params->polyCount; i++)
		polyFlags[i] = 1;
}

AI::Team AI::other(AI::Team t)
{
	return t == AI::Team::A ? AI::Team::B : AI::Team::A;
}

static const u16 RC_MESH_NULL_IDX = 0xffff;

void AI::init()
{
	nav_mesh_query = dtAllocNavMeshQuery();
	default_query_filter.setIncludeFlags((u16)-1);
	default_query_filter.setExcludeFlags(0);

	render_mesh = Loader::dynamic_mesh_permanent(1);
	Loader::dynamic_mesh_attrib(RenderDataType::Vec3);
	Loader::shader_permanent(Asset::Shader::flat);
}

void AI::load_nav_mesh(AssetID id)
{
	nav_mesh = Loader::nav_mesh(id);
	nav_tile_cache = Loader::nav_tile_cache;
	render_mesh_dirty = true;

	dtStatus status = nav_mesh_query->init(nav_mesh, 2048);
	vi_assert(dtStatusSucceed(status));
}

void AI::update(const Update& u)
{
	if (nav_tile_cache)
	{
		dtStatus status = nav_tile_cache->update(u.time.delta, nav_mesh);
		vi_assert(dtStatusSucceed(status));
	}
}

u32 AI::obstacle_add(const Vec3& pos, r32 radius, r32 height)
{
	render_mesh_dirty = true;
	vi_assert(nav_tile_cache);
	u32 id;
	dtStatus status = nav_tile_cache->addObstacle((r32*)&pos, radius, height, &id);
	return id;
}

void AI::obstacle_remove(u32 id)
{
	render_mesh_dirty = true;
	nav_tile_cache->removeObstacle(id);
}

void AI::debug_draw(const RenderParams& params)
{
#if DEBUG
	if (render_mesh_dirty)
	{
		Array<Vec3> vertices;
		Array<s32> indices;

		if (nav_mesh)
		{
			for (s32 tile_id = 0; tile_id < nav_mesh->getMaxTiles(); tile_id++)
			{
				const dtMeshTile* tile = ((const dtNavMesh*)nav_mesh)->getTile(tile_id);
				if (!tile->header)
					continue;

				for (s32 i = 0; i < tile->header->vertCount; i++)
				{
					memcpy(vertices.add(), &tile->verts[i * 3], sizeof(Vec3));
					indices.add(indices.length);
				}
			}
		}

		params.sync->write(RenderOp::UpdateAttribBuffers);
		params.sync->write(render_mesh);

		params.sync->write<s32>(vertices.length);
		params.sync->write<Vec3>(vertices.data, vertices.length);

		params.sync->write(RenderOp::UpdateIndexBuffer);
		params.sync->write(render_mesh);

		params.sync->write<s32>(indices.length);
		params.sync->write<s32>(indices.data, indices.length);

		render_mesh_dirty = false;
	}

	params.sync->write(RenderOp::Shader);
	params.sync->write(Asset::Shader::flat);
	params.sync->write(params.technique);

	params.sync->write(RenderOp::Uniform);
	params.sync->write(Asset::Uniform::diffuse_color);
	params.sync->write(RenderDataType::Vec4);
	params.sync->write<s32>(1);
	params.sync->write(Vec4(0, 1, 0, 0.5f));

	Mat4 mvp = params.view_projection;

	params.sync->write(RenderOp::Uniform);
	params.sync->write(Asset::Uniform::mvp);
	params.sync->write(RenderDataType::Mat4);
	params.sync->write<s32>(1);
	params.sync->write<Mat4>(mvp);

	params.sync->write(RenderOp::Mesh);
	params.sync->write(render_mesh);
#endif
}

b8 AI::vision_check(const Vec3& pos, const Vec3& enemy_pos, const Entity* a, const Entity* b)
{
	btCollisionWorld::ClosestRayResultCallback rayCallback(pos, enemy_pos);
	rayCallback.m_flags = btTriangleRaycastCallback::EFlags::kF_FilterBackfaces
		| btTriangleRaycastCallback::EFlags::kF_KeepUnflippedNormal;
	rayCallback.m_collisionFilterMask = rayCallback.m_collisionFilterGroup = ~CollisionTarget;

	Physics::btWorld->rayTest(pos, enemy_pos, rayCallback);

	if (!rayCallback.hasHit())
		return true;

	ID id = rayCallback.m_collisionObject->getUserIndex();
	return (a && id == a->id()) || (b && id == b->id());
}

Entity* AI::vision_query(const AIAgent* queryer, const Vec3& pos, const Vec3& forward, r32 radius, r32 angle, r32 max_height_diff, ComponentMask component_mask)
{
	r32 angle_dot = cosf(angle);
	for (auto i = AIAgent::list.iterator(); !i.is_last(); i.next())
	{
		AIAgent* agent = i.item();
		if (agent->team == queryer->team || !(agent->entity()->component_mask & component_mask))
			continue;

		Vec3 enemy_pos = agent->get<Transform>()->absolute_pos();

		Vec3 to_enemy = enemy_pos - pos;
		if (max_height_diff > 0.0f && to_enemy.y > max_height_diff)
			continue;

		r32 distance_to_enemy = to_enemy.length();

		if (distance_to_enemy < radius)
		{
			to_enemy /= distance_to_enemy;

			r32 dot = forward.dot(to_enemy);
			if (dot > angle_dot && vision_check(pos, enemy_pos, queryer->entity(), agent->entity()))
				return agent->entity();
		}
	}

	return nullptr;
}

Entity* AI::sound_query(AI::Team team, const Vec3& pos, ComponentMask component_mask)
{
	for (auto i = Shockwave::list.iterator(); !i.is_last(); i.next())
	{
		r32 radius = i.item()->radius();
		if ((i.item()->get<Transform>()->absolute_pos() - pos).length_squared() < radius * radius)
		{
			Entity* owner = i.item()->owner.ref();
			if (owner && (owner->component_mask & component_mask) && owner->has<AIAgent>())
			{
				AIAgent* agent = owner->get<AIAgent>();
				if (agent->team != team)
					return owner;
			}
		}
	}
	return nullptr;
}

dtPolyRef AI::get_poly(const Vec3& pos, const r32* search_extents)
{
	dtPolyRef result;

	AI::nav_mesh_query->findNearestPoly((r32*)&pos, search_extents, &default_query_filter, &result, 0);

	return result;
}

}
