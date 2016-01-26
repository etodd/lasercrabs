#include "minion.h"
#include "data/animator.h"
#include "render/skinned_model.h"
#include "walker.h"
#include "asset/armature.h"
#include "asset/animation.h"
#include "asset/shader.h"
#include "asset/texture.h"
#include "asset/mesh.h"
#include "asset/font.h"
#include "recast/Detour/Include/DetourNavMeshQuery.h"
#include "player.h"
#include "mersenne/mersenne-twister.h"
#include "common.h"
#include "game.h"
#include "audio.h"
#include "asset/Wwise_IDs.h"
#include "render/views.h"
#include "awk.h"
#include "BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "menu.h"
#include "data/ragdoll.h"
#include "usernames.h"
#include "entities.h"
#include "lmath.h"
#include "console.h"

#define HEAD_RADIUS 0.25f
#define VIEW_RANGE 30.0f
#define VIEW_FOV PI * 0.4f
#define VIEW_ATTENTION_FOV PI * 0.3f
#define VIEW_MAX_HEIGHT 6.0f

#define RAYCAST_RADIUS_RATIO 1.3f
#define WALL_RUN_DISTANCE_RATIO 1.1f

#define RUN_SPEED 5.0f
#define WALK_SPEED 2.0f
#define MAX_SPEED 7.0f
#define MIN_WALLRUN_SPEED 1.0f

#define JUMP_GRACE_PERIOD 0.3f

namespace VI
{

Minion::Minion(const Vec3& pos, const Quat& quat, AI::Team team)
{
	Transform* transform = create<Transform>();
	transform->pos = pos;

	Animator* animator = create<Animator>();
	SkinnedModel* model = create<SkinnedModel>();

	animator->armature = Asset::Armature::character_mesh;
	animator->layers[0].animation = Asset::Animation::character_idle;

	model->shader = Asset::Shader::armature;
	model->mesh = Asset::Mesh::character_mesh;
	model->color = AI::colors[(s32)team];

	create<Audio>();

	Health* health = create<Health>(100);
	
	Vec3 forward = quat * Vec3(0, 0, 1);

	Walker* walker = create<Walker>(atan2f(forward.x, forward.z));
	walker->max_speed = MAX_SPEED;

	create<MinionCommon>();

	create<AIAgent>()->team = team;
}

void MinionCommon::awake()
{
	link_arg<Entity*, &MinionCommon::killed>(get<Health>()->killed);

	Animator* animator = get<Animator>();
	animator->layers[1].loop = false;
	link<&MinionCommon::footstep>(animator->trigger(Asset::Animation::character_walk, 0.45f));
	link<&MinionCommon::footstep>(animator->trigger(Asset::Animation::character_walk, 1.0f));
	link<&MinionCommon::footstep>(animator->trigger(Asset::Animation::character_run, 0.216f));
	link<&MinionCommon::footstep>(animator->trigger(Asset::Animation::character_run, 0.476f));
}

void MinionCommon::set_run(b8 r)
{
	if (r && fsm.current == State::Normal)
		fsm.transition(State::Run);
	else if (!r && (fsm.current == State::Run || fsm.current == State::WallRun))
		fsm.transition(State::Normal);
	get<Walker>()->speed = r ? RUN_SPEED : WALK_SPEED;
}

void MinionCommon::footstep()
{
	Vec3 base_pos = get<Walker>()->base_pos();

	Audio::post_global_event(AK::EVENTS::PLAY_FOOTSTEP, base_pos);

	ShockwaveEntity* shockwave = World::create<ShockwaveEntity>(entity(), fsm.current == State::Run ? 10.0f : 3.0f);
	shockwave->get<Transform>()->pos = base_pos;
	shockwave->get<Transform>()->reparent(get<Transform>()->parent.ref());
}

Vec3 MinionCommon::head_pos()
{
	Vec3 pos = Vec3(0.1f, 0, 0);
	Quat rot = Quat::identity;
	get<Animator>()->to_world(Asset::Bone::character_head, &pos, &rot);
	return pos;
}

void MinionCommon::head_to_object_space(Vec3* pos, Quat* rot)
{
	Vec3 offset_pos = Vec3(0.05f, 0, 0.13f) + *pos;
	Quat offset_quat = Quat::euler(0, 0, PI * 0.5f) * *rot;
	get<Animator>()->bone_transform(Asset::Bone::character_head, &offset_pos, &offset_quat);
	*pos = (get<SkinnedModel>()->offset * Vec4(offset_pos)).xyz();
	*rot = (Quat::euler(0, get<Walker>()->rotation, 0) * offset_quat);
}

b8 MinionCommon::headshot_test(const Vec3& ray_start, const Vec3& ray_end)
{
	Vec3 head = head_pos();

	Vec3 ray = ray_end - ray_start;
	Vec3 head_to_ray_start = ray_start - head;

	r32 a = ray.length_squared();
	r32 b = 2.0f * ray.dot(head_to_ray_start);
	r32 c = head_to_ray_start.length_squared() - (HEAD_RADIUS * HEAD_RADIUS);

	r32 delta = (b * b) - 4.0f * a * c;

	return delta >= 0.0f;
}

void MinionCommon::update(const Update& u)
{
	fsm.time += u.time.delta;
	get<SkinnedModel>()->offset.make_transform(
		Vec3(0, -1.1f, 0),
		Vec3(1.0f, 1.0f, 1.0f),
		Quat::euler(0, get<Walker>()->rotation + PI * 0.5f, 0)
	);

	Animator::Layer* layer = &get<Animator>()->layers[0];

	if (fsm.current == State::Mantle)
	{
		const r32 mantle_time = 0.5f;
		if (fsm.time > mantle_time || !last_support.ref())
		{
			fsm.transition(State::Normal);
			get<RigidBody>()->btBody->setLinearVelocity(Quat::euler(0, get<Walker>()->rotation, 0) * Vec3(0, 0, get<Walker>()->last_supported_speed));
		}
		else
		{
			Vec3 end = last_support.ref()->get<Transform>()->to_world(relative_support_pos);
			Vec3 start = get<Transform>()->absolute_pos();
			Vec3 diff = end - start;
			r32 blend = fsm.time / mantle_time;
			Vec3 adjustment;
			if (blend < 0.5f)
			{
				// Move vertically
				r32 distance = diff.y;
				r32 time_left = (mantle_time * 0.5f) - fsm.time;
				adjustment = Vec3(0, fmin(distance, u.time.delta / time_left), 0);
			}
			else
			{
				// Move horizontally
				r32 distance = diff.length();
				r32 time_left = mantle_time - fsm.time;
				adjustment = diff * fmin(1.0f, u.time.delta / time_left);
			}
			get<RigidBody>()->btBody->setWorldTransform(btTransform(Quat::identity, start + adjustment));
		}
		last_support_time = Game::time.total;
	}
	else if (fsm.current == State::WallRun)
	{
		b8 exit_wallrun = false;
		if (get<Walker>()->support.ref())
			exit_wallrun = true;
		else
		{
			Vec3 ray_start = get<Walker>()->base_pos() + Vec3(0, get<Walker>()->support_height, 0);

			const Vec3 wall_run_normal = last_support.ref()->get<Transform>()->to_world_normal(relative_wall_run_normal);

			Vec3 ray_end = ray_start + wall_run_normal * get<Walker>()->radius * WALL_RUN_DISTANCE_RATIO * -2.0f;
			btCollisionWorld::ClosestRayResultCallback ray_callback(ray_start, ray_end);
			ray_callback.m_flags = btTriangleRaycastCallback::EFlags::kF_FilterBackfaces
				| btTriangleRaycastCallback::EFlags::kF_KeepUnflippedNormal;
			ray_callback.m_collisionFilterMask = ray_callback.m_collisionFilterGroup = ~CollisionWalker & ~CollisionTarget;

			Physics::btWorld->rayTest(ray_start, ray_end, ray_callback);

			if (ray_callback.hasHit())
			{
				Vec3 wall_normal = ray_callback.m_hitNormalWorld;
				if (wall_normal.dot(wall_run_normal) > 0.6f)
				{
					// Still on the wall
					btRigidBody* body = get<RigidBody>()->btBody;
					
					Vec3 velocity = body->getLinearVelocity() + Vec3(Physics::btWorld->getGravity()) * -0.3f * u.time.delta; // cancel gravity a bit
					velocity -= wall_normal * wall_normal.dot(velocity); // keep us on the wall

					Vec3 support_velocity = Vec3::zero;
					const btRigidBody* object = dynamic_cast<const btRigidBody*>(ray_callback.m_collisionObject);
					if (object)
					{
						support_velocity = Vec3(object->getLinearVelocity())
							+ Vec3(object->getAngularVelocity()).cross(ray_callback.m_hitPointWorld - Vec3(object->getCenterOfMassPosition()));
					}

					Vec3 velocity_diff = velocity - support_velocity;
					r32 vertical_velocity_diff = velocity_diff.y;
					velocity_diff.y = 0.0f;
					if (wall_run_state != WallRunState::Forward && velocity_diff.length() < MIN_WALLRUN_SPEED)
						exit_wallrun = true; // We're going too slow
					else
					{
						body->setLinearVelocity(velocity);

						Plane p(wall_normal, ray_callback.m_hitPointWorld);

						Vec3 pos = get<Transform>()->absolute_pos();
						Vec3 new_pos = pos + wall_normal * (-p.distance(pos) + (get<Walker>()->radius * WALL_RUN_DISTANCE_RATIO));

						btTransform transform = body->getWorldTransform();
						transform.setOrigin(new_pos);
						body->setWorldTransform(transform);

						last_support = Entity::list[ray_callback.m_collisionObject->getUserIndex()].get<RigidBody>();
						relative_wall_run_normal = last_support.ref()->get<Transform>()->to_local_normal(wall_normal);

						// Face the correct direction
						{
							Vec3 forward;
							if (wall_run_state == WallRunState::Forward)
							{
								forward = -wall_normal;
								forward.normalize();
							}
							else
							{
								forward = velocity_diff;
								forward.normalize();
							}
							get<Walker>()->target_rotation = atan2(forward.x, forward.z);
						}

						// Update animation speed
						if (wall_run_state == WallRunState::Forward)
							layer->speed = fmax(0.0f, vertical_velocity_diff / get<Walker>()->speed);
						else
							layer->speed = velocity_diff.length() / get<Walker>()->speed;

						// Try to climb stuff while we're wall-running
						if (try_parkour())
							exit_wallrun = true;
						else
						{
							relative_support_pos = last_support.ref()->get<Transform>()->to_local(get<Walker>()->base_pos());
							last_support_time = Game::time.total;
						}
					}
				}
				else // The wall was changed direction too drastically
					exit_wallrun = true;
			}
			else // ran out of wall to run on
			{
				exit_wallrun = true;
				if (wall_run_state == WallRunState::Forward)
					try_parkour(true); // do an extra broad raycast to make sure we hit the top if at all possible
			}
		}

		if (exit_wallrun && fsm.current == State::WallRun)
			fsm.transition(State::Normal);
	}
	else if (fsm.current == State::Normal || fsm.current == State::Run)
	{
		if (get<Walker>()->support.ref())
		{
			last_support_time = Game::time.total;
			last_support = get<Walker>()->support;
			relative_support_pos = last_support.ref()->get<Transform>()->to_local(get<Walker>()->base_pos());
		}
	}

	// update animation
	AssetID anim;
	if (fsm.current == State::WallRun)
		anim = Asset::Animation::character_run; // speed already set
	else if (get<Walker>()->support.ref() && get<Walker>()->dir.length_squared() > 0.0f)
	{
		r32 net_speed = fmax(get<Walker>()->net_speed, WALK_SPEED * 0.5f);
		if (fsm.current == State::Run)
		{
			anim = net_speed > WALK_SPEED ? Asset::Animation::character_run : Asset::Animation::character_walk;
			layer->speed = net_speed > WALK_SPEED ? LMath::lerpf((net_speed - WALK_SPEED) / RUN_SPEED, 0.75f, 1.0f) : (net_speed / WALK_SPEED);
		}
		else
		{
			anim = Asset::Animation::character_walk;
			layer->speed = net_speed / get<Walker>()->speed;
		}
	}
	else
	{
		anim = Asset::Animation::character_idle;
		layer->speed = 1.0f;
	}

	layer->animation = anim;

	get<Walker>()->enabled = fsm.current == State::Normal || fsm.current == State::Run;
}

void MinionCommon::killed(Entity* killer)
{
	get<Audio>()->post_event(AK::EVENTS::STOP);

	Entity* ragdoll = World::create<Empty>();
	ragdoll->get<Transform>()->absolute_pos(get<Transform>()->absolute_pos());

	// Apply the SkinnedModel::offset rotation to the ragdoll transform to make everything work
	ragdoll->get<Transform>()->absolute_rot(Quat::euler(0, get<Walker>()->rotation + PI * 0.5f, 0));

	SkinnedModel* new_skin = ragdoll->add<SkinnedModel>();
	SkinnedModel* old_skin = get<SkinnedModel>();
	new_skin->mesh = old_skin->mesh;

	// No rotation
	new_skin->offset.make_transform(
		Vec3(0, -1.1f, 0),
		Vec3(1.0f, 1.0f, 1.0f),
		Quat::identity
	);

	new_skin->color = old_skin->color;

	Animator* new_anim = ragdoll->add<Animator>();
	Animator* old_anim = get<Animator>();
	new_anim->armature = old_anim->armature;
	new_anim->bones.resize(old_anim->bones.length);
	for (s32 i = 0; i < old_anim->bones.length; i++)
		new_anim->bones[i] = old_anim->bones[i];

	Audio::post_global_event(AK::EVENTS::PLAY_HEADSHOT, head_pos());
	World::remove_deferred(entity());

	Ragdoll* r = ragdoll->add<Ragdoll>();
	btRigidBody* head = r->get_body(Asset::Bone::character_head)->btBody;

	if (killer)
	{
		if (killer->has<Awk>())
			head->applyImpulse(killer->get<Awk>()->velocity * 0.1f, Vec3::zero);
		else
		{
			Vec3 killer_to_head = head->getWorldTransform().getOrigin() - killer->get<Transform>()->absolute_pos();
			killer_to_head.normalize();
			head->applyImpulse(killer_to_head * 10.0f, Vec3::zero);
		}
	}
}

const s32 wall_jump_direction_count = 4;
const s32 wall_run_direction_count = 3;
Vec3 wall_directions[wall_jump_direction_count] =
{
	Vec3(-1, 0, 0),
	Vec3(1, 0, 0),
	Vec3(0, 0, 1),
	Vec3(0, 0, -1),
};

b8 MinionCommon::try_jump(r32 rotation)
{
	b8 result = false;
	if (get<Walker>()->support.ref()
		|| (last_support.ref() && Game::time.total - last_support_time < JUMP_GRACE_PERIOD && wall_run_state == WallRunState::None))
	{
		btRigidBody* body = get<RigidBody>()->btBody;
		r32 speed = fsm.current == State::Run ? 6.0f : 5.0f;
		body->setLinearVelocity(body->getLinearVelocity() + Vec3(0, speed, 0));
		last_support = get<Walker>()->support = nullptr;
		wall_run_state = WallRunState::None;
		result = true;
	}
	else
	{
		if (last_support.ref() && Game::time.total - last_support_time < JUMP_GRACE_PERIOD && wall_run_state != WallRunState::None)
		{
			wall_jump(rotation, last_support.ref()->get<Transform>()->to_world_normal(relative_wall_run_normal), last_support.ref()->btBody);
			result = true;
		}
		else
		{
			Vec3 ray_start = get<Walker>()->base_pos() + Vec3(0, get<Walker>()->support_height, 0);

			for (s32 i = 0; i < wall_jump_direction_count; i++)
			{
				Vec3 ray_end = ray_start + wall_directions[i] * get<Walker>()->radius * RAYCAST_RADIUS_RATIO;
				btCollisionWorld::ClosestRayResultCallback ray_callback(ray_start, ray_end);
				ray_callback.m_flags = btTriangleRaycastCallback::EFlags::kF_FilterBackfaces
					| btTriangleRaycastCallback::EFlags::kF_KeepUnflippedNormal;
				ray_callback.m_collisionFilterMask = ray_callback.m_collisionFilterGroup = ~CollisionWalker & ~CollisionTarget;

				Physics::btWorld->rayTest(ray_start, ray_end, ray_callback);

				if (ray_callback.hasHit())
				{
					Vec3 wall_normal = ray_callback.m_hitNormalWorld;
					const btRigidBody* support_body = dynamic_cast<const btRigidBody*>(ray_callback.m_collisionObject);
					wall_jump(rotation, wall_normal, support_body);
					result = true;
					break;
				}
			}
		}
	}

	if (result)
	{
		get<Audio>()->post_event(has<LocalPlayerControl>() ? AK::EVENTS::PLAY_JUMP_PLAYER : AK::EVENTS::PLAY_JUMP);
		fsm.transition(State::Normal);
	}

	return result;
}

void MinionCommon::wall_jump(r32 rotation, const Vec3& wall_normal, const btRigidBody* support_body)
{
	Vec3 pos = get<Walker>()->base_pos();
	Vec3 support_velocity = Vec3::zero;
	if (support_body)
	{
		support_velocity = Vec3(support_body->getLinearVelocity())
			+ Vec3(support_body->getAngularVelocity()).cross(pos - Vec3(support_body->getCenterOfMassPosition()));
	}

	RigidBody* body = get<RigidBody>();

	Vec3 velocity = body->btBody->getLinearVelocity();
	velocity.y = 0.0f;

	r32 velocity_length = LMath::clampf((velocity - support_velocity).length(), 4.0f, 8.0f);
	Vec3 velocity_reflected = Quat::euler(0, rotation, 0) * Vec3(0, 0, velocity_length);

	if (velocity_reflected.dot(wall_normal) < 0.0f)
		velocity_reflected = velocity_reflected.reflect(wall_normal);

	Vec3 new_velocity = velocity_reflected + (wall_normal * velocity_length * 0.4f) + Vec3(0, velocity_length, 0);
	body->btBody->setLinearVelocity(new_velocity);

	// Update our last supported speed so that air control will allow us to go the new speed
	get<Walker>()->last_supported_speed = new_velocity.length();
	last_support = nullptr;
}

const s32 mantle_sample_count = 3;
Vec3 mantle_samples[mantle_sample_count] =
{
	Vec3(0, 0, 1),
	Vec3(-0.35f, 0, 1),
	Vec3(0.35f, 0, 1),
};

// If force is true, we'll raycast farther downward when trying to mantle, to make sure we find something.
b8 MinionCommon::try_parkour(b8 force)
{
	if (fsm.current == State::Run || fsm.current == State::Normal || fsm.current == State::WallRun)
	{
		// Try to mantle
		Vec3 pos = get<Transform>()->absolute_pos();
		Walker* walker = get<Walker>();

		Quat rot = Quat::euler(0, walker->rotation, 0);
		for (s32 i = 0; i < mantle_sample_count; i++)
		{
			Vec3 dir_offset = rot * (mantle_samples[i] * walker->radius * RAYCAST_RADIUS_RATIO);

			Vec3 ray_start = pos + Vec3(dir_offset.x, walker->height * 0.7f, dir_offset.z);
			Vec3 ray_end = pos + Vec3(dir_offset.x, walker->height * -0.5f + (force ? -walker->support_height - 0.5f : 0.0f), dir_offset.z);

			btCollisionWorld::ClosestRayResultCallback ray_callback(ray_start, ray_end);
			ray_callback.m_flags = btTriangleRaycastCallback::EFlags::kF_FilterBackfaces
				| btTriangleRaycastCallback::EFlags::kF_KeepUnflippedNormal;
			ray_callback.m_collisionFilterMask = ray_callback.m_collisionFilterGroup = ~CollisionWalker & ~CollisionTarget;

			Physics::btWorld->rayTest(ray_start, ray_end, ray_callback);

			if (ray_callback.hasHit())
			{
				get<Animator>()->layers[1].play(Asset::Animation::character_mantle);
				fsm.transition(State::Mantle);
				last_support = Entity::list[ray_callback.m_collisionObject->getUserIndex()].get<RigidBody>();
				relative_support_pos = last_support.ref()->get<Transform>()->to_local(ray_callback.m_hitPointWorld + Vec3(0, walker->support_height + walker->height * 0.6f, 0));
				last_support_time = Game::time.total;

				get<RigidBody>()->btBody->setLinearVelocity(Vec3::zero);

				return true;
			}
		}

		if (fsm.current != State::WallRun)
		{
			// Try to wall-run

			Quat rot = Quat::euler(0, get<Walker>()->rotation, 0);
			for (s32 i = 0; i < wall_run_direction_count; i++)
			{
				if (try_wall_run((WallRunState)i, rot * wall_directions[i]))
					return true;
			}
		}
	}

	return false;
}

b8 MinionCommon::try_wall_run(WallRunState s, const Vec3& wall_direction)
{
	Vec3 ray_start = get<Walker>()->base_pos() + Vec3(0, get<Walker>()->support_height, 0);
	Vec3 ray_end = ray_start + wall_direction * get<Walker>()->radius * 3.0f;
	btCollisionWorld::ClosestRayResultCallback ray_callback(ray_start, ray_end);
	ray_callback.m_flags = btTriangleRaycastCallback::EFlags::kF_FilterBackfaces
		| btTriangleRaycastCallback::EFlags::kF_KeepUnflippedNormal;
	ray_callback.m_collisionFilterMask = ray_callback.m_collisionFilterGroup = ~CollisionWalker & ~CollisionTarget;

	Physics::btWorld->rayTest(ray_start, ray_end, ray_callback);

	if (ray_callback.hasHit() && wall_direction.dot(ray_callback.m_hitNormalWorld) < -0.7f)
	{
		btRigidBody* body = get<RigidBody>()->btBody;

		Vec3 velocity = body->getLinearVelocity();
		r32 vertical_velocity = velocity.y;
		velocity.y = 0.0f;

		Vec3 wall_normal = ray_callback.m_hitNormalWorld;

		const btRigidBody* support_body = dynamic_cast<const btRigidBody*>(ray_callback.m_collisionObject);
		Vec3 support_velocity = Vec3(support_body->getLinearVelocity())
			+ Vec3(support_body->getAngularVelocity()).cross(ray_callback.m_hitPointWorld - Vec3(support_body->getCenterOfMassPosition()));

		// if we are running on a new wall, we need to add velocity
		// if it's the same wall we were running on before, we should not add any velocity
		// this prevents the player from spamming the wall-run key to wall-run infinitely
		bool add_velocity = !last_support.ref()
			|| last_support.ref()->entity_id != support_body->getUserIndex()
			|| wall_run_state != s;

		if (s == WallRunState::Forward)
		{
			if (add_velocity && vertical_velocity - support_velocity.y > -2.0f)
			{
				// Going up
				r32 speed = LMath::clampf((velocity - support_velocity).length(), 0.0f, 4.0f);
				body->setLinearVelocity(support_velocity + Vec3(0, 4.0f + speed, 0));
			}
			else
			{
				// Going down
				body->setLinearVelocity(support_velocity + Vec3(0, (vertical_velocity - support_velocity.y) * 0.5f, 0));
			}
		}
		else
		{
			// Side wall run
			Vec3 relative_velocity = velocity - support_velocity;
			Vec3 velocity_flattened = relative_velocity - wall_normal * relative_velocity.dot(wall_normal);
			r32 flattened_vertical_speed = velocity_flattened.y;
			velocity_flattened.y = 0.0f;

			// Make sure we're facing the same way as we'll be moving
			if (velocity_flattened.dot(Quat::euler(0, get<Walker>()->rotation, 0) * Vec3(0, 0, 1)) < 0.0f)
				return false;

			r32 speed_flattened = velocity_flattened.length();
			if (speed_flattened > MAX_SPEED)
			{
				velocity_flattened *= MAX_SPEED / speed_flattened;
				speed_flattened = MAX_SPEED;
			}
			else if (speed_flattened < MIN_WALLRUN_SPEED + 1.0f)
			{
				velocity_flattened *= (MIN_WALLRUN_SPEED + 1.0f) / speed_flattened;
				speed_flattened = MIN_WALLRUN_SPEED + 1.0f;
			}

			if (add_velocity)
				body->setLinearVelocity(velocity_flattened + Vec3(support_velocity.x, support_velocity.y + 2.0f + speed_flattened * 0.3f, support_velocity.z));
			else
			{
				velocity_flattened.y = flattened_vertical_speed;
				body->setLinearVelocity(velocity_flattened);
			}
		}

		wall_run_state = s;
		last_support = Entity::list[ray_callback.m_collisionObject->getUserIndex()].get<RigidBody>();
		relative_support_pos = last_support.ref()->get<Transform>()->to_local(ray_callback.m_hitPointWorld);
		relative_wall_run_normal = last_support.ref()->get<Transform>()->to_local_normal(wall_normal);
		fsm.transition(State::WallRun);

		Plane p(wall_normal, ray_callback.m_hitPointWorld);

		Vec3 pos = get<Transform>()->absolute_pos();
		Vec3 new_pos = pos + wall_normal * (-p.distance(pos) + (get<Walker>()->radius * WALL_RUN_DISTANCE_RATIO));

		btTransform transform = body->getWorldTransform();
		transform.setOrigin(new_pos);
		body->setWorldTransform(transform);

		return true;
	}

	return false;
}

// Minion behaviors

void MinionCheckTarget::run()
{
	AI::Team team = minion->get<AIAgent>()->team;
	Vec3 pos = minion->get<Transform>()->absolute_pos();
	Entity* new_target = AI::sound_query(team, pos);

	// go to next turret
	if (!new_target)
	{
		float closest_distance = FLT_MAX;
		for (auto i = TurretControl::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->team != team)
			{
				Vec3 turret_pos = i.item()->get<Transform>()->absolute_pos();

				float total_distance = (turret_pos - pos).length();
				for (auto j = PlayerManager::list.iterator(); !j.is_last(); j.next())
				{
					if (j.item()->team != team)
					{
						total_distance += (j.item()->player_spawn.ref()->absolute_pos() - turret_pos).length();
						break;
					}
				}

				if (total_distance < closest_distance)
				{
					new_target = i.item()->entity();
					closest_distance = total_distance;
				}
			}
		}

		// go to player spawn if necessary
		for (auto j = PlayerManager::list.iterator(); !j.is_last(); j.next())
		{
			if (j.item()->team != team)
			{
				float distance = (j.item()->player_spawn.ref()->absolute_pos() - pos).length();
				if (distance < closest_distance * 0.5f)
					new_target = j.item()->player_spawn.ref()->entity();
				break;
			}
		}
	}

	if (minion->target.ref() == new_target)
		done(true);
	else
	{
		minion->target = new_target;
		done(false);
	}
}

void MinionGoToTarget::run()
{
	if (minion->target.ref())
		minion->go(minion->target.ref()->get<Transform>()->absolute_pos());
	else
		done(false);
}

void MinionGoToTarget::update(const Update& u)
{
	if (!minion->target.ref())
		done(false);
	else
	{
		Vec3 pos = minion->get<Transform>()->absolute_pos();
		Vec3 target_pos = minion->target.ref()->get<Transform>()->absolute_pos();
		if ((pos - target_pos).length_squared() < VIEW_RANGE *  VIEW_RANGE)
		{
			btCollisionWorld::ClosestRayResultCallback ray_callback(pos, target_pos);
			ray_callback.m_flags = btTriangleRaycastCallback::EFlags::kF_FilterBackfaces
				| btTriangleRaycastCallback::EFlags::kF_KeepUnflippedNormal;
			ray_callback.m_collisionFilterMask = ray_callback.m_collisionFilterGroup = ~CollisionWalker & ~CollisionTarget;
			Physics::btWorld->rayTest(pos, target_pos, ray_callback);
			if (!ray_callback.hasHit() || ray_callback.m_collisionObject->getUserIndex() == minion->target.ref()->id())
				done(true);
		}
	}
}

void MinionAttack::run()
{
	printf("%f\n", Game::time.total);
	done();
}

namespace MinionBehaviors
{
	void update_active(const Update& u)
	{
		MinionGoToTarget::update_active(u);
	}
}

// Minion AI

MinionAI::MinionAI()
{
	behavior = Repeat::alloc
	(
		-1,
		Sequence::alloc
		(
			Succeed::alloc
			(
				Parallel::alloc
				(
					Repeat::alloc
					(
						-1,
						Sequence::alloc
						(
							MinionCheckTarget::alloc(),
							Delay::alloc(2.0f)
						)
					),
					Sequence::alloc
					(
						MinionGoToTarget::alloc(),
						Repeat::alloc
						(
							-1,
							Sequence::alloc
							(
								MinionAttack::alloc(),
								Delay::alloc(3.0f)
							)
						)
					)
				)
			),
			Delay::alloc(0.0f)
		)
	);
	behavior->set_context(this);
}

void MinionAI::awake()
{
	vision_cone = VisionCone::create(get<Transform>(), VIEW_FOV, VIEW_RANGE, CONE_NORMAL);
	get<Walker>()->max_speed = get<Walker>()->speed;
	behavior->run();
}

MinionAI::~MinionAI()
{
	if (vision_cone.ref())
		World::remove(vision_cone.ref());
	behavior->~Behavior();
}

void MinionAI::update(const Update& u)
{
	{
		Vec3 relative_pos = Vec3::zero;
		Quat relative_rot = Quat::identity;
		get<MinionCommon>()->head_to_object_space(&relative_pos, &relative_rot);
		vision_cone.ref()->get<Transform>()->set(relative_pos, relative_rot);
	}
	Vec3 pos;
	Quat rot;
	vision_cone.ref()->get<Transform>()->absolute(&pos, &rot);
	Vec3 forward = rot * Vec3(0, 0, 1);
	
	if (path_index < path_point_count)
	{
		Vec3 flat_pos = pos;
		flat_pos.y = 0.0f;
		Vec3 t = path_points[path_index];
		t.y = 0.0f;
		Vec3 ray = t - flat_pos;
		while (ray.length() < 0.1f)
		{
			path_index++;
			if (path_index == path_point_count)
				break;
			t = path_points[path_index];
			t.y = 0.0f;
			ray = t - flat_pos;
		}

		// Check if we're still on the path
		if (Game::time.total - last_path_recalc > 0.25f)
		{
			if (path_index > 0)
			{
				if (path_index < path_point_count)
				{
					Vec3 last = path_points[path_index - 1];
					last.y = 0.0f;
					Vec3 next = path_points[path_index];
					next.y = 0.0f;
					Vec3 last_to_next = next - last;
					r32 last_to_next_distance = last_to_next.length();
					Vec3 last_to_next_dir = last_to_next / last_to_next_distance;
					Vec3 last_to_pos = flat_pos - last;
					r32 dot = last_to_next_dir.dot(last_to_pos);
					Vec3 desired_location = last + last_to_next_dir * dot;
					if (dot < -1.0f || dot > last_to_next_distance || (desired_location - flat_pos).length() > 0.5f)
						recalc_path(u); // We're off the path
				}
			}
			else
			{
				if (AI::get_poly(pos, AI::default_search_extents) != path_polys[0])
					recalc_path(u);
			}
		}

		ray.normalize();
		get<Walker>()->dir = Vec2(ray.x, ray.z);
	}
	else
		get<Walker>()->dir = Vec2::zero;
}

void MinionAI::go(const Vec3& target)
{
	Vec3 pos = get<Walker>()->base_pos();
	dtPolyRef start_poly = AI::get_poly(pos, AI::default_search_extents);
	dtPolyRef end_poly = AI::get_poly(target, AI::default_search_extents);

	dtPolyRef path_polys[MAX_POLYS];
	dtPolyRef path_parents[MAX_POLYS];
	u8 path_straight_flags[MAX_POLYS];
	dtPolyRef path_straight_polys[MAX_POLYS];
	s32 path_poly_count;

	AI::nav_mesh_query->findPath(start_poly, end_poly, (r32*)&pos, (r32*)&target, &AI::default_query_filter, path_polys, &path_poly_count, MAX_POLYS);
	path_index = 0;
	path_point_count = 0;
	if (path_poly_count)
	{
		// In case of partial path, make sure the end point is clamped to the last polygon.
		Vec3 epos = target;
		if (path_polys[path_poly_count - 1] != end_poly)
			AI::nav_mesh_query->closestPointOnPoly(path_polys[path_poly_count - 1], (r32*)&target, (r32*)&epos, 0);
		
		s32 point_count;
		AI::nav_mesh_query->findStraightPath((r32*)&pos, (r32*)&target, path_polys, path_poly_count,
									 (r32*)path_points, path_straight_flags,
									 path_straight_polys, &point_count, MAX_POLYS, 0);
		path_point_count = point_count;
	}
}

void MinionAI::recalc_path(const Update& u)
{
	last_path_recalc = Game::time.total;
	go(path_points[path_point_count - 1]);
}


}
