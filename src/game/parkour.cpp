#include "parkour.h"
#include "walker.h"
#include "entities.h"
#include "data/animator.h"
#include "game.h"
#include "asset/animation.h"
#include "asset/armature.h"
#include "asset/mesh.h"
#include "asset/shader.h"
#include "asset/Wwise_IDs.h"
#include "audio.h"
#include "BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "awk.h"

namespace VI
{

#define RAYCAST_RADIUS_RATIO 1.3f
#define WALL_RUN_DISTANCE_RATIO 1.1f

#define RUN_SPEED 5.0f
#define WALK_SPEED 2.0f
#define MAX_SPEED 7.0f
#define MIN_WALLRUN_SPEED 1.0f

#define JUMP_GRACE_PERIOD 0.3f

Traceur::Traceur(const Vec3& pos, const Quat& quat, AI::Team team)
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
	model->color.w = 1.0f / 255.0f; // special G-buffer index prevents override lights from overriding this color

	create<Audio>();

	Health* health = create<Health>(AWK_HEALTH);
	
	Vec3 forward = quat * Vec3(0, 0, 1);

	Walker* walker = create<Walker>(atan2f(forward.x, forward.z));
	walker->max_speed = MAX_SPEED;

	create<AIAgent>()->team = team;

	create<Parkour>();
}

void Parkour::awake()
{
	Animator* animator = get<Animator>();
	animator->layers[1].loop = false;
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_walk, 0.3375f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_walk, 0.75f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_run, 0.216f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_run, 0.476f));
}

Vec3 Parkour::head_pos()
{
	Vec3 pos = Vec3(0.1f, 0, 0);
	Quat rot = Quat::identity;
	get<Animator>()->to_world(Asset::Bone::character_head, &pos, &rot);
	return pos;
}

void Parkour::head_to_object_space(Vec3* pos, Quat* rot)
{
	Vec3 offset_pos = Vec3(0.05f, 0, 0.13f) + *pos;
	Quat offset_quat = Quat::euler(0, 0, PI * 0.5f) * *rot;
	get<Animator>()->bone_transform(Asset::Bone::character_head, &offset_pos, &offset_quat);
	*pos = (get<SkinnedModel>()->offset * Vec4(offset_pos)).xyz();
	*rot = (Quat::euler(0, get<Walker>()->rotation, 0) * offset_quat);
}

void Parkour::set_run(b8 r)
{
	if (r && fsm.current == State::Normal)
		fsm.transition(State::Run);
	else if (!r && (fsm.current == State::Run || fsm.current == State::WallRun))
		fsm.transition(State::Normal);
	get<Walker>()->speed = r ? RUN_SPEED : WALK_SPEED;
}

void Parkour::footstep()
{
	Vec3 base_pos = get<Walker>()->base_pos();

	Audio::post_global_event(AK::EVENTS::PLAY_FOOTSTEP, base_pos);

	ShockwaveEntity* shockwave = World::create<ShockwaveEntity>(entity(), 3.0f);
	shockwave->get<Transform>()->pos = base_pos;
	shockwave->get<Transform>()->reparent(get<Transform>()->parent.ref());
}

void Parkour::update(const Update& u)
{
	fsm.time += u.time.delta;
	get<SkinnedModel>()->offset.make_transform(
		Vec3(0, -1.1f, 0),
		Vec3(1.0f, 1.0f, 1.0f),
		Quat::euler(0, get<Walker>()->rotation + PI * 0.5f, 0)
	);

	if (get<Walker>()->support.ref())
		wall_run_state = WallRunState::None;

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

const s32 wall_jump_direction_count = 4;
const s32 wall_run_direction_count = 3;
Vec3 wall_directions[wall_jump_direction_count] =
{
	Vec3(-1, 0, 0),
	Vec3(1, 0, 0),
	Vec3(0, 0, 1),
	Vec3(0, 0, -1),
};

b8 Parkour::try_jump(r32 rotation)
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

void Parkour::wall_jump(r32 rotation, const Vec3& wall_normal, const btRigidBody* support_body)
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
b8 Parkour::try_parkour(b8 force)
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

b8 Parkour::try_wall_run(WallRunState s, const Vec3& wall_direction)
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
		b8 add_velocity = !last_support.ref()
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


}
