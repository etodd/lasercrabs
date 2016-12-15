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
#include "minion.h"
#include "render/particles.h"
#include "mersenne/mersenne-twister.h"
#include "team.h"

namespace VI
{

#define RAYCAST_RADIUS_RATIO 1.3f
#define WALL_JUMP_RAYCAST_RADIUS_RATIO 2.0f
#define WALL_RUN_DISTANCE_RATIO 1.1f

#define RUN_SPEED 5.0f
#define WALK_SPEED 2.25f
#define MAX_SPEED 6.0f
#define MIN_WALLRUN_SPEED 3.0f
#define MIN_ATTACK_SPEED 4.0f
#define JUMP_SPEED 5.5f
#define COLLECTIBLE_RADIUS 1.0f

#define JUMP_GRACE_PERIOD 0.3f

#define MIN_SLIDE_TIME 0.5f

Traceur::Traceur(const Vec3& pos, const Quat& quat, AI::Team team)
{
	Transform* transform = create<Transform>();
	transform->pos = pos;

	Animator* animator = create<Animator>();
	SkinnedModel* model = create<SkinnedModel>();

	animator->armature = Asset::Armature::character;

	model->shader = Asset::Shader::armature;
	model->mesh_shadow = Asset::Mesh::parkour;
	model->mesh = Asset::Mesh::parkour_headless;
	model->team = s8(team);

	create<Audio>();
	
	Vec3 forward = quat * Vec3(0, 0, 1);

	Walker* walker = create<Walker>(atan2f(forward.x, forward.z));
	walker->max_speed = MAX_SPEED;
	walker->speed = RUN_SPEED;
	walker->auto_rotate = false;

	create<AIAgent>()->team = team;

	create<Health>(AWK_HEALTH, AWK_HEALTH, AWK_SHIELD, AWK_SHIELD);

	create<Parkour>();
}

void Parkour::awake()
{
	Animator* animator = get<Animator>();
	animator->layers[0].loop = true;
	animator->layers[0].play(Asset::Animation::character_idle);
	animator->layers[1].loop = false;
	animator->layers[1].blend_time = 0.2f;
	animator->layers[2].loop = true;
	animator->layers[3].loop = false;
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_walk, 0.3375f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_walk, 0.75f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_run, 0.216f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_run, 0.476f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_wall_run_left, 0.216f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_wall_run_left, 0.476f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_wall_run_right, 0.216f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_wall_run_right, 0.476f));
	link<&Parkour::pickup_animation_complete>(animator->trigger(Asset::Animation::character_pickup, 3.5f));
	link_arg<r32, &Parkour::land>(get<Walker>()->land);
	link_arg<Entity*, &Parkour::killed>(get<Health>()->killed);
}

void Parkour::killed(Entity*)
{
	if (Game::level.local)
	{
		Team::game_over = true;
		World::remove_deferred(entity());
		Game::schedule_load_level(Game::level.id, Game::Mode::Parkour, 2.0f);
	}
}

void Parkour::land(r32 velocity_diff)
{
	if (fsm.current == State::Normal)
	{
		if (velocity_diff < LANDING_VELOCITY_LIGHT)
		{
			if (velocity_diff < LANDING_VELOCITY_HARD)
			{
				// hard landing
				fsm.transition(State::HardLanding);
				get<Walker>()->max_speed = 0.0f;
				get<Walker>()->speed = 0.0f;
				get<Animator>()->layers[1].play(Asset::Animation::character_land_hard);
			}
			else // light landing
				get<Animator>()->layers[1].play(Asset::Animation::character_land);
		}
	}
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
	Vec3 offset_pos = Vec3(0.05f, 0, 0.0f) + *pos;
	Quat offset_quat = Quat::euler(0, 0, PI * 0.5f) * *rot;
	get<Animator>()->bone_transform(Asset::Bone::character_head, &offset_pos, &offset_quat);
	*pos = (get<SkinnedModel>()->offset * Vec4(offset_pos)).xyz();
	*rot = (Quat::euler(0, get<Walker>()->rotation, 0) * offset_quat);
}

void Parkour::footstep()
{
	if (fsm.current == State::Normal || fsm.current == State::WallRun)
	{
		Vec3 base_pos = get<Walker>()->base_pos();

		Audio::post_global_event(AK::EVENTS::PLAY_FOOTSTEP, base_pos);

		RigidBody* support = get<Walker>()->support.ref();
		Shockwave::add(base_pos, 1.0f, 5.0f, support ? support->get<Transform>() : nullptr);
	}
}

b8 Parkour::wallrun(const Update& u, RigidBody* wall, const Vec3& relative_wall_pos, const Vec3& relative_wall_normal)
{
	b8 exit_wallrun = false;

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
		support_velocity = Walker::get_support_velocity(absolute_wall_pos, wall->btBody);

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
		r32 wall_distance = WALKER_RADIUS * WALL_RUN_DISTANCE_RATIO;
		get<Walker>()->absolute_pos(pos + absolute_wall_normal * (-p.distance(pos) + wall_distance));

		last_support = wall;
		last_support_wall_run_state = wall_run_state;
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

	return exit_wallrun;
}

namespace ParkourNet
{
	enum Message
	{
		Pickup,
		StateSync,
		Kill,
		count,
	};

	b8 pickup(Parkour* parkour, Collectible* collectible)
	{
		using Stream = Net::StreamWrite;
		Stream* p = Net::msg_new(Net::MessageType::Parkour);
		{
			Message m = Message::Pickup;
			serialize_enum(p, Message, m);
		}
		{
			Ref<Parkour> ref = parkour;
			serialize_ref(p, ref);
		}
		{
			Ref<Collectible> ref = collectible;
			serialize_ref(p, ref);
		}
		Net::msg_finalize(p);
		return true;
	}

	b8 sync_state(Parkour* parkour)
	{
		using Stream = Net::StreamWrite;
		Stream* p = Net::msg_new(Net::MessageType::Parkour);
		{
			Message m = Message::StateSync;
			serialize_enum(p, Message, m);
		}
		{
			Ref<Parkour> ref = parkour;
			serialize_ref(p, ref);
		}
		serialize_enum(p, Parkour::State, parkour->fsm.current);
		Net::msg_finalize(p);
		return true;
	}

	b8 kill(Parkour* parkour, MinionCommon* minion)
	{
		using Stream = Net::StreamWrite;
		Stream* p = Net::msg_new(Net::MessageType::Parkour);
		{
			Message m = Message::Kill;
			serialize_enum(p, Message, m);
		}
		{
			Ref<Parkour> ref = parkour;
			serialize_ref(p, ref);
		}
		{
			Ref<MinionCommon> ref = minion;
			serialize_ref(p, ref);
		}
		Net::msg_finalize(p);
		return true;
	}
}

b8 minion_in_front(Parkour* parkour, MinionCommon* minion, b8 forgiving)
{
	Vec3 minion_pos = minion->get<Walker>()->base_pos();
	Vec3 to_minion = minion_pos - parkour->get<Walker>()->base_pos();
	Vec3 forward = Quat::euler(0, parkour->get<Walker>()->target_rotation, 0) * Vec3(0, 0, 1);
	r32 forgiveness = forgiving ? 2.0f : 0.0f;
	return fabsf(to_minion.y) < WALKER_SUPPORT_HEIGHT + WALKER_DEFAULT_CAPSULE_HEIGHT + forgiveness
		&& forward.dot(to_minion) < WALKER_RADIUS * 2.5f + forgiveness
		&& (forgiving || forward.dot(Vec3::normalize(to_minion)) > 0.5f);
}

b8 Parkour::net_msg(Net::StreamRead* p, Net::MessageSource src)
{
	using Stream = Net::StreamRead;
	ParkourNet::Message type;
	serialize_enum(p, ParkourNet::Message, type);
	Ref<Parkour> parkour;
	serialize_ref(p, parkour);
	if (parkour.ref())
	{
		switch (type)
		{
			case ParkourNet::Message::Pickup:
			{
				Ref<Collectible> collectible;
				serialize_ref(p, collectible);
				if (collectible.ref()
					&& (parkour.ref()->get<Walker>()->base_pos() - collectible.ref()->get<Transform>()->absolute_pos()).length_squared() < (COLLECTIBLE_RADIUS * 2.0f) * (COLLECTIBLE_RADIUS * 2.0f)
					&& collectible.ref()->get<Transform>()->parent.ref() != parkour.ref()->get<Transform>())
				{
					Animator::Layer* layer3 = &parkour.ref()->get<Animator>()->layers[3];
					if (layer3->animation == AssetNull)
					{
						collectible.ref()->give_rewards();
						collectible.ref()->get<Transform>()->reparent(parkour.ref()->get<Transform>());
						layer3->set(Asset::Animation::character_pickup, 0.0f); // bypass animation blending
					}
				}
				break;
			}
			case ParkourNet::Message::StateSync:
			{
				State old_value = parkour.ref()->fsm.current;
				serialize_enum(p, State, parkour.ref()->fsm.current);
				if ((src == Net::MessageSource::Remote || Game::level.local) && old_value != parkour.ref()->fsm.current)
				{
					parkour.ref()->fsm.last = old_value;
					parkour.ref()->fsm.time = 0.0f;
				}
				break;
			}
			case ParkourNet::Message::Kill:
			{
				Ref<MinionCommon> minion;
				serialize_ref(p, minion);
				if (minion.ref()
					&& (src == Net::MessageSource::Remote || Game::level.local)
					&& (parkour.ref()->fsm.current == State::Slide || parkour.ref()->fsm.current == State::Roll)
					&& minion_in_front(parkour.ref(), minion.ref(), true))
				{
					minion.ref()->get<Health>()->kill(parkour.ref()->entity());
				}
				break;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}
	}
	return true;
}

void Parkour::update(const Update& u)
{
	fsm.time += u.time.delta;
	get<SkinnedModel>()->offset.make_transform(
		Vec3(0, get<Walker>()->capsule_height() * -0.5f - WALKER_SUPPORT_HEIGHT, 0),
		Vec3(1.0f, 1.0f, 1.0f),
		Quat::euler(0, get<Walker>()->rotation + PI * 0.5f, 0) * Quat::euler(0, 0, lean * -1.5f)
	);

	if (fsm.current == State::WallRun && fsm.time > JUMP_GRACE_PERIOD && get<Walker>()->support.ref())
	{
		wall_run_state = WallRunState::None;
		fsm.transition(State::Normal);
	}

	// animation layers
	// layer 0 = running, walking, wall-running
	// layer 1 = mantle, land, hard landing, jump, wall-jump, roll
	// layer 2 = slide

	r32 lean_target = 0.0f;

	r32 angular_velocity = LMath::angle_to(get<PlayerCommon>()->angle_horizontal, last_angle_horizontal);
	last_angle_horizontal = get<PlayerCommon>()->angle_horizontal;
	angular_velocity = (0.5f * angular_velocity) + (0.5f * last_angular_velocity); // smooth it out a bit
	last_angular_velocity = angular_velocity;

	if (fsm.current == State::Mantle)
	{
		get<Animator>()->layers[1].play(Asset::Animation::character_mantle);
		get<Animator>()->layers[2].animation = AssetNull;
		const r32 mantle_time = 0.5f;
		if (fsm.time > mantle_time || !last_support.ref())
		{
			fsm.transition(State::Normal);
			get<RigidBody>()->btBody->setLinearVelocity(Quat::euler(0, get<Walker>()->target_rotation, 0) * Vec3(0, 0, get<Walker>()->net_speed));
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
			get<Walker>()->absolute_pos(start + adjustment);
		}
		last_support_time = Game::time.total;
	}
	else if (fsm.current == State::HardLanding)
	{
		if (get<Animator>()->layers[1].animation != Asset::Animation::character_land_hard)
			fsm.transition(State::Normal);
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
			support_velocity = Walker::get_support_velocity(relative_support_pos, last_support.ref()->btBody);
		Vec3 velocity = get<RigidBody>()->btBody->getLinearVelocity();
		if (fsm.time > 0.2f || velocity.y - support_velocity.y < 0.0f)
		{
			btCollisionWorld::ClosestRayResultCallback support_callback = get<Walker>()->check_support();
			if (support_callback.hasHit())
				exit_wallrun = true;
		}

		if (!exit_wallrun)
		{
			// check if we need to transfer to a different wall
			{
				Vec3 ray_start = get<Walker>()->base_pos() + Vec3(0, WALKER_SUPPORT_HEIGHT + WALKER_DEFAULT_CAPSULE_HEIGHT * 0.25f, 0);

				Vec3 wall_run_normal = last_support.ref()->get<Transform>()->to_world_normal(relative_wall_run_normal);

				Quat rot = Quat::euler(0, get<Walker>()->target_rotation, 0);
				Vec3 forward = rot * Vec3(0, 0, 1);
				if (wall_run_state == WallRunState::Left || wall_run_state == WallRunState::Right)
				{
					// check if we need to switch to a perpendicular wall right in front of us
					if (!try_wall_run(WallRunState::Forward, forward))
					{
						// check if we need to transfer between walls that are slightly angled
						Vec3 ray_dir = rot * Vec3(wall_run_state == WallRunState::Left ? 1 : -1, 0, 1);
						Vec3 ray_end = ray_start + ray_dir * (WALKER_RADIUS * WALL_RUN_DISTANCE_RATIO);
						btCollisionWorld::ClosestRayResultCallback ray_callback(ray_start, ray_end);
						Physics::raycast(&ray_callback, CollisionParkour);

						r32 forward_dot = forward.dot(ray_callback.m_hitNormalWorld);
						if (ray_callback.hasHit()
							&& wall_run_normal.dot(ray_callback.m_hitNormalWorld) < 0.99f
							&& fabsf(ray_callback.m_hitNormalWorld.getY()) < 0.25f
							&& forward_dot > -0.9f
							&& forward_dot < 0.1f)
						{
							// transition from one wall to another
							last_support = Entity::list[ray_callback.m_collisionObject->getUserIndex()].get<RigidBody>();
							last_support_wall_run_state = wall_run_state;
							relative_support_pos = last_support.ref()->get<Transform>()->to_local(ray_callback.m_hitPointWorld);
							wall_run_normal = ray_callback.m_hitNormalWorld;
							relative_wall_run_normal = last_support.ref()->get<Transform>()->to_local_normal(wall_run_normal);
						}
					}
				}
			}

			// keep us glued to the wall

			Vec3 ray_start = get<Walker>()->base_pos() + Vec3(0, WALKER_SUPPORT_HEIGHT + WALKER_DEFAULT_CAPSULE_HEIGHT * 0.25f, 0);

			Vec3 wall_run_normal = last_support.ref()->get<Transform>()->to_world_normal(relative_wall_run_normal);

			Quat rot = Quat::euler(0, get<Walker>()->target_rotation, 0);
			Vec3 forward = rot * Vec3(0, 0, 1);

			Vec3 ray_end = ray_start + wall_run_normal * WALKER_RADIUS * WALL_RUN_DISTANCE_RATIO * -2.0f;
			btCollisionWorld::ClosestRayResultCallback ray_callback(ray_start, ray_end);
			Physics::raycast(&ray_callback, CollisionParkour);

			if (ray_callback.hasHit()
				&& wall_run_normal.dot(ray_callback.m_hitNormalWorld) > 0.5f
				&& fabsf(ray_callback.m_hitNormalWorld.getY()) < 0.25f
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
				exit_wallrun = true;
				if (wall_run_state == WallRunState::Forward)
					try_parkour(true); // do an extra broad raycast to make sure we hit the top if at all possible
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
		get<Walker>()->max_speed = MAX_SPEED;
		get<Walker>()->speed = RUN_SPEED;
		get<Animator>()->layers[2].animation = AssetNull;
		if (get<Walker>()->support.ref())
		{
			Animator::Layer* layer1 = &get<Animator>()->layers[1];
			if (layer1->animation == Asset::Animation::character_jump1)
				layer1->animation = AssetNull; // stop jump animations
			last_support_time = Game::time.total;
			last_support = get<Walker>()->support;
			last_support_wall_run_state = WallRunState::None;
			relative_support_pos = last_support.ref()->get<Transform>()->to_local(get<Walker>()->base_pos());
			lean_target = get<Walker>()->net_speed * angular_velocity * (0.75f / 180.0f) / u.time.delta;
		}
	}
	else if (fsm.current == State::Slide || fsm.current == State::Roll)
	{
		// check how fast we're going
		Vec3 support_velocity = Walker::get_support_velocity(relative_support_pos, last_support.ref() ? last_support.ref()->btBody : nullptr);
		Vec3 velocity = get<RigidBody>()->btBody->getLinearVelocity();
		Vec3 relative_velocity = velocity - support_velocity;
		Vec3 forward = Quat::euler(0, get<Walker>()->target_rotation, 0) * Vec3(0, 0, 1);
		b8 stop;
		if (fsm.current == State::Slide)
			stop = fsm.time > MIN_SLIDE_TIME && (!slide_continue || relative_velocity.dot(forward) < MIN_WALLRUN_SPEED);
		else // rolling
			stop = get<Animator>()->layers[1].animation != Asset::Animation::character_roll;

		if (stop) // going too slow, or button no longer held
			fsm.transition(State::Normal);
		else
		{
			// keep sliding/rolling
			if (fsm.current == State::Slide) // do damping
				relative_velocity -= Vec3::normalize(relative_velocity) * u.time.delta * 2.5f;

			// handle support
			{
				btCollisionWorld::ClosestRayResultCallback support_callback = get<Walker>()->check_support();
				// we still have support; update our position relative to it
				if (support_callback.hasHit())
				{
					last_support = Entity::list[support_callback.m_collisionObject->getUserIndex()].get<RigidBody>();
					last_support_wall_run_state = WallRunState::None;
					relative_support_pos = last_support.ref()->get<Transform>()->to_local(support_callback.m_hitPointWorld);
					relative_wall_run_normal = last_support.ref()->get<Transform>()->to_local_normal(support_callback.m_hitNormalWorld);
					last_support_time = Game::time.total;
				}

				Transform* last_support_transform = last_support.ref()->get<Transform>();

				Vec3 base_pos = get<Transform>()->absolute_pos();
				r32 base_offset = get<Walker>()->capsule_height() * 0.5f; // don't include support height
				base_pos.y -= base_offset;
				Vec3 absolute_support_pos = last_support_transform->to_world(relative_support_pos);

				if (support_callback.hasHit() || Game::time.total - last_support_time < 0.25f)
				{
					Vec3 support_normal = last_support_transform->to_world_normal(relative_wall_run_normal);
					Vec3 support_diff = base_pos - absolute_support_pos;
					r32 support_diff_normal = support_diff.dot(support_normal);
					Vec3 projected_support = base_pos - (support_diff_normal * support_normal);
					if (support_diff_normal < 0.0f)
					{
						get<Walker>()->absolute_pos(projected_support + Vec3(0, base_offset, 0));
						r32 velocity_normal = relative_velocity.dot(support_normal);
						relative_velocity -= support_normal * (velocity_normal * support_normal);
					}

					// update relative support pos
					relative_support_pos = last_support_transform->to_local(projected_support);
				}
				else
					fsm.transition(State::Normal);
			}

			get<RigidBody>()->btBody->setLinearVelocity(support_velocity + relative_velocity);

			// check for minions in front of us
			if (get<Walker>()->net_speed > MIN_ATTACK_SPEED)
			{
				for (auto i = MinionCommon::list.iterator(); !i.is_last(); i.next())
				{
					b8 already_damaged = false;
					for (s32 j = 0; j < damage_minions.length; j++)
					{
						if (i.item() == damage_minions[j].ref())
						{
							already_damaged = true;
							break;
						}
					}

					if (!already_damaged && minion_in_front(this, i.item(), false))
					{
						ParkourNet::kill(this, i.item());
						damage_minions.add(i.item());

						get<PlayerControlHuman>()->player.ref()->rumble_add(0.5f);

						Vec3 base_pos = get<Walker>()->base_pos();

						// sparks
						Vec3 p = base_pos + Vec3(0, (WALKER_SUPPORT_HEIGHT + WALKER_DEFAULT_CAPSULE_HEIGHT) * 0.5f, 0);
						Quat rot = Quat::look(i.item()->get<Walker>()->base_pos() - base_pos);
						for (s32 i = 0; i < 50; i++)
						{
							Particles::sparks.add
							(
								p,
								rot * Vec3(mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo()) * 10.0f,
								Vec4(1, 1, 1, 1)
							);
						}
					}
				}
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
			layer0->play(Asset::Animation::character_wall_run_left);
		else if (wall_run_state == WallRunState::Right)
			layer0->play(Asset::Animation::character_wall_run_right);
		else
		{
			// todo: wall run straight animation
			layer0->play(Asset::Animation::character_run);
		}
	}
	else if (get<Walker>()->support.ref())
	{
		if (get<Walker>()->dir.length_squared() > 0.0f)
		{
			// walking/running
			r32 net_speed = vi_max(get<Walker>()->net_speed, WALK_SPEED * 0.5f);
			layer0->speed = 0.9f * (net_speed > WALK_SPEED ? LMath::lerpf((net_speed - WALK_SPEED) / RUN_SPEED, 0.75f, 1.0f) : (net_speed / WALK_SPEED));
			AssetID new_anim = net_speed > WALK_SPEED ? Asset::Animation::character_run : Asset::Animation::character_walk;
			if (new_anim != layer0->animation)
			{
				r32 time = layer0->time;
				layer0->play(new_anim); // start animation at random position
				if (layer0->animation == Asset::Animation::character_run || layer0->animation == Asset::Animation::character_walk)
					layer0->time = time; // seamless transition
			}
		}
		else
		{
			// standing still
			layer0->play(Asset::Animation::character_idle);
			layer0->speed = 1.0f;
		}
	}
	else
	{
		// floating in space
		layer0->play(Asset::Animation::character_fall);
		layer0->speed = 1.0f;
	}

	{
		// update collision filter
		// don't collide with minions if we are sliding or rolling
		RigidBody* body = get<RigidBody>();
		b8 collide_with_minions = fsm.current != State::Roll && fsm.current != State::Slide;
		if (collide_with_minions && !(body->collision_filter & CollisionWalker))
			body->set_collision_masks(body->collision_group, body->collision_filter | CollisionWalker);
		else if (!collide_with_minions && (body->collision_filter & CollisionWalker))
			body->set_collision_masks(body->collision_group, body->collision_filter & ~CollisionWalker);
	}

	get<Walker>()->enabled = fsm.current == State::Normal || fsm.current == State::HardLanding;

	{
		// handle collectibles
		b8 pickup = fsm.current == State::Normal && get<Animator>()->layers[3].animation == AssetNull; // should we look for collectibles to pick up?
		Vec3 me = get<Walker>()->base_pos();
		for (auto i = Collectible::list.iterator(); !i.is_last(); i.next())
		{
			Transform* t = i.item()->get<Transform>();

			if (pickup && t->parent.ref() != get<Transform>() && (t->absolute_pos() - me).length_squared() < COLLECTIBLE_RADIUS * COLLECTIBLE_RADIUS)
				ParkourNet::pickup(this, i.item()); // pick it up

			if (t->parent.ref() == get<Transform>())
			{
				// glue it to our hand
				t->pos = Vec3(0.04f, 0, 0);
				t->rot = Quat::euler(0, PI * 0.5f, 0);
				get<Animator>()->to_local(Asset::Bone::character_hand_R, &t->pos, &t->rot);
				break;
			}
		}
	}

	if (!Game::level.local)
	{
		if (last_frame_state != fsm.current)
			ParkourNet::sync_state(this);
		last_frame_state = fsm.current;
	}
}

void Parkour::pickup_animation_complete()
{
	// delete whatever we're holding
	if (Game::level.local)
	{
		for (auto i = Collectible::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->get<Transform>()->parent.ref() == get<Transform>())
				World::remove_deferred(i.item()->entity());
		}
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
	Vec3 new_velocity = body->getLinearVelocity();
	new_velocity.y = vi_max(0.0f, new_velocity.y) + JUMP_SPEED;
	body->setLinearVelocity(new_velocity);
	last_support = get<Walker>()->support = nullptr;
	last_support_wall_run_state = WallRunState::None;
	last_support_time = Game::time.total;
	wall_run_state = WallRunState::None;
	get<Animator>()->layers[1].play(Asset::Animation::character_jump1);
}

void Parkour::lessen_gravity()
{
	// slightly increase vertical velocity
	// this results in higher jumps when the player holds the jump button
	btRigidBody* body = get<RigidBody>()->btBody;
	if (body->getLinearVelocity().y() > 0.0f)
		body->applyCentralForce(Physics::btWorld->getGravity() * -0.2f);
}

b8 Parkour::try_jump(r32 rotation)
{
	b8 did_jump = false;
	if (fsm.current == State::Normal || fsm.current == State::Slide || fsm.current == State::WallRun)
	{
		if ((get<Walker>()->support.ref() && get<Walker>()->support.ref()->btBody->getBroadphaseProxy()->m_collisionFilterGroup & CollisionParkour)
			|| fsm.current == State::Slide
			|| (
				last_support_wall_run_state == WallRunState::None
				&& Game::time.total - last_support_time < JUMP_GRACE_PERIOD
				&& last_support.ref() && last_support.ref()->btBody->getBroadphaseProxy()->m_collisionFilterGroup & CollisionParkour
				))
		{
			do_normal_jump();
			did_jump = true;
		}
		else
		{
			// try wall-jumping
			if (last_support.ref() && Game::time.total - last_support_time < JUMP_GRACE_PERIOD && last_support_wall_run_state != WallRunState::None)
			{
				wall_jump(rotation, last_support.ref()->get<Transform>()->to_world_normal(relative_wall_run_normal), last_support.ref()->btBody);
				did_jump = true;
			}
			else
			{
				Vec3 ray_start = get<Walker>()->base_pos() + Vec3(0, WALKER_SUPPORT_HEIGHT, 0);

				for (s32 i = 0; i < wall_jump_direction_count; i++)
				{
					Vec3 ray_end = ray_start + wall_directions[i] * WALKER_RADIUS * WALL_JUMP_RAYCAST_RADIUS_RATIO;
					btCollisionWorld::ClosestRayResultCallback ray_callback(ray_start, ray_end);
					Physics::raycast(&ray_callback, CollisionParkour);

					if (ray_callback.hasHit())
					{
						Vec3 wall_normal = ray_callback.m_hitNormalWorld;
						const btRigidBody* support_body = dynamic_cast<const btRigidBody*>(ray_callback.m_collisionObject);
						wall_jump(rotation, wall_normal, support_body);
						did_jump = true;
						break;
					}
				}
			}
		}

		if (did_jump)
		{
			get<Audio>()->post_event(has<PlayerControlHuman>() ? AK::EVENTS::PLAY_JUMP_PLAYER : AK::EVENTS::PLAY_JUMP);
			fsm.transition(State::Normal);
		}
	}

	return did_jump;
}

void Parkour::wall_jump(r32 rotation, const Vec3& wall_normal, const btRigidBody* support_body)
{
	Vec3 pos = get<Walker>()->base_pos();
	Vec3 support_velocity = Walker::get_support_velocity(pos, support_body);

	RigidBody* body = get<RigidBody>();

	Vec3 velocity = body->btBody->getLinearVelocity();
	velocity.y = 0.0f;

	r32 velocity_length = LMath::clampf((velocity - support_velocity).length(), MIN_WALLRUN_SPEED, get<Walker>()->max_speed);
	Vec3 velocity_reflected = Quat::euler(0, rotation, 0) * Vec3(0, 0, velocity_length);

	if (velocity_reflected.dot(wall_normal) < 0.0f)
		velocity_reflected = velocity_reflected.reflect(wall_normal);

	Vec3 new_velocity = velocity_reflected + (wall_normal * velocity_length * 0.5f);
	new_velocity.y = 0.0f;
	new_velocity *= velocity_length / new_velocity.length();
	new_velocity.y = velocity_length;
	body->btBody->setLinearVelocity(new_velocity);

	// Update our last supported speed so that air control will allow us to go the new speed
	last_support = nullptr;
	last_support_wall_run_state = WallRunState::None;
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
	if (fsm.current == State::Normal)
	{
		btCollisionWorld::ClosestRayResultCallback support_callback = get<Walker>()->check_support(get<Walker>()->capsule_height() * 1.5f);
		if (support_callback.hasHit())
		{
			Vec3 velocity = get<RigidBody>()->btBody->getLinearVelocity();
			Vec3 forward = Quat::euler(0, get<Walker>()->rotation, 0) * Vec3(0, 0, 1);
			Vec3 support_velocity = Walker::get_support_velocity(support_callback.m_hitPointWorld, support_callback.m_collisionObject);
			Vec3 relative_velocity = velocity - support_velocity;

			if (get<Walker>()->support.ref())
			{
				// already on ground
				if (get<Walker>()->net_speed < MIN_WALLRUN_SPEED)
					return false; // too slow
				fsm.transition(State::Slide);
				get<Animator>()->layers[2].play(Asset::Animation::character_slide);
			}
			else
			{
				if (relative_velocity.y > 1.0f)
					return false; // need to be going down
				fsm.transition(State::Roll);
				get<Animator>()->layers[1].play(Asset::Animation::character_roll);
			}
			velocity = support_velocity + (forward * (get<Walker>()->net_speed + 3.0f));
			get<Walker>()->enabled = false;

			get<RigidBody>()->btBody->setLinearVelocity(velocity);

			last_support = Entity::list[support_callback.m_collisionObject->getUserIndex()].get<RigidBody>();
			last_support_wall_run_state = WallRunState::None;
			relative_support_pos = last_support.ref()->get<Transform>()->to_local(support_callback.m_hitPointWorld);
			relative_wall_run_normal = last_support.ref()->get<Transform>()->to_local_normal(support_callback.m_hitNormalWorld);
			last_support_time = Game::time.total;

			slide_continue = true;

			damage_minions.length = 0;
			return true;
		}
	}
	return false;
}

// If force is true, we'll raycast farther downward when trying to mantle, to make sure we find something.
b8 Parkour::try_parkour(b8 force)
{
	Quat rot = Quat::euler(0, get<Walker>()->rotation, 0);

	if (fsm.current == State::Normal)
	{
		if (try_wall_run(WallRunState::Forward, rot * Vec3(0, 0, 1)))
			return true;
		if (try_wall_run(WallRunState::Left, rot * Vec3(1, 0, 0)))
			return true;
		if (try_wall_run(WallRunState::Right, rot * Vec3(-1, 0, 0)))
			return true;
	}

	if (fsm.current == State::Normal || fsm.current == State::WallRun)
	{
		// Try to mantle
		Vec3 pos = get<Transform>()->absolute_pos();
		Walker* walker = get<Walker>();

		for (s32 i = 0; i < mantle_sample_count; i++)
		{
			Vec3 dir_offset = rot * (mantle_samples[i] * WALKER_RADIUS * RAYCAST_RADIUS_RATIO);

			Vec3 ray_start = pos + Vec3(dir_offset.x, WALKER_DEFAULT_CAPSULE_HEIGHT * 0.7f, dir_offset.z);
			Vec3 ray_end = pos + Vec3(dir_offset.x, WALKER_DEFAULT_CAPSULE_HEIGHT * -0.25f + (force ? -WALKER_SUPPORT_HEIGHT - 0.5f : 0.0f), dir_offset.z);

			btCollisionWorld::ClosestRayResultCallback ray_callback(ray_start, ray_end);
			Physics::raycast(&ray_callback, CollisionParkour);

			if (ray_callback.hasHit() && ray_callback.m_hitNormalWorld.getY() > 0.25f)
			{
				// check for wall blocking the mantle
				{
					Vec3 wall_ray_start = pos;
					pos.y = ray_callback.m_hitPointWorld.getY() + 0.1f;
					Vec3 wall_ray_end = ray_callback.m_hitPointWorld;
					wall_ray_end.y += 0.1f;
					btCollisionWorld::ClosestRayResultCallback wall_ray_callback(wall_ray_start, wall_ray_end);
					Physics::raycast(&wall_ray_callback, ~CollisionAwkIgnore);
					if (wall_ray_callback.hasHit())
						return false;
				}

				get<Animator>()->layers[1].play(Asset::Animation::character_mantle);
				fsm.transition(State::Mantle);
				last_support = Entity::list[ray_callback.m_collisionObject->getUserIndex()].get<RigidBody>();
				last_support_wall_run_state = WallRunState::None;
				relative_support_pos = last_support.ref()->get<Transform>()->to_local(ray_callback.m_hitPointWorld + Vec3(0, WALKER_SUPPORT_HEIGHT + WALKER_DEFAULT_CAPSULE_HEIGHT * 0.6f, 0));
				last_support_time = Game::time.total;

				get<RigidBody>()->btBody->setLinearVelocity(Vec3::zero);

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
	r32 speed = LMath::clampf(horizontal_velocity.length(), 0.0f, MAX_SPEED);
	get<RigidBody>()->btBody->setLinearVelocity(support_velocity + Vec3(0, (RUN_SPEED * 0.5f) + speed + vertical_velocity - support_velocity.y, 0));
}

b8 Parkour::try_wall_run(WallRunState s, const Vec3& wall_direction)
{
	Vec3 ray_start = get<Walker>()->base_pos() + Vec3(0, WALKER_SUPPORT_HEIGHT + WALKER_DEFAULT_CAPSULE_HEIGHT * 0.25f, 0);
	Vec3 ray_end = ray_start + wall_direction * WALKER_RADIUS * WALL_RUN_DISTANCE_RATIO * 2.0f;
	btCollisionWorld::ClosestRayResultCallback ray_callback(ray_start, ray_end);
	Physics::raycast(&ray_callback, CollisionParkour);

	if (ray_callback.hasHit()
		&& fabsf(ray_callback.m_hitNormalWorld.getY()) < 0.25f
		&& wall_direction.dot(ray_callback.m_hitNormalWorld) < -0.8f)
	{
		btRigidBody* body = get<RigidBody>()->btBody;

		Vec3 velocity = body->getLinearVelocity();
		r32 vertical_velocity = velocity.y;
		velocity.y = 0.0f;

		Vec3 wall_normal = ray_callback.m_hitNormalWorld;

		const btRigidBody* support_body = dynamic_cast<const btRigidBody*>(ray_callback.m_collisionObject);
		Vec3 support_velocity = Walker::get_support_velocity(ray_callback.m_hitPointWorld, support_body);

		// if we are running on a new wall, we need to add velocity
		// if it's the same wall we were running on before, we should not add any velocity
		// this prevents the player from spamming the wall-run key to wall-run infinitely
		b8 add_velocity = !last_support.ref()
			|| last_support.ref()->entity_id != support_body->getUserIndex()
			|| last_support_wall_run_state != s
			|| wall_run_state != s;

		if (s == WallRunState::Forward)
		{
			if (add_velocity && vertical_velocity - support_velocity.y > -4.0f)
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
			Vec3 forward = Quat::euler(0, get<Walker>()->rotation, 0) * Vec3(0, 0, 1);
			if (velocity_flattened.dot(forward) < 0.0f)
				return false;

			r32 speed = get<Walker>()->net_speed;
			if (speed < MIN_WALLRUN_SPEED + 1.0f)
				return false;

			velocity_flattened *= (speed * 1.1f) / velocity_flattened.length();

			if (add_velocity)
				velocity_flattened.y = vi_max(flattened_vertical_speed, 0.0f) + 3.0f + speed * 0.5f;
			else
				velocity_flattened.y = flattened_vertical_speed;
			body->setLinearVelocity(velocity_flattened);
		}

		wall_run_state = s;
		last_support = Entity::list[ray_callback.m_collisionObject->getUserIndex()].get<RigidBody>();
		last_support_wall_run_state = s;
		relative_support_pos = last_support.ref()->get<Transform>()->to_local(ray_callback.m_hitPointWorld);
		relative_wall_run_normal = last_support.ref()->get<Transform>()->to_local_normal(wall_normal);
		fsm.transition(State::WallRun);

		Plane p(wall_normal, ray_callback.m_hitPointWorld);

		Vec3 pos = get<Transform>()->absolute_pos();
		get<Walker>()->absolute_pos(pos + wall_normal * (-p.distance(pos) + (WALKER_RADIUS * WALL_RUN_DISTANCE_RATIO)));

		{
			Vec3 forward;
			if (wall_run_state == WallRunState::Forward)
			{
				forward = -wall_normal;
				forward.normalize();
			}
			else
			{
				forward = body->getLinearVelocity();
				forward.y = 0.0f;
				forward.normalize();
			}
			get<Walker>()->target_rotation = atan2(forward.x, forward.z);
		}

		return true;
	}

	return false;
}


}
