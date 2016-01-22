#include "awk.h"
#include "data/components.h"
#include "BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "entities.h"
#include "render/skinned_model.h"
#include "audio.h"
#include "asset/Wwise_IDs.h"
#include "player.h"
#include "mersenne/mersenne-twister.h"
#include "common.h"
#include "asset/mesh.h"
#include "asset/armature.h"
#include "asset/animation.h"
#include "data/animator.h"
#include "render/views.h"
#include "asset/font.h"
#include "game.h"
#include "console.h"
#include "sentinel.h"

namespace VI
{

#define LERP_ROTATION_SPEED 10.0f
#define LERP_TRANSLATION_SPEED 4.0f
#define MAX_FLIGHT_TIME 2.0f
#define AWK_LEG_LENGTH (0.277f - 0.101f)
#define AWK_LEG_BLEND_SPEED (1.0f / 0.02f)
#define AWK_MIN_LEG_BLEND_SPEED (AWK_LEG_BLEND_SPEED * 0.05f)

Vec4 SecondaryFireColors::all[(s32)SecondaryFire::Count] =
{
	Vec4(0, 0, 0, 1),
	Vec4(0, 1, 0, 1),
	Vec4(1, 1, 0, 1),
	Vec4(1, 0, 0, 1),
};

AwkRaycastCallback::AwkRaycastCallback(const Vec3& a, const Vec3& b, const Entity* awk)
	: btCollisionWorld::ClosestRayResultCallback(a, b)
{
	hit_target = false;
	entity_id = awk->id();
}

btScalar AwkRaycastCallback::addSingleResult(btCollisionWorld::LocalRayResult& rayResult, b8 normalInWorldSpace)
{
	short filter_group = rayResult.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup;
	if (filter_group & CollisionWalker)
	{
		Entity* entity = &World::entities[rayResult.m_collisionObject->getUserIndex()];
		if (entity->has<SentinelCommon>() && entity->get<SentinelCommon>()->headshot_test(m_rayFromWorld, m_rayToWorld))
		{
			hit_target = true;
			return m_closestHitFraction;
		}
	}
	else if (filter_group & CollisionTarget)
	{
		if (rayResult.m_collisionObject->getUserIndex() != entity_id)
			hit_target = true;
		return m_closestHitFraction;
	}

	m_closestHitFraction = rayResult.m_hitFraction;
	m_collisionObject = rayResult.m_collisionObject;
	if (normalInWorldSpace)
	{
		m_hitNormalWorld = rayResult.m_hitNormalLocal;
	}
	else
	{
		///need to transform normal into worldspace
		m_hitNormalWorld = m_collisionObject->getWorldTransform().getBasis()*rayResult.m_hitNormalLocal;
	}
	m_hitPointWorld.setInterpolate3(m_rayFromWorld,m_rayToWorld,rayResult.m_hitFraction);
	return rayResult.m_hitFraction;
}

Awk::Awk()
	: velocity(0.0f, -AWK_FLY_SPEED, 0.0f),
	attached(),
	attach_time(),
	rope(),
	footing(),
	last_speed(),
	last_footstep()
{
}

void Awk::awake()
{
	link_arg<Entity*, &Awk::killed>(get<Health>()->killed);
	link_arg<Entity*, &Awk::hit_by>(get<Target>()->hit_by);
}

Vec3 Awk::center()
{
	return get<Transform>()->to_world((get<SkinnedModel>()->offset * Vec4(0, 0, 0.05f, 1)).xyz());
}

void Awk::hit_by(Entity* hit_by)
{
	get<Health>()->damage(hit_by, 100); // Killed
}

void Awk::hit_target(Entity* target)
{
	if (target->has<Target>())
	{
		Target* t = target->get<Target>();

		if (target->has<RigidBody>())
		{
			RigidBody* body = target->get<RigidBody>();
			body->btBody->applyImpulse(velocity * 0.05f, Vec3::zero);
			body->btBody->setActivationState(ACTIVE_TAG);
		}

		hit.fire(target);
		t->hit(entity());
	}
}

void Awk::killed(Entity* e)
{
	get<Audio>()->post_event(has<LocalPlayerControl>() ? AK::EVENTS::PLAY_HURT_PLAYER : AK::EVENTS::PLAY_HURT);
	get<Audio>()->post_event(AK::EVENTS::STOP_FLY);
	World::remove_deferred(entity());
}

b8 Awk::can_go(const Vec3& dir, Vec3* final_pos)
{
	if (get<Transform>()->parent.ref())
	{
		Vec3 wall_normal = get<Transform>()->absolute_rot() * Vec3(0, 0, 1);

		Vec3 trace_dir = Vec3::normalize(dir);

		if (trace_dir.dot(wall_normal) < 0.0f)
			return false;

		Vec3 trace_start = center();
		s32 bounce = 0;
		while (true)
		{
			Vec3 trace_end = trace_start + trace_dir * AWK_MAX_DISTANCE;

			AwkRaycastCallback ray_callback(trace_start, trace_end, entity());
			ray_callback.m_flags = btTriangleRaycastCallback::EFlags::kF_FilterBackfaces
				| btTriangleRaycastCallback::EFlags::kF_KeepUnflippedNormal;
			ray_callback.m_collisionFilterMask = ray_callback.m_collisionFilterGroup = btBroadphaseProxy::AllFilter;

			Physics::btWorld->rayTest(trace_start, trace_end, ray_callback);

			if (ray_callback.hasHit())
			{
				short group = ray_callback.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup;
				if (group & (CollisionWalker | CollisionInaccessible))
					return false;
				else
				{
					if (final_pos)
						*final_pos = ray_callback.m_hitPointWorld;
					return true;
				}
			}
			else
				return false;
		}
	}
	return false;
}

b8 Awk::detach(const Update& u, const Vec3& dir)
{
	if (get<PlayerCommon>()->cooldown == 0.0f)
	{
		get<Audio>()->post_event(has<LocalPlayerControl>() ? AK::EVENTS::PLAY_LAUNCH_PLAYER : AK::EVENTS::PLAY_LAUNCH);

		Transform* parent = get<Transform>()->parent.ref();
		if (parent && parent->has<Socket>())
		{
			Entity* rope_entity = World::create<RopeEntity>(get<Transform>()->to_world(Vec3(0, 0, -AWK_RADIUS)), get<Transform>()->to_world_normal(Vec3(0, 0, 1)), get<Transform>()->parent.ref()->get<RigidBody>(), 1.0f);
			rope = rope_entity->get<Rope>();
		}

		attach_time = u.time.total;
		get<PlayerCommon>()->cooldown = AWK_MIN_COOLDOWN;
		Vec3 dir_normalized = Vec3::normalize(dir);
		velocity = dir_normalized * AWK_FLY_SPEED;
		get<Transform>()->reparent(nullptr);
		get<Transform>()->pos = center() + dir_normalized * AWK_RADIUS * 0.5f;
		get<Transform>()->rot = Quat::look(dir_normalized);
		get<SkinnedModel>()->offset = Mat4::identity;

		for (s32 i = 0; i < AWK_LEGS; i++)
			footing[i].parent = nullptr;
		get<Animator>()->reset_overrides();
		get<Animator>()->layers[0].animation = Asset::Animation::awk_fly;

		return true;
	}
	return false;
}

const AssetID awk_legs[AWK_LEGS] =
{
	Asset::Bone::awk_a1,
	Asset::Bone::awk_b1,
	Asset::Bone::awk_c1,
};

const AssetID awk_outer_legs[AWK_LEGS] =
{
	Asset::Bone::awk_a2,
	Asset::Bone::awk_b2,
	Asset::Bone::awk_c2,
};

const r32 awk_outer_leg_rotation[AWK_LEGS] =
{
	-1,
	1,
	1,
};

void Awk::set_footing(const s32 index, const Transform* parent, const Vec3& pos)
{
	if (footing[index].parent.ref())
	{
		footing[index].blend = 0.0f;
		footing[index].last_abs_pos = footing[index].parent.ref()->to_world(footing[index].pos);
	}
	else
		footing[index].blend = 1.0f;
	footing[index].parent = parent;
	footing[index].pos = footing[index].parent.ref()->to_local(pos);
}

void Awk::update(const Update& u)
{
	if (get<Transform>()->parent.ref())
	{
		Quat rot = get<Transform>()->rot;

		Mat4 inverse_offset = get<SkinnedModel>()->offset.inverse();

		r32 leg_blend_speed = fmax(AWK_MIN_LEG_BLEND_SPEED, AWK_LEG_BLEND_SPEED * (last_speed / AWK_CRAWL_SPEED));
		last_speed = 0.0f;

		Armature* arm = Loader::armature(get<Animator>()->armature);
		for (s32 i = 0; i < AWK_LEGS; i++)
		{
			b8 find_footing = false;

			Vec3 relative_target;

			Vec3 find_footing_offset;

			if (footing[i].parent.ref())
			{
				Vec3 target = footing[i].parent.ref()->to_world(footing[i].pos);
				Vec3 relative_target = get<Transform>()->to_local(target);
				Vec3 target_leg_space = (arm->inverse_bind_pose[awk_legs[i]] * Vec4(relative_target, 1.0f)).xyz();
				// x axis = lengthwise along the leg
				// y axis = left and right rotation of the leg
				// z axis = up and down rotation of the leg
				if (target_leg_space.x < AWK_LEG_LENGTH * 0.25f && target_leg_space.z > AWK_LEG_LENGTH * -1.25f)
				{
					find_footing_offset = Vec3(AWK_LEG_LENGTH * 2.0f, -target_leg_space.y, 0);
					find_footing = true;
				}
				else if (target_leg_space.x > AWK_LEG_LENGTH * 1.75f)
				{
					find_footing_offset = Vec3(AWK_LEG_LENGTH * 0.5f, -target_leg_space.y, 0);
					find_footing = true;
				}
				else if (target_leg_space.y < AWK_LEG_LENGTH * -1.5f || target_leg_space.y > AWK_LEG_LENGTH * 1.5f
					|| target_leg_space.length_squared() > (AWK_LEG_LENGTH * 2.0f) * (AWK_LEG_LENGTH * 2.0f))
				{
					find_footing_offset = Vec3(fmax(target_leg_space.x, AWK_LEG_LENGTH * 0.25f), -target_leg_space.y, 0);
					find_footing = true;
				}
			}
			else
			{
				find_footing = true;
				find_footing_offset = Vec3(AWK_LEG_LENGTH, 0, 0);
			}

			if (find_footing)
			{
				Mat4 bind_pose_mat = arm->abs_bind_pose[awk_legs[i]];
				Vec3 ray_start = get<Transform>()->to_world((bind_pose_mat * Vec4(0, 0, AWK_LEG_LENGTH * 1.75f, 1)).xyz());
				Vec3 ray_end = get<Transform>()->to_world((bind_pose_mat * Vec4(find_footing_offset + Vec3(0, 0, AWK_LEG_LENGTH * -1.0f), 1)).xyz());
				btCollisionWorld::ClosestRayResultCallback rayCallback(ray_start, ray_end);
				rayCallback.m_flags = btTriangleRaycastCallback::EFlags::kF_FilterBackfaces
					| btTriangleRaycastCallback::EFlags::kF_KeepUnflippedNormal;
				rayCallback.m_collisionFilterMask = rayCallback.m_collisionFilterGroup = ~CollisionWalker & ~CollisionTarget & ~CollisionInaccessible;

				Physics::btWorld->rayTest(ray_start, ray_end, rayCallback);
				if (rayCallback.hasHit())
					set_footing(i, World::entities[rayCallback.m_collisionObject->getUserIndex()].get<Transform>(), rayCallback.m_hitPointWorld);
				else
				{
					Vec3 new_ray_start = get<Transform>()->to_world((bind_pose_mat * Vec4(AWK_LEG_LENGTH * 1.5f, 0, 0, 1)).xyz());
					Vec3 new_ray_end = get<Transform>()->to_world((bind_pose_mat * Vec4(AWK_LEG_LENGTH * -1.0f, find_footing_offset.y, AWK_LEG_LENGTH * -1.0f, 1)).xyz());
					btCollisionWorld::ClosestRayResultCallback rayCallback(new_ray_start, new_ray_end);
					rayCallback.m_flags = btTriangleRaycastCallback::EFlags::kF_FilterBackfaces
						| btTriangleRaycastCallback::EFlags::kF_KeepUnflippedNormal;
					rayCallback.m_collisionFilterMask = rayCallback.m_collisionFilterGroup = ~CollisionWalker & ~CollisionTarget & ~CollisionInaccessible;
					Physics::btWorld->rayTest(new_ray_start, new_ray_end, rayCallback);
					if (rayCallback.hasHit())
						set_footing(i, World::entities[rayCallback.m_collisionObject->getUserIndex()].get<Transform>(), rayCallback.m_hitPointWorld);
					else
						footing[i].parent = nullptr;
				}
			}

			if (footing[i].parent.ref())
			{
				Vec3 target = footing[i].parent.ref()->to_world(footing[i].pos);
				Vec3 relative_target = (inverse_offset * Vec4(get<Transform>()->to_local(target), 1)).xyz();
				Vec3 target_leg_space = (arm->inverse_bind_pose[awk_legs[i]] * Vec4(relative_target, 1.0f)).xyz();

				if (footing[i].blend < 1.0f)
				{
					Vec3 last_relative_target = (inverse_offset * Vec4(get<Transform>()->to_local(footing[i].last_abs_pos), 1)).xyz();
					Vec3 last_target_leg_space = (arm->inverse_bind_pose[awk_legs[i]] * Vec4(last_relative_target, 1.0f)).xyz();

					footing[i].blend = fmin(1.0f, footing[i].blend + u.time.delta * leg_blend_speed);
					target_leg_space = Vec3::lerp(footing[i].blend, last_target_leg_space, target_leg_space);
					if (footing[i].blend == 1.0f && Game::real_time.total - last_footstep > 0.07f)
					{
						get<Audio>()->post_event(has<LocalPlayerControl>() ? AK::EVENTS::PLAY_AWK_FOOTSTEP_PLAYER : AK::EVENTS::PLAY_AWK_FOOTSTEP);
						last_footstep = Game::real_time.total;
					}
				}

				r32 angle = atan2f(-target_leg_space.y, target_leg_space.x);

				r32 angle_x = acosf((target_leg_space.length() * 0.5f) / AWK_LEG_LENGTH);

				if (target_leg_space.x < 0.0f)
					angle += PI;

				Vec2 xy_offset = Vec2(target_leg_space.x, target_leg_space.y);
				r32 angle_x_offset = -atan2f(target_leg_space.z, xy_offset.length() * (target_leg_space.x < 0.0f ? -1.0f : 1.0f));

				get<Animator>()->override_bone(awk_legs[i], Vec3::zero, Quat::euler(-angle, 0, 0) * Quat::euler(0, angle_x_offset - angle_x, 0));
				get<Animator>()->override_bone(awk_outer_legs[i], Vec3::zero, Quat::euler(0, angle_x * 2.0f * awk_outer_leg_rotation[i], 0));
			}
			else
			{
				get<Animator>()->override_bone(awk_legs[i], Vec3::zero, Quat::euler(0, PI * -0.1f, 0));
				get<Animator>()->override_bone(awk_outer_legs[i], Vec3::zero, Quat::euler(0, PI * 0.75f * awk_outer_leg_rotation[i], 0));
			}
		}
	}
	else
	{
		if (attach_time > 0.0f && u.time.total - attach_time > MAX_FLIGHT_TIME)
			get<Health>()->damage(entity(), 100); // Kill self
		else
		{
			Vec3 position = get<Transform>()->absolute_pos();
			Vec3 next_position = position + velocity * u.time.delta;
			get<Transform>()->absolute_pos(next_position);

			if (!btVector3(velocity).fuzzyZero())
			{
				Vec3 dir = Vec3::normalize(velocity);
				Vec3 ray_start = position - dir * AWK_RADIUS;
				Vec3 ray_end = next_position + dir * AWK_RADIUS;
				btCollisionWorld::AllHitsRayResultCallback rayCallback(ray_start, ray_end);
				rayCallback.m_flags = btTriangleRaycastCallback::EFlags::kF_FilterBackfaces
					| btTriangleRaycastCallback::EFlags::kF_KeepUnflippedNormal;
				rayCallback.m_collisionFilterMask = rayCallback.m_collisionFilterGroup = btBroadphaseProxy::AllFilter;

				Physics::btWorld->rayTest(ray_start, ray_end, rayCallback);

				r32 fraction_end = 2.0f;
				s32 index_end = -1;

				for (s32 i = 0; i < rayCallback.m_collisionObjects.size(); i++)
				{
					if (rayCallback.m_hitFractions[i] < fraction_end)
					{
						short group = rayCallback.m_collisionObjects[i]->getBroadphaseHandle()->m_collisionFilterGroup;
						b8 end = false;
						if (group & CollisionInaccessible)
							end = true;
						else if (group & CollisionWalker)
						{
							Entity* t = &World::entities[rayCallback.m_collisionObjects[i]->getUserIndex()];
							if (t->has<SentinelCommon>() && !t->get<SentinelCommon>()->headshot_test(ray_start, ray_end))
								end = true;
						}
						else if (!(group & CollisionTarget))
							end = true;

						if (end)
						{
							fraction_end = rayCallback.m_hitFractions[i];
							index_end = i;
						}
					}
				}

				for (s32 i = 0; i < rayCallback.m_collisionObjects.size(); i++)
				{
					if (i == index_end || rayCallback.m_hitFractions[i] < fraction_end)
					{
						short group = rayCallback.m_collisionObjects[i]->getBroadphaseHandle()->m_collisionFilterGroup;
						if (group & CollisionWalker)
						{
							Entity* t = &World::entities[rayCallback.m_collisionObjects[i]->getUserIndex()];
							if (t->has<SentinelCommon>() && t->get<SentinelCommon>()->headshot_test(ray_start, ray_end))
							{
								hit.fire(t);
								t->get<Health>()->damage(entity(), 100);
							}
						}
						else if (group & CollisionInaccessible)
						{
							get<Health>()->damage(entity(), 100); // Kill self
							return;
						}
						else if (group & CollisionTarget)
						{
							Entity* t = &World::entities[rayCallback.m_collisionObjects[i]->getUserIndex()];
							if (t != entity())
								hit_target(t);
						}
						else
						{
							Entity* entity = &World::entities[rayCallback.m_collisionObjects[i]->getUserIndex()];
							get<Transform>()->parent = entity->get<Transform>();
							next_position = rayCallback.m_hitPointWorld[i] + rayCallback.m_hitNormalWorld[i] * AWK_RADIUS;
							get<Transform>()->absolute(next_position, Quat::look(rayCallback.m_hitNormalWorld[i]));

							ShockwaveEntity* shockwave = World::create<ShockwaveEntity>(Awk::entity(), AWK_SHOCKWAVE_RADIUS);
							shockwave->get<Transform>()->pos = get<Transform>()->pos;
							shockwave->get<Transform>()->parent = get<Transform>()->parent;

							if (rope.ref())
							{
								RigidBody* rope_end = get<Transform>()->parent.ref()->get<RigidBody>();
								if (rope_end->has<Socket>())
									rope.ref()->end(get<Transform>()->to_world(Vec3(0, 0, -AWK_RADIUS)), get<Transform>()->to_world_normal(Vec3(0, 0, 1)), rope_end);
								rope = nullptr;
							}

							get<Animator>()->layers[0].animation = AssetNull;

							attached.fire();
							get<Audio>()->post_event(has<LocalPlayerControl>() ? AK::EVENTS::PLAY_LAND_PLAYER : AK::EVENTS::PLAY_LAND);
							attach_time = u.time.total;

							velocity = Vec3::zero;

							const r32 damage_radius = 2.0f;
							const r32 damage_radius_squared = damage_radius * damage_radius;
							Vec3 pos = get<Transform>()->absolute_pos();
							AI::Team team = get<AIAgent>()->team;
							for (auto i = Awk::list().iterator(); !i.is_last(); i.next())
							{
								if (i.item()->get<AIAgent>()->team != team && (i.item()->center() - pos).length_squared() < damage_radius_squared)
									hit_target(i.item()->entity());
							}
						}
					}
				}

				if (rope.ref())
				{
					Vec3 pos;
					Quat rot;
					get<Transform>()->absolute(&pos, &rot);
					rope.ref()->add(pos, rot);
				}
			}
			get<PlayerCommon>()->cooldown = fmin(get<PlayerCommon>()->cooldown + (next_position - position).length() * AWK_COOLDOWN_DISTANCE_RATIO, AWK_MAX_DISTANCE_COOLDOWN);
		}
	}
}

}