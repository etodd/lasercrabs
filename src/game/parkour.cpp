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
#define WALL_JUMP_RAYCAST_RADIUS_RATIO 1.5f
#define WALL_RUN_DISTANCE_RATIO 1.1f

#define RUN_SPEED 5.0f
#define WALK_SPEED 2.0f
#define MAX_SPEED 7.0f
#define MIN_WALLRUN_SPEED 2.0f

#define JUMP_GRACE_PERIOD 0.3f

#define TILE_CREATE_RADIUS 4.5f

Traceur::Traceur(const Vec3& pos, const Quat& quat, AI::Team team)
{
	Transform* transform = create<Transform>();
	transform->pos = pos;

	Animator* animator = create<Animator>();
	SkinnedModel* model = create<SkinnedModel>();

	animator->armature = Asset::Armature::character;
	animator->layers[0].animation = Asset::Animation::character_idle;

	model->shader = Asset::Shader::armature;
	model->mesh = Asset::Mesh::character;
	model->team = (u8)team;

	create<Audio>();
	
	Vec3 forward = quat * Vec3(0, 0, 1);

	Walker* walker = create<Walker>(atan2f(forward.x, forward.z));
	walker->max_speed = MAX_SPEED;
	walker->speed = RUN_SPEED;
	walker->auto_rotate = false;

	create<AIAgent>()->team = team;

	create<Parkour>();
}

r32 Parkour::min_y;

void Parkour::awake()
{
	Animator* animator = get<Animator>();
	animator->layers[1].loop = false;
	animator->layers[1].blend_time = 0.1f;
	animator->layers[2].loop = true;
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_walk, 0.3375f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_walk, 0.75f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_run, 0.216f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_run, 0.476f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_wall_run_left, 0.216f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_wall_run_left, 0.476f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_wall_run_right, 0.216f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_wall_run_right, 0.476f));
	link_arg<r32, &Parkour::land>(get<Walker>()->land);
}

void Parkour::land(r32 velocity_diff)
{
	if (fsm.current == State::Normal && velocity_diff < 5.0f * -1.25f)
		get<Animator>()->layers[1].play(Asset::Animation::character_land);
}

Vec3 Parkour::head_pos() const
{
	Vec3 pos = Vec3(0.1f, 0, 0);
	Quat rot = Quat::identity;
	get<Animator>()->to_world(Asset::Bone::character_head, &pos, &rot);
	return pos;
}

void Parkour::head_to_object_space(Vec3* pos, Quat* rot) const
{
	Vec3 offset_pos = Vec3(0.05f, 0, 0.13f) + *pos;
	Quat offset_quat = Quat::euler(0, 0, PI * 0.5f) * *rot;
	get<Animator>()->bone_transform(Asset::Bone::character_head, &offset_pos, &offset_quat);
	*pos = (get<SkinnedModel>()->offset * Vec4(offset_pos)).xyz();
	*rot = (Quat::euler(0, get<Walker>()->rotation, 0) * offset_quat);
}

void Parkour::footstep()
{
	Vec3 base_pos = get<Walker>()->base_pos();

	Audio::post_global_event(AK::EVENTS::PLAY_FOOTSTEP, base_pos);

	ShockwaveEntity* shockwave = World::create<ShockwaveEntity>(3.0f);
	shockwave->get<Transform>()->pos = base_pos;
	shockwave->get<Transform>()->reparent(get<Transform>()->parent.ref());
}

b8 Parkour::wallrun(const Update& u, RigidBody* wall, const Vec3& relative_wall_pos, const Vec3& relative_wall_normal)
{
	b8 exit_wallrun = false;
	if (!wall)
	{
		exit_wallrun = true;
		return exit_wallrun;
	}

	Vec3 absolute_wall_normal = wall->get<Transform>()->to_world_normal(relative_wall_normal);
	Vec3 absolute_wall_pos = wall->get<Transform>()->to_world(relative_wall_pos);

	btRigidBody* body = get<RigidBody>()->btBody;
	
	Vec3 velocity = body->getLinearVelocity();
	velocity += Vec3(Physics::btWorld->getGravity()) * -0.5f * u.time.delta; // cancel gravity a bit

	{
		r32 speed = Vec2(velocity.x, velocity.z).length();
		velocity -= absolute_wall_normal * absolute_wall_normal.dot(velocity); // keep us on the wall
		r32 new_speed = Vec2(velocity.x, velocity.z).length();
		if (new_speed < speed) // make sure we don't slow down at all though
		{
			r32 scale = speed / new_speed;
			velocity.x *= scale;
			velocity.z *= scale;
		}
	}

	Vec3 support_velocity = Vec3::zero;
	if (wall)
	{
		support_velocity = Vec3(wall->btBody->getLinearVelocity())
			+ Vec3(wall->btBody->getAngularVelocity()).cross(absolute_wall_pos - Vec3(wall->btBody->getCenterOfMassPosition()));
	}

	Vec3 horizontal_velocity_diff = velocity - support_velocity;
	r32 vertical_velocity_diff = horizontal_velocity_diff.y;
	horizontal_velocity_diff.y = 0.0f;
	if (wall_run_state != WallRunState::Forward && horizontal_velocity_diff.length() < MIN_WALLRUN_SPEED)
		exit_wallrun = true; // We're going too slow
	else
	{
		body->setLinearVelocity(velocity);

		Plane p(absolute_wall_normal, absolute_wall_pos);

		Vec3 pos = get<Transform>()->absolute_pos();
		r32 wall_distance = get<Walker>()->radius * WALL_RUN_DISTANCE_RATIO;
		Vec3 new_pos = pos + absolute_wall_normal * (-p.distance(pos) + wall_distance);

		btTransform transform = body->getWorldTransform();
		transform.setOrigin(new_pos);
		body->setWorldTransform(transform);

		last_support = wall;
		relative_wall_run_normal = relative_wall_normal;

		// Face the correct direction
		{
			Vec3 forward;
			if (wall_run_state == WallRunState::Forward)
			{
				forward = -absolute_wall_normal;
				forward.normalize();
			}
			else
			{
				forward = horizontal_velocity_diff;
				forward.normalize();
			}
			get<Walker>()->target_rotation = atan2(forward.x, forward.z);
		}

		// Update animation speed
		Animator::Layer* layer = &get<Animator>()->layers[0];
		if (wall_run_state == WallRunState::Forward)
			layer->speed = vi_max(0.0f, vertical_velocity_diff / get<Walker>()->speed);
		else
			layer->speed = horizontal_velocity_diff.length() / get<Walker>()->speed;

		// Try to climb stuff while we're wall-running
		if (try_parkour())
			exit_wallrun = true;
		else
		{
			relative_support_pos = last_support.ref()->get<Transform>()->to_local(get<Walker>()->base_pos() + absolute_wall_normal * -wall_distance);
			last_support_time = Game::time.total;
		}
	}

	if (!exit_wallrun && wall_run_state != WallRunState::Forward)
	{
		Vec3 relative_wall_right = relative_wall_run_normal.cross(last_support.ref()->get<Transform>()->to_local_normal(Vec3(0, 1, 0)));
		relative_wall_right.normalize();
		Vec3 relative_wall_up = relative_wall_right.cross(relative_wall_run_normal);
		relative_wall_up.normalize();

		Vec3 spawn_offset = ((velocity - support_velocity) * 1.5f) + (absolute_wall_normal * -3.0f);
		spawn_tiles(relative_wall_right, relative_wall_up, relative_wall_normal, spawn_offset);
	}

	return exit_wallrun;
}

b8 Parkour::TilePos::operator==(const Parkour::TilePos& other) const
{
	return x == other.x && y == other.y;
}

b8 Parkour::TilePos::operator!=(const Parkour::TilePos& other) const
{
	return x != other.x || y != other.y;
}

void Parkour::update(const Update& u)
{
	if (get<Transform>()->absolute_pos().y < min_y)
	{
		World::remove_deferred(entity());
		return;
	}

	fsm.time += u.time.delta;
	get<SkinnedModel>()->offset.make_transform(
		Vec3(0, -1.1f, 0),
		Vec3(1.0f, 1.0f, 1.0f),
		Quat::euler(0, get<Walker>()->rotation + PI * 0.5f, 0) * Quat::euler(0, 0, lean * -1.5f)
	);

	if (get<Walker>()->support.ref())
	{
		wall_run_state = WallRunState::None;
		can_double_jump = true;
		tile_history.length = 0;
	}

	// animation layers
	// layer 0 = running, walking, wall-running
	// layer 1 = mantle, land, jump, wall-jump
	// layer 2 = slide

	r32 lean_target = 0.0f;

	if (fsm.current == State::Mantle)
	{
		get<Animator>()->layers[1].animation = Asset::Animation::character_mantle;
		get<Animator>()->layers[2].animation = AssetNull;
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
				adjustment = Vec3(0, vi_min(distance, u.time.delta / time_left), 0);
			}
			else
			{
				// Move horizontally
				r32 distance = diff.length();
				r32 time_left = mantle_time - fsm.time;
				adjustment = diff * vi_min(1.0f, u.time.delta / time_left);
			}
			get<RigidBody>()->btBody->setWorldTransform(btTransform(Quat::identity, start + adjustment));
		}
		last_support_time = Game::time.total;
	}
	else if (fsm.current == State::WallRun)
	{
		get<Animator>()->layers[1].animation = AssetNull;
		get<Animator>()->layers[2].animation = AssetNull;
		b8 exit_wallrun = false;

		// under certain circumstances, check if we have ground beneath us
		// if so, stop wall-running
		Vec3 support_velocity = Vec3::zero;
		if (last_support.ref())
			support_velocity = get_support_velocity(relative_support_pos, last_support.ref()->btBody);
		Vec3 velocity = get<RigidBody>()->btBody->getLinearVelocity();
		if (fsm.time > 0.2f || velocity.y - support_velocity.y < 0.0f)
		{
			btCollisionWorld::ClosestRayResultCallback support_callback = get<Walker>()->check_support();
			if (support_callback.hasHit())
				exit_wallrun = true;
		}

		if (!exit_wallrun)
		{
			Vec3 ray_start = get<Walker>()->base_pos() + Vec3(0, get<Walker>()->support_height, 0);

			Vec3 wall_run_normal = last_support.ref()->get<Transform>()->to_world_normal(relative_wall_run_normal);

			Quat rot = Quat::euler(0, get<Walker>()->rotation, 0);
			Vec3 forward = rot * Vec3(0, 0, 1);
			if (wall_run_state == WallRunState::Left || wall_run_state == WallRunState::Right)
			{
				// check in front of us
				Vec3 ray_dir = rot * Vec3(wall_run_state == WallRunState::Left ? 1 : -1, 0, 1);
				Vec3 ray_end = ray_start + ray_dir * get<Walker>()->radius * WALL_RUN_DISTANCE_RATIO * 2.0f;
				btCollisionWorld::ClosestRayResultCallback ray_callback(ray_start, ray_end);
				ray_callback.m_flags = btTriangleRaycastCallback::EFlags::kF_FilterBackfaces
					| btTriangleRaycastCallback::EFlags::kF_KeepUnflippedNormal;
				ray_callback.m_collisionFilterMask = ray_callback.m_collisionFilterGroup = ~CollisionWalker & ~CollisionTarget;

				Physics::btWorld->rayTest(ray_start, ray_end, ray_callback);

				if (ray_callback.hasHit() && wall_run_normal.dot(ray_callback.m_hitNormalWorld) < 0.99f)
				{
					if (forward.dot(ray_callback.m_hitNormalWorld) < -0.8f)
					{
						// the wall is facing directly toward us; run up it
						wall_run_state = WallRunState::Forward;
						wall_run_up_add_velocity(get<RigidBody>()->btBody->getLinearVelocity(), get_support_velocity(ray_callback.m_hitPointWorld, ray_callback.m_collisionObject));
					}
					last_support = Entity::list[ray_callback.m_collisionObject->getUserIndex()].get<RigidBody>();
					relative_support_pos = last_support.ref()->get<Transform>()->to_local(ray_callback.m_hitPointWorld);
					wall_run_normal = ray_callback.m_hitNormalWorld;
					relative_wall_run_normal = last_support.ref()->get<Transform>()->to_local_normal(wall_run_normal);
				}
			}

			Vec3 ray_end = ray_start + wall_run_normal * get<Walker>()->radius * WALL_RUN_DISTANCE_RATIO * -2.0f;
			btCollisionWorld::ClosestRayResultCallback ray_callback(ray_start, ray_end);
			ray_callback.m_flags = btTriangleRaycastCallback::EFlags::kF_FilterBackfaces
				| btTriangleRaycastCallback::EFlags::kF_KeepUnflippedNormal;
			ray_callback.m_collisionFilterMask = ray_callback.m_collisionFilterGroup = ~CollisionWalker & ~CollisionTarget;

			Physics::btWorld->rayTest(ray_start, ray_end, ray_callback);

			if (ray_callback.hasHit()
				&& wall_run_normal.dot(ray_callback.m_hitNormalWorld) > 0.5f
				&& forward.dot(ray_callback.m_hitNormalWorld) < 0.1f)
			{
				// Still on the wall
				RigidBody* wall = Entity::list[ray_callback.m_collisionObject->getUserIndex()].get<RigidBody>();
				Vec3 relative_normal = wall->get<Transform>()->to_local_normal(ray_callback.m_hitNormalWorld);
				Vec3 relative_pos = wall->get<Transform>()->to_local(ray_callback.m_hitPointWorld);
				exit_wallrun = wallrun(u, wall, relative_pos, relative_normal);
			}
			else // ran out of wall to run on
			{
				if (wall_run_state == WallRunState::Forward)
				{
					exit_wallrun = true;
					try_parkour(true); // do an extra broad raycast to make sure we hit the top if at all possible
				}
				else
				{
					// keep going, generate a wall
					exit_wallrun = wallrun(u, last_support.ref(), relative_support_pos, relative_wall_run_normal);
				}
			}
		}

		if (exit_wallrun && fsm.current == State::WallRun)
		{
			fsm.transition(State::Normal);
			wall_run_state = WallRunState::None;
		}
	}
	else if (fsm.current == State::Normal)
	{
		get<Animator>()->layers[2].animation = AssetNull;
		if (get<Walker>()->support.ref())
		{
			Animator::Layer* layer1 = &get<Animator>()->layers[1];
			if (layer1->animation == Asset::Animation::character_jump1)
				layer1->animation = AssetNull; // stop jump animations
			last_support_time = Game::time.total;
			last_support = get<Walker>()->support;
			relative_support_pos = last_support.ref()->get<Transform>()->to_local(get<Walker>()->base_pos());
			lean_target = get<Walker>()->net_speed * LMath::angle_to(get<PlayerCommon>()->angle_horizontal, get<PlayerCommon>()->last_angle_horizontal) * (1.0f / 180.0f) / u.time.delta;
		}
	}
	else if (fsm.current == State::Slide)
	{
		if (!last_support.ref()) // support is gone
			fsm.transition(State::Normal);
		else
		{
			// check how fast we're going
			Vec3 support_velocity = get_support_velocity(relative_support_pos, last_support.ref()->btBody);
			Vec3 velocity = get<RigidBody>()->btBody->getLinearVelocity();
			Vec3 relative_velocity = velocity - support_velocity;
			Vec3 forward = Quat::euler(0, get<Walker>()->rotation, 0) * Vec3(0, 0, 1);
			if (relative_velocity.dot(forward) < MIN_WALLRUN_SPEED) // going too slow
				fsm.transition(State::Normal);
			else
			{
				// keep sliding
				Transform* last_support_transform = last_support.ref()->get<Transform>();

				// do damping
				relative_velocity -= Vec3::normalize(relative_velocity) * u.time.delta * 3.0f;

				// handle support
				{
					Vec3 base_pos = get<Transform>()->absolute_pos();
					r32 base_offset = get<Walker>()->height * 0.5f; // don't include support_height
					base_pos.y -= base_offset;
					Vec3 absolute_support_pos = last_support_transform->to_world(relative_support_pos);
					Vec3 support_normal = last_support_transform->to_world_normal(relative_wall_run_normal);
					Vec3 support_diff = base_pos - absolute_support_pos;
					r32 support_diff_normal = support_diff.dot(support_normal);
					Vec3 projected_support = base_pos - (support_diff_normal * support_normal);
					if (support_diff_normal < 0.0f)
					{
						get<Transform>()->absolute_pos(projected_support + Vec3(0, base_offset, 0));
						r32 velocity_normal = relative_velocity.dot(support_normal);
						relative_velocity -= support_normal * (velocity_normal * support_normal);
					}
					

					// update relative support pos
					relative_support_pos = last_support_transform->to_local(projected_support);
					last_support_time = Game::time.total;
				}

				Vec3 new_velocity = support_velocity + relative_velocity;
				get<RigidBody>()->btBody->setLinearVelocity(new_velocity);

				// spawn tiles
				Vec3 spawn_offset = new_velocity * 1.5f;
				spawn_offset.y = -2.0f;
				Vec3 support_normal = relative_wall_run_normal;
				Vec3 support_right = Vec3::normalize(support_normal.cross(last_support_transform->to_local_normal(Vec3(1, 0, 0))));
				Vec3 support_forward = support_normal.cross(support_right);
				spawn_tiles(support_right, support_forward, support_normal, spawn_offset);
			}
		}
	}

	// update animation

	lean += (lean_target - lean) * vi_min(1.0f, 20.0f * u.time.delta);

	Animator::Layer* layer0 = &get<Animator>()->layers[0];
	if (fsm.current == State::WallRun)
	{
		// speed already set
		if (wall_run_state == WallRunState::Left)
			layer0->animation = Asset::Animation::character_wall_run_left;
		else if (wall_run_state == WallRunState::Right)
			layer0->animation = Asset::Animation::character_wall_run_right;
		else
		{
			layer0->animation = Asset::Animation::character_run;
			// todo: wall run straight animation
		}
	}
	else if (get<Walker>()->support.ref())
	{
		if (get<Walker>()->dir.length_squared() > 0.0f)
		{
			// walking/running
			r32 net_speed = vi_max(get<Walker>()->net_speed, WALK_SPEED * 0.5f);
			layer0->animation = net_speed > WALK_SPEED ? Asset::Animation::character_run : Asset::Animation::character_walk;
			layer0->speed = net_speed > WALK_SPEED ? LMath::lerpf((net_speed - WALK_SPEED) / RUN_SPEED, 0.75f, 1.0f) : (net_speed / WALK_SPEED);
		}
		else
		{
			// standing still
			layer0->animation = Asset::Animation::character_idle;
			layer0->speed = 1.0f;
		}
	}
	else
	{
		// floating in space
		layer0->animation = Asset::Animation::character_fall;
		layer0->speed = 1.0f;
	}

	get<Walker>()->enabled = fsm.current == State::Normal;
}

void Parkour::spawn_tiles(const Vec3& relative_wall_right, const Vec3& relative_wall_up, const Vec3& relative_wall_normal, const Vec3& spawn_offset)
{
	TilePos wall_coord =
	{
		(s32)(relative_support_pos.dot(relative_wall_right) * (1.0f / TILE_SIZE)),
		(s32)(relative_support_pos.dot(relative_wall_up) * (1.0f / TILE_SIZE)),
	};

	bool new_wall_coord = true;
	if (tile_history.length > 0)
	{
		for (s32 i = tile_history.length - 1; i >= 0; i--)
		{
			if (wall_coord == tile_history[i])
			{
				new_wall_coord = false;
				break;
			}
		}
	}

	if (new_wall_coord)
	{
		r32 relative_wall_z = relative_support_pos.dot(relative_wall_normal) - 0.05f;

		Vec3 absolute_wall_normal = last_support.ref()->get<Transform>()->to_world_normal(relative_wall_normal);
		Quat absolute_wall_rot = Quat::look(absolute_wall_normal);

		s32 i = 0;
		for (s32 x = -TILE_CREATE_RADIUS; x <= (s32)TILE_CREATE_RADIUS; x++)
		{
			for (s32 y = -TILE_CREATE_RADIUS; y <= (s32)TILE_CREATE_RADIUS; y++)
			{
				if (Vec2(x, y).length_squared() < TILE_CREATE_RADIUS * TILE_CREATE_RADIUS)
				{
					b8 create = true;
					for (s32 i = tile_history.length - 1; i >= 0; i--)
					{
						const TilePos& history_coord = tile_history[i];
						if (Vec2(wall_coord.x + x - history_coord.x, wall_coord.y + y - history_coord.y).length_squared() < (TILE_CREATE_RADIUS + TILE_SIZE) * (TILE_CREATE_RADIUS + TILE_SIZE))
						{
							create = false;
							break;
						}
					}

					if (create)
					{
						Vec2 relative_tile_wall_coord = Vec2(wall_coord.x + x, wall_coord.y + y) * TILE_SIZE;
						Vec3 relative_tile_pos = (relative_wall_right * relative_tile_wall_coord.x)
							+ (relative_wall_up * relative_tile_wall_coord.y)
							+ (relative_wall_normal * relative_wall_z);
						Vec3 absolute_tile_pos = last_support.ref()->get<Transform>()->to_world(relative_tile_pos);

						r32 anim_time = tile_history.length == 0 ? (0.03f + 0.01f * i) : 0.3f;
						World::create<TileEntity>(absolute_tile_pos, absolute_wall_rot, last_support.ref()->get<Transform>(), spawn_offset, anim_time);

						i++;
					}
				}
			}
		}
		if (tile_history.length == MAX_TILE_HISTORY)
			tile_history.remove_ordered(0);
		tile_history.add(wall_coord);
	}
}

const s32 wall_jump_direction_count = 4;
const s32 wall_run_direction_count = 3;
Vec3 wall_directions[wall_jump_direction_count] =
{
	Vec3(1, 0, 0),
	Vec3(-1, 0, 0),
	Vec3(0, 0, 1),
	Vec3(0, 0, -1),
};

void Parkour::do_normal_jump()
{
	btRigidBody* body = get<RigidBody>()->btBody;
	const r32 speed = 6.0f;
	Vec3 new_velocity = body->getLinearVelocity();
	new_velocity.y = vi_max(0.0f, new_velocity.y) + speed;
	body->setLinearVelocity(new_velocity);
	last_support = get<Walker>()->support = nullptr;
	wall_run_state = WallRunState::None;
	get<Animator>()->layers[1].play(Asset::Animation::character_jump1);
}

b8 Parkour::try_jump(r32 rotation)
{
	b8 did_jump = false;
	if (get<Walker>()->support.ref()
		|| fsm.current == State::Slide
		|| (last_support.ref() && Game::time.total - last_support_time < JUMP_GRACE_PERIOD && wall_run_state == WallRunState::None))
	{
		do_normal_jump();
		did_jump = true;
	}
	else
	{
		// try wall-jumping
		if (last_support.ref() && Game::time.total - last_support_time < JUMP_GRACE_PERIOD && wall_run_state != WallRunState::None)
		{
			wall_jump(rotation, last_support.ref()->get<Transform>()->to_world_normal(relative_wall_run_normal), last_support.ref()->btBody);
			did_jump = true;
		}
		else
		{
			Vec3 ray_start = get<Walker>()->base_pos() + Vec3(0, get<Walker>()->support_height, 0);

			for (s32 i = 0; i < wall_jump_direction_count; i++)
			{
				Vec3 ray_end = ray_start + wall_directions[i] * get<Walker>()->radius * WALL_JUMP_RAYCAST_RADIUS_RATIO;
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
					did_jump = true;
					break;
				}
			}

			if (!did_jump)
			{
				// we couldn't wall-jump
				// try to double-jump
				if (can_double_jump)
				{
					Vec3 spawn_offset = get<RigidBody>()->btBody->getLinearVelocity() * 1.5f;
					spawn_offset.y = 5.0f;

					Vec3 pos = get<Walker>()->base_pos();

					Quat tile_rot = Quat::look(Vec3(0, 1, 0));
					const r32 radius = TILE_CREATE_RADIUS - 2.0f;
					s32 i = 0;
					for (s32 x = -(s32)TILE_CREATE_RADIUS; x <= (s32)radius; x++)
					{
						for (s32 y = -(s32)TILE_CREATE_RADIUS; y <= (s32)radius; y++)
						{
							if (Vec2(x, y).length_squared() < radius * radius)
							{
								World::create<TileEntity>(pos + Vec3(x, 0, y) * TILE_SIZE, tile_rot, nullptr, spawn_offset, 0.15f + 0.05f * i);
								i++;
							}
						}
					}

					// override horizontal velocity based on current facing angle
					Vec3 velocity = get<RigidBody>()->btBody->getLinearVelocity();
					Vec3 horizontal_velocity = velocity;
					horizontal_velocity.y = 0.0f;
					Vec3 new_velocity = Quat::euler(0, get<Walker>()->rotation, 0) * Vec3(0, 0, horizontal_velocity.length());
					new_velocity.y = velocity.y;
					get<RigidBody>()->btBody->setLinearVelocity(new_velocity);

					do_normal_jump();

					can_double_jump = false;
					did_jump = true;
				}
			}
		}
	}

	if (did_jump)
	{
		get<Audio>()->post_event(has<LocalPlayerControl>() ? AK::EVENTS::PLAY_JUMP_PLAYER : AK::EVENTS::PLAY_JUMP);
		fsm.transition(State::Normal);
	}

	return did_jump;
}

Vec3 Parkour::get_support_velocity(const Vec3& absolute_pos, const btCollisionObject* support) const
{
	Vec3 support_velocity = Vec3::zero;
	if (support)
	{
		const btRigidBody* support_body = dynamic_cast<const btRigidBody*>(support);
		support_velocity = Vec3(support_body->getLinearVelocity())
			+ Vec3(support_body->getAngularVelocity()).cross(get<Walker>()->base_pos() - Vec3(support_body->getCenterOfMassPosition()));
	}
	return support_velocity;
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

b8 Parkour::try_slide()
{
	if (get<Walker>()->support.ref() && fsm.current == State::Normal)
	{
		btCollisionWorld::ClosestRayResultCallback support_callback = get<Walker>()->check_support();
		Vec3 support_velocity = get_support_velocity(support_callback.m_hitPointWorld, support_callback.m_collisionObject);
		Vec3 velocity = get<RigidBody>()->btBody->getLinearVelocity();
		Vec3 relative_velocity = velocity - support_velocity;
		Vec3 forward = Quat::euler(0, get<Walker>()->rotation, 0) * Vec3(0, 0, 1);
		if (relative_velocity.dot(forward) > MIN_WALLRUN_SPEED)
		{
			velocity += forward * 2.0f;
			get<RigidBody>()->btBody->setLinearVelocity(velocity);
			fsm.transition(State::Slide);
			last_support = get<Walker>()->support;
			relative_wall_run_normal = last_support.ref()->get<Transform>()->to_local_normal(support_callback.m_hitNormalWorld);
			get<Animator>()->layers[2].play(Asset::Animation::character_slide);
			can_double_jump = true;
			return true;
		}
	}
	return false;
}

// If force is true, we'll raycast farther downward when trying to mantle, to make sure we find something.
b8 Parkour::try_parkour(b8 force)
{
	if (fsm.current == State::Normal || fsm.current == State::WallRun)
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

void Parkour::wall_run_up_add_velocity(const Vec3& velocity, const Vec3& support_velocity)
{
	Vec3 horizontal_velocity = velocity - support_velocity;
	r32 vertical_velocity = horizontal_velocity.y;
	horizontal_velocity.y = 0.0f;
	r32 speed = LMath::clampf(horizontal_velocity.length(), 0.0f, 5.0f);
	get<RigidBody>()->btBody->setLinearVelocity(support_velocity + Vec3(0, 4.5f + speed + vertical_velocity - support_velocity.y, 0));
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
				wall_run_up_add_velocity(velocity, support_velocity);
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
				body->setLinearVelocity(velocity_flattened + Vec3(support_velocity.x, support_velocity.y + 3.0f + speed_flattened * 0.5f, support_velocity.z));
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

		can_double_jump = true;

		return true;
	}

	return false;
}


}
