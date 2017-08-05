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
#include "drone.h"
#include "minion.h"
#include "render/particles.h"
#include "mersenne/mersenne-twister.h"
#include "team.h"
#include "data/components.h"
#include "player.h"
#include "render/skinned_model.h"
#include "net.h"
#include "ease.h"

namespace VI
{

#define RAYCAST_RADIUS_RATIO 1.3f
#define WALL_JUMP_RAYCAST_RADIUS_RATIO 2.0f
#define WALL_RUN_DISTANCE_RATIO 1.1f

#define RUN_SPEED 5.0f
#define WALK_SPEED 3.0f
#define MAX_SPEED 6.0f
#define MIN_WALLRUN_SPEED 3.0f
#define MIN_ATTACK_SPEED 4.0f
#define JUMP_SPEED 5.5f
#define COLLECTIBLE_RADIUS 1.0f

#define JUMP_GRACE_PERIOD 0.3f

#define TILE_CREATE_RADIUS 4.5f

#define MIN_SLIDE_TIME 0.6f
#define ANIMATION_SPEED_MULTIPLIER 2.0f

Traceur::Traceur(const Vec3& pos, r32 rot, AI::Team team)
{
	Transform* transform = create<Transform>();
	transform->pos = pos;

	Animator* animator = create<Animator>();
	SkinnedModel* model = create<SkinnedModel>();

	animator->armature = Asset::Armature::character;

	model->shader = Asset::Shader::armature;
	model->mesh_shadow = Asset::Mesh::parkour;
	model->mesh = Asset::Mesh::parkour_headless;
	model->radius = 20.0f;

	create<Audio>();
	
	Walker* walker = create<Walker>(rot);
	walker->max_speed = MAX_SPEED;
	walker->speed = RUN_SPEED;
	walker->auto_rotate = false;

	create<AIAgent>()->team = team;

	create<Target>();
	create<Health>(DRONE_HEALTH, DRONE_HEALTH, PARKOUR_SHIELD, PARKOUR_SHIELD);

	create<Parkour>();
}

void Parkour::awake()
{
	get<RigidBody>()->set_ccd(true);
	Animator* animator = get<Animator>();
	animator->layers[0].behavior = Animator::Behavior::Loop;
	animator->layers[0].play(Asset::Animation::character_idle);
	animator->layers[1].blend_time = 0.2f;
	link<&Parkour::climb_sound>(animator->trigger(Asset::Animation::character_climb_down, 0.0f));
	link<&Parkour::climb_sound>(animator->trigger(Asset::Animation::character_climb_up, 0.0f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_walk, 0.0f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_walk, 0.5f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_walk_left, 0.0f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_walk_left, 0.5f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_walk_right, 0.0f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_walk_right, 0.5f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_walk_backward, 0.0f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_walk_backward, 0.5f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_run, 0.5f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_run, 0.0f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_run_left, 0.5f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_run_left, 0.0f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_run_right, 0.5f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_run_right, 0.0f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_run_backward, 0.5f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_run_backward, 0.0f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_wall_run_left, 0.5f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_wall_run_left, 0.0f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_wall_run_right, 0.5f));
	link<&Parkour::footstep>(animator->trigger(Asset::Animation::character_wall_run_right, 0.0f));
	link<&Parkour::pickup_animation_complete>(animator->trigger(Asset::Animation::character_pickup, 2.5f));
	link_arg<r32, &Parkour::land>(get<Walker>()->land);
	link_arg<Entity*, &Parkour::killed>(get<Health>()->killed);
	last_angle_horizontal = get<Walker>()->rotation;
}

Parkour::~Parkour()
{
	pickup_animation_complete(); // delete anything we're holding
}

namespace ParkourNet
{
	enum Message
	{
		Pickup,
		StateSync,
		Electrocuted,
		FallDamage,
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

	b8 electrocuted(Parkour* parkour)
	{
		using Stream = Net::StreamWrite;
		Stream* p = Net::msg_new(Net::MessageType::Parkour);
		{
			Message m = Message::Electrocuted;
			serialize_enum(p, Message, m);
		}
		{
			Ref<Parkour> ref = parkour;
			serialize_ref(p, ref);
		}
		Net::msg_finalize(p);
		return true;
	}

	b8 fall_damage(Parkour* parkour, s8 damage)
	{
		using Stream = Net::StreamWrite;
		Stream* p = Net::msg_new(Net::MessageType::Parkour);
		{
			Message m = Message::FallDamage;
			serialize_enum(p, Message, m);
		}
		{
			Ref<Parkour> ref = parkour;
			serialize_ref(p, ref);
		}
		serialize_int(p, s8, damage, 0, DRONE_HEALTH + PARKOUR_SHIELD);
		Net::msg_finalize(p);
		return true;
	}
}

void Parkour::killed(Entity*)
{
	if (Game::level.local)
		World::remove_deferred(entity());
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
				get<Walker>()->max_speed = get<Walker>()->speed = get<Walker>()->net_speed = 0.0f;
				get<RigidBody>()->btBody->setLinearVelocity(Vec3(0, get<RigidBody>()->btBody->getLinearVelocity().getY(), 0));
				get<Animator>()->layers[1].play(Asset::Animation::character_land_hard);
				get<Audio>()->post_event(AK::EVENTS::PLAY_PARKOUR_LAND_HARD);
				s8 damage = vi_min(s8((LANDING_VELOCITY_HARD - velocity_diff) * 0.5f), s8(DRONE_HEALTH + PARKOUR_SHIELD));
				if (damage > 0)
					ParkourNet::fall_damage(this, damage);
			}
			else // light landing
			{
				get<Animator>()->layers[1].play(Asset::Animation::character_land);
				get<Audio>()->post_event(AK::EVENTS::PLAY_PARKOUR_LAND_SOFT);
			}
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
		EffectLight::add(base_pos, 1.0f, 5.0f, EffectLight::Type::Shockwave, support ? support->get<Transform>() : nullptr);
	}
}

void Parkour::climb_sound()
{
	if (fsm.current == State::Climb)
		get<Audio>()->post_event(AK::EVENTS::PLAY_PARKOUR_CLIMB);
}

enum class ParkourHand
{
	Left,
	Right,
	Both,
	count,
};

void parkour_sparks(Parkour* parkour, const Update& u, ParkourHand hand, r32 amount)
{
	static r32 particle_accumulator = 0.0f;
	static const AssetID claws[6] =
	{
		Asset::Bone::character_claw1_L,
		Asset::Bone::character_claw2_L,
		Asset::Bone::character_claw3_L,
		Asset::Bone::character_claw1_R,
		Asset::Bone::character_claw2_R,
		Asset::Bone::character_claw3_R,
	};
	s32 claw_offset;
	s32 claw_count;
	Quat rot;
	switch (hand)
	{
		case ParkourHand::Left:
		{
			claw_offset = 0;
			claw_count = 3;
			rot = Quat::euler(0, parkour->get<Walker>()->rotation + PI * -0.5f, 0);
			break;
		}
		case ParkourHand::Right:
		{
			claw_offset = 3;
			claw_count = 3;
			rot = Quat::euler(0, parkour->get<Walker>()->rotation + PI * 0.5f, 0);
			break;
		}
		case ParkourHand::Both:
		{
			claw_offset = 0;
			claw_count = 6;
			rot = Quat::euler(0, parkour->get<Walker>()->rotation + PI, 0);
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}

	particle_accumulator += u.time.delta;
	const r32 interval = 0.025f / vi_max(0.005f, claw_count * amount); // emit more particles when more claws are active
	while (particle_accumulator > interval)
	{
		Vec3 p(0.085f, 0.0f, 0.15f);
		parkour->get<Animator>()->to_world(claws[claw_offset + mersenne::rand() % claw_count], &p);

		Particles::sparks_small.add
		(
			p,
			rot * Vec3(mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo()) * 2.0f,
			Vec4(1, 1, 1, 1)
		);

		if (mersenne::randf_cc() < 0.15f)
			EffectLight::add(p, 1.0f, 0.15f, EffectLight::Type::Spark, nullptr);

		particle_accumulator -= interval;
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

	Vec3 support_velocity = Walker::get_support_velocity(absolute_wall_pos, wall->btBody);

	Vec3 horizontal_velocity_diff = velocity - support_velocity;
	r32 vertical_velocity_diff = horizontal_velocity_diff.y;
	horizontal_velocity_diff.y = 0.0f;
	if (wall_run_state != WallRunState::Forward && horizontal_velocity_diff.length() < MIN_WALLRUN_SPEED)
		exit_wallrun = true; // we're going too slow
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

		// face the correct direction
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

		// animation stuff
		{
			Animator::Layer* layer0 = &get<Animator>()->layers[0];
			if (wall_run_state == WallRunState::Forward)
			{
				r32 amount = ANIMATION_SPEED_MULTIPLIER * (vertical_velocity_diff / get<Walker>()->speed);
				layer0->speed = vi_max(0.0f, amount);
				amount = vi_min(fabsf(amount), 1.0f);
				if (amount > 0.25f)
					parkour_sparks(this, u, ParkourHand::Both, amount);
				if (layer0->speed > 0.0f)
					layer0->play(Asset::Animation::character_wall_run_straight);
				else
					layer0->play(Asset::Animation::character_wall_slide); // going down
			}
			else
			{
				layer0->speed = ANIMATION_SPEED_MULTIPLIER * (horizontal_velocity_diff.length() / get<Walker>()->speed);
				if (wall_run_state == WallRunState::Left)
				{
					layer0->play(Asset::Animation::character_wall_run_left);
					parkour_sparks(this, u, ParkourHand::Left, layer0->speed);
				}
				else if (wall_run_state == WallRunState::Right)
				{
					layer0->play(Asset::Animation::character_wall_run_right);
					parkour_sparks(this, u, ParkourHand::Right, layer0->speed);
				}
			}
		}

		// try to climb stuff while we're wall-running
		if (try_parkour(MantleAttempt::Extra))
			exit_wallrun = true;
		else
		{
			relative_support_pos = last_support.ref()->get<Transform>()->to_local(get<Walker>()->base_pos() + absolute_wall_normal * -wall_distance);
			last_support_time = Game::time.total;
		}
	}

	// spawn tiles
	if (!exit_wallrun && wall_run_state != WallRunState::Forward && Game::save.extended_parkour)
	{
		Vec3 relative_wall_right = relative_wall_run_normal.cross(last_support.ref()->get<Transform>()->to_local_normal(Vec3(0, 1, 0)));
		relative_wall_right.normalize();
		Vec3 relative_wall_up = relative_wall_right.cross(relative_wall_run_normal);
		relative_wall_up.normalize();

		Vec3 spawn_offset = ((velocity - support_velocity) * 1.5f) + (absolute_wall_normal * -3.0f);
		spawn_tiles(relative_wall_right, relative_wall_up, relative_wall_run_normal, spawn_offset);
	}

	return exit_wallrun;
}

void parkour_set_collectible_position(Animator* parkour, Transform* collectible)
{
	collectible->pos = Vec3(0.04f, 0, 0);
	collectible->rot = Quat::euler(0, PI * 0.5f, 0);
	parkour->to_local(Asset::Bone::character_hand_R, &collectible->pos, &collectible->rot);
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
						collectible.ref()->get<Transform>()->parent = parkour.ref()->get<Transform>();
						layer3->set(Asset::Animation::character_pickup, 0.0f); // bypass animation blending
						parkour.ref()->get<Animator>()->update_world_transforms();
						parkour.ref()->get<Audio>()->post_event(AK::EVENTS::PLAY_PARKOUR_COLLECTIBLE_PICKUP);
						parkour_set_collectible_position(parkour.ref()->get<Animator>(), collectible.ref()->get<Transform>());
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
			case ParkourNet::Message::Electrocuted:
			{
				spawn_sparks(parkour.ref()->get<Walker>()->base_pos(), Quat::look(Vec3(0, 1, 0)));
				if (Game::level.local)
					parkour.ref()->get<Health>()->kill(nullptr);
				break;
			}
			case ParkourNet::Message::FallDamage:
			{
				s8 damage;
				serialize_int(p, s8, damage, 0, DRONE_HEALTH + PARKOUR_SHIELD);
				if (Game::level.local)
					parkour.ref()->get<Health>()->damage(nullptr, damage);
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

void parkour_stop_climbing(Parkour* parkour)
{
	parkour->fsm.transition(Parkour::State::Normal);
	parkour->rope = nullptr;
	parkour->last_support = nullptr;
	parkour->last_support_wall_run_state = Parkour::WallRunState::None;
	parkour->last_support_time = Game::time.total;
	RigidBody::remove_constraint(parkour->rope_constraint);
	parkour->rope_constraint = IDNull;
}

enum class ParkourRopeSearch : s8
{
	Any,
	Above,
	Below,
	count,
};

Vec3 parkour_climb_offset(Parkour* parkour)
{
	return Quat::euler(0, parkour->get<Walker>()->target_rotation, 0) * Vec3(0, 0.8f, 0.27f);
}

Transform* parkour_get_rope(Parkour* parkour, ParkourRopeSearch search)
{
	Vec3 climb_attach_point = parkour->get<Transform>()->absolute_pos() + parkour_climb_offset(parkour);
	for (auto i = Rope::list.iterator(); !i.is_last(); i.next())
	{
		Vec3 diff = i.item()->get<Transform>()->absolute_pos() - climb_attach_point;
		r32 distance_sq = diff.length_squared();
		switch (search)
		{
			case ParkourRopeSearch::Any:
			{
				if (distance_sq < ROPE_SEGMENT_LENGTH * ROPE_SEGMENT_LENGTH)
					return i.item()->get<Transform>();
				break;
			}
			case ParkourRopeSearch::Above:
			{
				if (distance_sq < ROPE_SEGMENT_LENGTH * 0.75f * ROPE_SEGMENT_LENGTH * 0.75f
					&& diff.y > ROPE_SEGMENT_LENGTH * 0.25f)
					return i.item()->get<Transform>();
				break;
			}
			case ParkourRopeSearch::Below:
			{
				if (distance_sq < ROPE_SEGMENT_LENGTH * 0.75f * ROPE_SEGMENT_LENGTH * 0.75f
					&& diff.y < ROPE_SEGMENT_LENGTH * -0.25f)
					return i.item()->get<Transform>();
				break;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}
	}
	return nullptr;
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
	fsm.time += u.time.delta;

	if (fsm.current == State::WallRun && fsm.time > JUMP_GRACE_PERIOD && get<Walker>()->support.ref())
	{
		wall_run_state = WallRunState::None;
		fsm.transition(State::Normal);
	}

	// animation layers
	// layer 0 = running, walking, wall-running
	// layer 1 = mantle, land, hard landing, jump, wall-jump

	r32 lean_target = 0.0f;

	r32 angular_velocity = LMath::angle_to(get<PlayerCommon>()->angle_horizontal, last_angle_horizontal);
	last_angle_horizontal = get<PlayerCommon>()->angle_horizontal;
	angular_velocity = (0.5f * angular_velocity) + (0.5f * last_angular_velocity); // smooth it out a bit
	last_angular_velocity = angular_velocity;

	{
		Tram* tram = Tram::player_inside(entity());
		if (tram)
		{
			// HACK to prevent player from falling out of tram
			Vec3 tram_pos;
			Quat tram_rot;
			tram->get<Transform>()->absolute(&tram_pos, &tram_rot);
			Vec3 pos = get<Transform>()->absolute_pos();
			Vec3 forward = tram_rot * Vec3(0, 0, 1);
			r32 z = (pos - tram_pos).dot(forward);
			if (z < -5.0f)
				get<Walker>()->absolute_pos(pos + forward * (-5.0f - z));
		}
	}

	if (fsm.current == State::Mantle)
	{
		// top-out animation is slower than mantle animation
		const r32 mantle_time = get<Animator>()->layers[1].animation == Asset::Animation::character_mantle ? 0.4f : 1.2f;

		if (fsm.time > mantle_time || !last_support.ref())
		{
			// done
			fsm.transition(State::Normal);
			get<RigidBody>()->btBody->setLinearVelocity(Quat::euler(0, get<Walker>()->target_rotation, 0) * Vec3(0, 0, get<Walker>()->net_speed));
		}
		else
		{
			// still going

			if (get<Animator>()->layers[1].animation == Asset::Animation::character_top_out)
			{
				// bring camera back level if we're looking up
				if (get<PlayerCommon>()->angle_vertical < 0.0f)
					get<PlayerCommon>()->angle_vertical = vi_min(0.0f, get<PlayerCommon>()->angle_vertical + u.time.delta);
			}

			Vec3 start = get<Transform>()->absolute_pos();
			Vec3 end = last_support.ref()->get<Transform>()->to_world(relative_support_pos);

			Vec3 diff = end - start;
			r32 blend = fsm.time / mantle_time;
			Vec3 adjustment;
			if (blend < 0.5f)
			{
				// move vertically
				r32 distance = diff.y;
				r32 time_left = (mantle_time * 0.5f) - fsm.time;
				adjustment = Vec3(0, vi_min(distance, u.time.delta / time_left), 0);
			}
			else
			{
				// move horizontally
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
		b8 exit_wallrun = false;

		// under certain circumstances, check if we have ground beneath us
		// if so, stop wall-running
		Vec3 support_velocity = Vec3::zero;
		if (last_support.ref())
			support_velocity = Walker::get_support_velocity(relative_support_pos, last_support.ref()->btBody);
		Vec3 velocity = get<RigidBody>()->btBody->getLinearVelocity();
		if (fsm.time > JUMP_GRACE_PERIOD || velocity.y - support_velocity.y < 0.0f)
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
				// still on the wall
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
					try_parkour(MantleAttempt::Force); // do an extra broad raycast to make sure we hit the top if at all possible
				}
				else if (Game::save.extended_parkour) // keep going, generate a wall
					exit_wallrun = wallrun(u, last_support.ref(), relative_support_pos, relative_wall_run_normal);
				else
					exit_wallrun = true;
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
		if (get<Walker>()->support.ref())
		{
			if (get<Walker>()->support.ref()->get<RigidBody>()->collision_group & CollisionElectric)
				ParkourNet::electrocuted(this);

			can_double_jump = true;
			tile_history.length = 0;

			Animator::Layer* layer1 = &get<Animator>()->layers[1];
			if (layer1->animation == Asset::Animation::character_jump1)
				layer1->animation = AssetNull; // stop jump animations
			last_support_time = Game::time.total;
			last_support = get<Walker>()->support;
			last_support_wall_run_state = WallRunState::None;
			relative_support_pos = last_support.ref()->get<Transform>()->to_local(get<Walker>()->base_pos());
			lean_target = get<Walker>()->net_speed * angular_velocity * (0.75f / 180.0f) / vi_max(0.0001f, u.time.delta);
		}
	}

	// update animation

	lean += (lean_target - lean) * vi_min(1.0f, 20.0f * u.time.delta);

	Animator::Layer* layer0 = &get<Animator>()->layers[0];
	if (fsm.current == State::WallRun)
	{
		// animation already handled by wallrun()
	}
	else if (fsm.current == State::Climb)
	{
		get<Animator>()->layers[1].animation = AssetNull;
		if (climb_velocity == 0.0f)
			layer0->play(Asset::Animation::character_hang);
		else
		{
			AssetID anim = climb_velocity > 0.0f ? Asset::Animation::character_climb_up : Asset::Animation::character_climb_down;
			if (layer0->animation != anim)
			{
				layer0->animation = anim;
				layer0->time = 0.0f;
			}
			layer0->speed = fabsf(climb_velocity);
		}
	}
	else if (get<Walker>()->support.ref())
	{
		if (get<Walker>()->dir.length_squared() > 0.0f)
		{
			// walking/running

			// set animation speed
			r32 net_speed = vi_max(get<Walker>()->net_speed, WALK_SPEED * 0.5f);
			layer0->speed = ANIMATION_SPEED_MULTIPLIER * (net_speed > WALK_SPEED ? LMath::lerpf((net_speed - WALK_SPEED) / RUN_SPEED, 0.75f, 1.0f) : (net_speed / WALK_SPEED));

			// choose animation
			r32 angle = LMath::angle_range(LMath::angle_to(get<Walker>()->target_rotation, atan2f(get<Walker>()->dir.x, get<Walker>()->dir.y)));
			s32 dir = s32(roundf(angle / (PI * 0.5f)));
			if (dir < 0)
				dir += 4;
			AssetID new_anim;
			if (net_speed > WALK_SPEED)
			{
				const AssetID animations[] =
				{
					Asset::Animation::character_run,
					Asset::Animation::character_run_left,
					Asset::Animation::character_run_backward,
					Asset::Animation::character_run_right,
				};
				new_anim = animations[dir];
			}
			else
			{
				const AssetID animations[] =
				{
					Asset::Animation::character_walk,
					Asset::Animation::character_walk_left,
					Asset::Animation::character_walk_backward,
					Asset::Animation::character_walk_right,
				};
				new_anim = animations[dir];
			}

			if (new_anim != layer0->animation)
			{
				r32 time = layer0->time;
				layer0->play(new_anim); // start animation at random position
				if (layer0->animation == Asset::Animation::character_run
					|| layer0->animation == Asset::Animation::character_run_left
					|| layer0->animation == Asset::Animation::character_run_right
					|| layer0->animation == Asset::Animation::character_run_backward
					|| layer0->animation == Asset::Animation::character_walk
					|| layer0->animation == Asset::Animation::character_walk_left
					|| layer0->animation == Asset::Animation::character_walk_right
					|| layer0->animation == Asset::Animation::character_walk_backward)
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

	get<Walker>()->enabled = fsm.current == State::Normal || fsm.current == State::HardLanding;

	{
		if (fsm.current == State::Normal && Game::time.total - last_support_time > JUMP_GRACE_PERIOD * 2.0f)
		{
			// check for stuff to climb
			Transform* r = parkour_get_rope(this, ParkourRopeSearch::Any);
			if (r)
			{
				fsm.transition(State::Climb);
				rope = r;
				Vec3 v = get<RigidBody>()->btBody->getLinearVelocity();
				v.y = 0.0f;
				RigidBody* rope_body = rope.ref()->get<RigidBody>();
				rope_body->activate_linked();
				rope_body->btBody->setLinearVelocity(v);

				Vec3 climb_offset = parkour_climb_offset(this);

				get<Walker>()->absolute_pos(rope.ref()->absolute_pos() - climb_offset);

				RigidBody::Constraint constraint = RigidBody::Constraint();
				constraint.type = RigidBody::Constraint::Type::PointToPoint;
				constraint.frame_a = btTransform(btQuaternion::getIdentity(), climb_offset);
				constraint.frame_b = btTransform(btQuaternion::getIdentity(), Vec3::zero),
				constraint.a = get<RigidBody>();
				constraint.b = rope_body;
				rope_constraint = RigidBody::add_constraint(constraint);
			}
		}
		else if (fsm.current == State::Climb)
		{
			RigidBody* body = get<RigidBody>();
			Vec3 v = body->btBody->getLinearVelocity();
			Vec2 accel = get<Walker>()->dir * AIR_CONTROL_ACCEL * u.time.delta;
			body->btBody->setLinearVelocity(v + Vec3(accel.x, 0, accel.y));

			if (climb_velocity != 0.0f)
			{
				RigidBody::Constraint* constraint = &RigidBody::global_constraints[rope_constraint];
				Transform* new_rope = parkour_get_rope(this, climb_velocity > 0.0f ? ParkourRopeSearch::Above : ParkourRopeSearch::Below);
				if (new_rope && new_rope != rope.ref())
				{
					// switch to new segment
					RigidBody* new_rope_body = new_rope->get<RigidBody>();
					constraint->b = new_rope_body;
					constraint->frame_b.setOrigin(Vec3(0, 0, ROPE_SEGMENT_LENGTH * 0.75f * (climb_velocity > 0.0f ? 1.0f : -1.0f)));
					rope = new_rope;
				}
				else
				{
					// keep climbing on current segment
					Vec3 origin = constraint->frame_b.getOrigin();
					origin.z = LMath::clampf(origin.z - climb_velocity * 1.5f * u.time.delta, ROPE_SEGMENT_LENGTH * -0.75f, ROPE_SEGMENT_LENGTH * 0.75f);
					constraint->frame_b.setOrigin(origin);
				}
				RigidBody::rebuild_constraint(rope_constraint);
			}
		}
	}

	if (!Game::level.local)
	{
		if (last_frame_state != fsm.current)
			ParkourNet::sync_state(this);
		last_frame_state = fsm.current;
	}

	// mantle animation stuff

	{
		Animator::Layer* layer1 = &get<Animator>()->layers[1];
		if ((layer1->animation == Asset::Animation::character_top_out || layer1->animation == Asset::Animation::character_mantle)
			&& fsm.current != State::Mantle
			&& fsm.time > 0.2f)
			layer1->animation = AssetNull;
	}

	// update model offset
	{
		get<Animator>()->update_server(u);
		Vec3 model_offset(0, get<Walker>()->capsule_height() * -0.5f - WALKER_SUPPORT_HEIGHT, 0);
		{
			Animator::Layer* layer1 = &get<Animator>()->layers[1];

			if (animation_start_support.ref())
			{
				Vec3 offset = animation_start_support.ref()->get<Transform>()->to_world(relative_animation_start_pos) - get<Transform>()->absolute_pos();
				if (layer1->animation == Asset::Animation::character_top_out || layer1->animation == Asset::Animation::character_mantle)
					model_offset += offset * Ease::quad_out<r32>(layer1->blend);
				else if (layer1->last_animation == Asset::Animation::character_top_out || layer1->last_animation == Asset::Animation::character_mantle)
					model_offset += offset * (1.0f - Ease::quad_out<r32>(layer1->blend));
			}
		}

		get<SkinnedModel>()->offset.make_transform(
			model_offset,
			Vec3(1.0f, 1.0f, 1.0f),
			Quat::euler(0, get<Walker>()->rotation + PI * 0.5f, 0) * Quat::euler(0, 0, lean * -1.5f)
		);
	}

	// handle collectibles
	{
		b8 pickup = get<Animator>()->layers[3].animation == AssetNull; // should we look for collectibles to pick up?
		Vec3 me = get<Walker>()->base_pos();
		for (auto i = Collectible::list.iterator(); !i.is_last(); i.next())
		{
			Transform* t = i.item()->get<Transform>();

			if (pickup && t->parent.ref() != get<Transform>() && (t->absolute_pos() - me).length_squared() < COLLECTIBLE_RADIUS * COLLECTIBLE_RADIUS)
				ParkourNet::pickup(this, i.item()); // pick it up

			if (t->parent.ref() == get<Transform>())
			{
				// glue it to our hand
				parkour_set_collectible_position(get<Animator>(), t);
				break;
			}
		}
	}
}

void Parkour::spawn_tiles(const Vec3& relative_wall_right, const Vec3& relative_wall_up, const Vec3& relative_wall_normal, const Vec3& spawn_offset)
{
	TilePos wall_coord =
	{
		s32(relative_support_pos.dot(relative_wall_right) * (1.0f / TILE_SIZE)),
		s32(relative_support_pos.dot(relative_wall_up) * (1.0f / TILE_SIZE)),
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
		for (s32 x = -s32(TILE_CREATE_RADIUS); x <= s32(TILE_CREATE_RADIUS); x++)
		{
			for (s32 y = -s32(TILE_CREATE_RADIUS); y <= s32(TILE_CREATE_RADIUS); y++)
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

					if (create && Tile::list.count() < MAX_ENTITIES)
					{
						Vec2 relative_tile_wall_coord = Vec2(wall_coord.x + x, wall_coord.y + y) * TILE_SIZE;
						Vec3 relative_tile_pos = (relative_wall_right * relative_tile_wall_coord.x)
							+ (relative_wall_up * relative_tile_wall_coord.y)
							+ (relative_wall_normal * relative_wall_z);
						Vec3 absolute_tile_pos = last_support.ref()->get<Transform>()->to_world(relative_tile_pos);

						r32 anim_time = tile_history.length == 0 ? (0.03f + 0.01f * i) : 0.3f;
						Tile::add(absolute_tile_pos, absolute_wall_rot, spawn_offset, last_support.ref()->get<Transform>(), anim_time);

						i++;
					}
				}
			}
		}
		if (tile_history.length == tile_history.capacity())
			tile_history.remove_ordered(0);
		tile_history.add(wall_coord);
	}
}

void Parkour::pickup_animation_complete()
{
	// delete whatever we're holding
	if (Game::level.local)
	{
		for (auto i = Collectible::list.iterator(); !i.is_last(); i.next())
		{
			// compare IDs rather than refs because we're tearing down this entity and the revision of our Transform has already been incremented
			if (i.item()->get<Transform>()->parent.id == get<Transform>()->id())
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
	if (fsm.current == State::Climb)
	{
		parkour_stop_climbing(this);
		RigidBody* body = get<RigidBody>();
		Vec3 velocity = body->btBody->getLinearVelocity();
		r32 velocity_length = velocity.length();
		r32 speed = vi_max(MIN_WALLRUN_SPEED, velocity_length * 1.5f);
		body->btBody->setLinearVelocity(velocity * (speed / velocity_length) + Vec3(0, JUMP_SPEED, 0));
		did_jump = true;
	}
	else if (fsm.current == State::Normal || fsm.current == State::WallRun)
	{
		if ((get<Walker>()->support.ref() && get<Walker>()->support.ref()->btBody->getBroadphaseProxy()->m_collisionFilterGroup & CollisionParkour)
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
						const btRigidBody* support_body = (const btRigidBody*)(ray_callback.m_collisionObject);
						wall_jump(rotation, wall_normal, support_body);
						did_jump = true;
						break;
					}
				}
			}
		}
	}

	if (!did_jump && can_double_jump && Game::save.extended_parkour)
	{
		Vec3 velocity = get<RigidBody>()->btBody->getLinearVelocity();
		if (velocity.y < 0.0f) // have to be going down to double jump
		{
			Vec3 spawn_offset = velocity * 1.5f;
			spawn_offset.y = 5.0f;

			Vec3 pos = get<Walker>()->base_pos();

			Quat tile_rot = Quat::look(Vec3(0, 1, 0));
			const r32 radius = TILE_CREATE_RADIUS - 2.0f;
			s32 i = 0;
			for (s32 x = -s32(TILE_CREATE_RADIUS); x <= s32(radius); x++)
			{
				for (s32 y = -s32(TILE_CREATE_RADIUS); y <= s32(radius); y++)
				{
					if (Vec2(x, y).length_squared() < radius * radius)
					{
						Tile::add(pos + Vec3(x, 0, y) * TILE_SIZE, tile_rot, spawn_offset, nullptr, 0.15f + 0.05f * i);
						i++;
					}
				}
			}

			// override horizontal velocity based on current facing angle
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

	if (did_jump)
	{
		get<Audio>()->post_event(AK::EVENTS::PLAY_PARKOUR_JUMP);
		fsm.transition(State::Normal);
		footstep();
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

	last_support = nullptr;
	last_support_wall_run_state = WallRunState::None;
	last_support_time = Game::time.total;
}

const s32 mantle_sample_count = 3;
Vec3 mantle_samples[mantle_sample_count] =
{
	Vec3(0, 0, 1),
	Vec3(-0.35f, 0, 1),
	Vec3(0.35f, 0, 1),
};

// i hate you OOP
struct RayCallbackDefaultConstructor : public btCollisionWorld::ClosestRayResultCallback
{
	RayCallbackDefaultConstructor() : btCollisionWorld::ClosestRayResultCallback(btVector3(), btVector3())
	{
	}

	RayCallbackDefaultConstructor(const btVector3& start, const btVector3& end) : btCollisionWorld::ClosestRayResultCallback(start, end)
	{
	}
};

// if force is true, we'll raycast farther downward when trying to mantle, to make sure we find something.
b8 Parkour::try_parkour(MantleAttempt attempt)
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
		// try to mantle
		Vec3 pos = get<Transform>()->absolute_pos();
		Walker* walker = get<Walker>();

		RayCallbackDefaultConstructor ray_callbacks[mantle_sample_count];

		// perform all raycasts, checking for obstacles first
		for (s32 i = 0; i < mantle_sample_count; i++)
		{
			Vec3 dir_offset = rot * (mantle_samples[i] * WALKER_RADIUS * RAYCAST_RADIUS_RATIO);

			Vec3 ray_start = pos + Vec3(dir_offset.x, 1.3f, dir_offset.z);

			r32 extra_raycast;
			switch (attempt)
			{
				case MantleAttempt::Normal:
				{
					extra_raycast = 0.0f;
					break;
				}
				case MantleAttempt::Extra:
				{
					extra_raycast = -WALKER_SUPPORT_HEIGHT;
					break;
				}
				case MantleAttempt::Force:
				{
					extra_raycast = -WALKER_SUPPORT_HEIGHT - 0.5f;
					break;
				}
				default:
				{
					vi_assert(false);
					break;
				}
			}
			Vec3 ray_end = pos + Vec3(dir_offset.x, WALKER_DEFAULT_CAPSULE_HEIGHT * -0.25f + extra_raycast, dir_offset.z);

			RayCallbackDefaultConstructor ray_callback(ray_start, ray_end);
			Physics::raycast(&ray_callback, CollisionParkour);

			// check for wall blocking the mantle
			if (ray_callback.hasHit() && ray_callback.m_hitNormalWorld.getY() > 0.25f)
			{
				Vec3 wall_ray_start = pos;
				wall_ray_start.y = ray_callback.m_hitPointWorld.getY() + 0.1f;
				Vec3 wall_ray_end = ray_callback.m_hitPointWorld;
				wall_ray_end.y += 0.1f;
				btCollisionWorld::ClosestRayResultCallback wall_ray_callback(wall_ray_start, wall_ray_end);
				Physics::raycast(&wall_ray_callback, ~CollisionDroneIgnore);
				if (wall_ray_callback.hasHit())
					return false;

				ray_callbacks[i] = ray_callback;
			}
		}

		// no obstacles. see if any of the raycasts were valid and if so, use it
		for (s32 i = 0; i < mantle_sample_count; i++)
		{
			const RayCallbackDefaultConstructor& ray_callback = ray_callbacks[i];
			if (ray_callback.hasHit() && ray_callback.m_hitNormalWorld.getY() > 0.25f)
			{
				b8 top_out = ray_callback.m_hitPointWorld.getY() > pos.y + 0.2f;

				get<Animator>()->layers[1].play(top_out ? Asset::Animation::character_top_out : Asset::Animation::character_mantle);
				fsm.transition(State::Mantle);
				last_support = Entity::list[ray_callback.m_collisionObject->getUserIndex()].get<RigidBody>();
				last_support_wall_run_state = WallRunState::None;
				relative_support_pos = last_support.ref()->get<Transform>()->to_local(ray_callback.m_hitPointWorld + Vec3(0, WALKER_SUPPORT_HEIGHT + WALKER_DEFAULT_CAPSULE_HEIGHT * 0.6f, 0));
				relative_animation_start_pos = pos;
				relative_animation_start_pos.y = ray_callback.m_hitPointWorld.getY();
				relative_animation_start_pos += rot * (top_out ? Vec3(0, -0.65f, 0) : Vec3(0, 0, -0.35f));
				animation_start_support = last_support.ref()->get<Transform>();
				relative_animation_start_pos = animation_start_support.ref()->to_local(relative_animation_start_pos);
				last_support_time = Game::time.total;

				get<RigidBody>()->btBody->setLinearVelocity(Vec3::zero);

				get<Audio>()->post_event(top_out ? AK::EVENTS::PLAY_PARKOUR_TOPOUT : AK::EVENTS::PLAY_PARKOUR_MANTLE);

				return true;
			}
		}
	}

	return false;
}

void Parkour::wall_run_up_add_velocity(const Vec3& velocity, const Vec3& support_velocity)
{
	Vec3 horizontal_velocity = velocity - support_velocity;
	r32 vertical_velocity = vi_max(0.0f, horizontal_velocity.y);
	horizontal_velocity.y = 0.0f;
	r32 speed = LMath::clampf(horizontal_velocity.length(), 2.0f, MAX_SPEED);
	get<RigidBody>()->btBody->setLinearVelocity(support_velocity + Vec3(0, 3.0f + speed + vertical_velocity, 0));
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

		const btRigidBody* support_body = (const btRigidBody*)(ray_callback.m_collisionObject);
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
				// going up
				wall_run_up_add_velocity(velocity, support_velocity);
			}
			else
			{
				// going down
				body->setLinearVelocity(support_velocity + Vec3(0, (vertical_velocity - support_velocity.y) * 0.5f, 0));
			}
		}
		else
		{
			// side wall run
			Vec3 relative_velocity = velocity - support_velocity;
			Vec3 velocity_flattened = relative_velocity - wall_normal * relative_velocity.dot(wall_normal);
			r32 flattened_vertical_speed = velocity_flattened.y;
			velocity_flattened.y = 0.0f;

			// make sure we're facing the same way as we'll be moving
			Vec3 forward = Quat::euler(0, get<Walker>()->rotation, 0) * Vec3(0, 0, 1);
			if (velocity_flattened.dot(forward) < 0.0f)
				velocity_flattened *= -1.0f;

			r32 speed = vi_max(get<Walker>()->net_speed, MIN_WALLRUN_SPEED + 1.0f);

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
