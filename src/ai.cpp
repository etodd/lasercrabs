#include "ai.h"
#include "data/import_common.h"
#include "load.h"
#include "data/components.h"
#include "bullet/src/BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "game/entities.h"
#include "recast/Recast/Include/Recast.h"
#include "recast/Detour/Include/DetourNavMeshBuilder.h"
#include "recast/Detour/Include/DetourCommon.h"
#include "data/priority_queue.h"
#include "mersenne/mersenne-twister.h"

#include "asset/shader.h"

namespace VI
{

namespace AI
{

Team other(Team t)
{
	return t == Team::A ? Team::B : Team::A;
}

Array<b8> obstacles;
SyncRingBuffer<SYNC_IN_SIZE> sync_in;
SyncRingBuffer<SYNC_OUT_SIZE> sync_out;

void loop()
{
	Internal::loop();
}

void quit()
{
	sync_in.lock();
	sync_in.write(Op::Quit);
	sync_in.unlock();
}

void update(const Update& u)
{
	sync_out.lock();
	while (sync_out.can_read())
	{
		Callback cb;
		sync_out.read(&cb);
		switch (cb)
		{
			case Callback::Path:
			{
				LinkEntryArg<const Path&> link;
				sync_out.read(&link);
				Path path;
				sync_out.read(&path);
				(&link)->fire(path);
				break;
			}
			case Callback::AwkPath:
			{
				LinkEntryArg<const Path&> link;
				sync_out.read(&link);
				Path path;
				sync_out.read(&path);
				(&link)->fire(path);
				break;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}
	}
	sync_out.unlock();
}

b8 vision_check(const Vec3& pos, const Vec3& enemy_pos, const Entity* a, const Entity* b)
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

Entity* vision_query(const AIAgent* queryer, const Vec3& pos, const Vec3& forward, r32 radius, r32 angle, r32 max_height_diff, ComponentMask component_mask)
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

u32 obstacle_add(const Vec3& pos, r32 radius, r32 height)
{
	u32 id;
	b8 found = false;
	for (s32 i = 0; i < obstacles.length; i++)
	{
		if (!obstacles[i])
		{
			id = (u32)i;
			found = true;
			break;
		}
	}

	if (found)
		obstacles[id] = true;
	else
	{
		id = obstacles.length;
		obstacles.add(true);
	}

	sync_in.lock();
	sync_in.write(Op::ObstacleAdd);
	sync_in.write(id);
	sync_in.write(pos);
	sync_in.write(radius);
	sync_in.write(height);
	sync_in.unlock();

	return id;
}

void obstacle_remove(u32 id)
{
	obstacles[id] = false;
	sync_in.lock();
	sync_in.write(Op::ObstacleRemove);
	sync_in.write(id);
	sync_in.unlock();
}

void load(const u8* data, s32 length)
{
	sync_in.lock();
	sync_in.write(Op::Load);
	sync_in.write(data, length);
	sync_in.unlock();
}

void random_path(const Vec3& pos, const LinkEntryArg<const Path&>& callback)
{
	sync_in.lock();
	sync_in.write(Op::RandomPath);
	sync_in.write(pos);
	sync_in.write(callback);
	sync_in.unlock();
}

void awk_random_path(const Vec3& pos, const LinkEntryArg<const Path&>& callback)
{
	sync_in.lock();
	sync_in.write(Op::AwkRandomPath);
	sync_in.write(pos);
	sync_in.write(callback);
	sync_in.unlock();
}

void pathfind(const Vec3& a, const Vec3& b, const LinkEntryArg<const Path&>& callback)
{
	sync_in.lock();
	sync_in.write(Op::Pathfind);
	sync_in.write(a);
	sync_in.write(b);
	sync_in.write(callback);
	sync_in.unlock();
}

void awk_pathfind(const Vec3& a, const Vec3& b, const LinkEntryArg<const Path&>& callback)
{
	sync_in.lock();
	sync_in.write(Op::AwkPathfind);
	sync_in.write(a);
	sync_in.write(b);
	sync_in.write(callback);
	sync_in.unlock();
}

// todo: re-enable AI debug visualizations
/*
#if DEBUG
void refresh_nav_render_meshes(const RenderParams& params)
{
	Array<Vec3> vertices;
	Array<s32> indices;

	// nav mesh
	{
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

		params.sync_in->write(RenderOp::UpdateAttribBuffers);
		params.sync_in->write(render_mesh);

		params.sync_in->write<s32>(vertices.length);
		params.sync_in->write<Vec3>(vertices.data, vertices.length);

		params.sync_in->write(RenderOp::UpdateIndexBuffer);
		params.sync_in->write(render_mesh);

		params.sync_in->write<s32>(indices.length);
		params.sync_in->write<s32>(indices.data, indices.length);
	}

	vertices.length = 0;
	indices.length = 0;

	// awk nav mesh
	{
		s32 vertex_count = 0;
		for (s32 chunk_index = 0; chunk_index < awk_nav_mesh.chunks.length; chunk_index++)
		{
			const AwkNavMeshChunk& chunk = awk_nav_mesh.chunks[chunk_index];

			for (s32 i = 0; i < chunk.vertices.length; i++)
				vertices.add(chunk.vertices[i]);

			for (s32 i = 0; i < chunk.adjacency.length; i++)
			{
				const AwkNavMeshAdjacency& adjacency = chunk.adjacency[i];
				for (s32 j = 0; j < adjacency.length; j++)
				{
					const AwkNavMeshNode& neighbor = adjacency[j];
					indices.add(vertex_count + i);

					s32 neighbor_chunk_vertex_index = 0;
					for (s32 k = 0; k < neighbor.chunk; k++)
						neighbor_chunk_vertex_index += awk_nav_mesh.chunks[k].vertices.length;
					indices.add(neighbor_chunk_vertex_index + neighbor.vertex);
				}
			}

			vertex_count += chunk.vertices.length;
		}

		params.sync_in->write(RenderOp::UpdateAttribBuffers);
		params.sync_in->write(awk_render_mesh);

		params.sync_in->write<s32>(vertices.length);
		params.sync_in->write<Vec3>(vertices.data, vertices.length);

		params.sync_in->write(RenderOp::UpdateIndexBuffer);
		params.sync_in->write(awk_render_mesh);

		params.sync_in->write<s32>(indices.length);
		params.sync_in->write<s32>(indices.data, indices.length);
	}
}

void render_helper(const RenderParams& params, AssetID m, RenderPrimitiveMode primitive_mode)
{
	params.sync_in->write(RenderOp::Shader);
	params.sync_in->write(Asset::Shader::flat);
	params.sync_in->write(params.technique);

	params.sync_in->write(RenderOp::Uniform);
	params.sync_in->write(Asset::Uniform::diffuse_color);
	params.sync_in->write(RenderDataType::Vec4);
	params.sync_in->write<s32>(1);
	params.sync_in->write(Vec4(0, 1, 0, 0.5f));

	Mat4 mvp = params.view_projection;

	params.sync_in->write(RenderOp::Uniform);
	params.sync_in->write(Asset::Uniform::mvp);
	params.sync_in->write(RenderDataType::Mat4);
	params.sync_in->write<s32>(1);
	params.sync_in->write<Mat4>(mvp);

	params.sync_in->write(RenderOp::FillMode);
	params.sync_in->write(RenderFillMode::Point);
	params.sync_in->write(RenderOp::PointSize);
	params.sync_in->write<r32>(4 * UI::scale);
	params.sync_in->write(RenderOp::CullMode);
	params.sync_in->write(RenderCullMode::None);

	params.sync_in->write(RenderOp::Mesh);
	params.sync_in->write(primitive_mode);
	params.sync_in->write(m);

	params.sync_in->write(RenderOp::FillMode);
	params.sync_in->write(RenderFillMode::Fill);
	params.sync_in->write(RenderOp::CullMode);
	params.sync_in->write(RenderCullMode::Back);
}
#endif
*/

}


}