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
#include "asset/shader.h"
#include "data/animator.h"
#include "render/views.h"
#include "game.h"
#include "console.h"
#include "minion.h"
#include "strings.h"
#include "render/particles.h"
#include "net.h"
#include "net_serialize.h"
#include "team.h"

namespace VI
{

#define LERP_ROTATION_SPEED 10.0f
#define LERP_TRANSLATION_SPEED 3.0f
#define MAX_FLIGHT_TIME 6.0f
#define AWK_LEG_LENGTH (0.277f - 0.101f)
#define AWK_LEG_BLEND_SPEED (1.0f / 0.03f)
#define AWK_MIN_LEG_BLEND_SPEED (AWK_LEG_BLEND_SPEED * 0.1f)
#define AWK_SHIELD_ALPHA 0.2f
#define AWK_OVERSHIELD_ALPHA 0.75f
#define AWK_OVERSHIELD_RADIUS (AWK_SHIELD_RADIUS * 1.1f)
#define AWK_SHIELD_ANIM_TIME 0.35f

AwkRaycastCallback::AwkRaycastCallback(const Vec3& a, const Vec3& b, const Entity* awk)
	: btCollisionWorld::ClosestRayResultCallback(a, b)
{
	closest_target_hit_fraction = 2.0f;
	entity_id = awk->id();
}

b8 AwkRaycastCallback::hit_target() const
{
	return closest_target_hit_fraction < 2.0f;
}

btScalar AwkRaycastCallback::addSingleResult(btCollisionWorld::LocalRayResult& ray_result, b8 normalInWorldSpace)
{
	s32 collision_entity_id = ray_result.m_collisionObject->getUserIndex();
	if (collision_entity_id == entity_id)
		return m_closestHitFraction; // keep going

	s16 filter_group = ray_result.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup;
	if (filter_group & (CollisionWalker | CollisionShield | CollisionTarget | CollisionAwk))
	{
		Entity* entity = &Entity::list[collision_entity_id];
		// if it's a minion, do an extra headshot test
		if (!entity->has<MinionCommon>() || entity->get<MinionCommon>()->headshot_test(m_rayFromWorld, m_rayToWorld))
		{
			if (ray_result.m_hitFraction < closest_target_hit_fraction)
			{
				closest_target_hit_fraction = ray_result.m_hitFraction;
				closest_target_hit_group = filter_group;
			}
			return m_closestHitFraction; // keep going
		}
	}

	m_closestHitFraction = ray_result.m_hitFraction;
	m_collisionObject = ray_result.m_collisionObject;
	if (normalInWorldSpace)
		m_hitNormalWorld = ray_result.m_hitNormalLocal;
	else // need to transform normal into worldspace
		m_hitNormalWorld = m_collisionObject->getWorldTransform().getBasis() * ray_result.m_hitNormalLocal;
	m_hitPointWorld.setInterpolate3(m_rayFromWorld, m_rayToWorld, ray_result.m_hitFraction);
	return ray_result.m_hitFraction;
}

namespace AwkNet
{

enum class Message
{
	FlyStart,
	FlyDone,
	DashStart,
	DashDone,
	count,
};

b8 start_flying(Awk* a, Vec3 dir)
{
	using Stream = Net::StreamWrite;
	Net::StreamWrite* p = Net::msg_new_local(Net::MessageType::Awk);
	{
		Ref<Awk> ref = a;
		serialize_ref(p, ref);
	}
	{
		Message t = Message::FlyStart;
		serialize_enum(p, Message, t);
	}
	dir.normalize();
	serialize_r32_range(p, dir.x, -1.0f, 1.0f, 16);
	serialize_r32_range(p, dir.y, -1.0f, 1.0f, 16);
	serialize_r32_range(p, dir.z, -1.0f, 1.0f, 16);
	Net::msg_finalize(p);
	return true;
}

b8 start_dashing(Awk* a, Vec3 dir)
{
	using Stream = Net::StreamWrite;
	Net::StreamWrite* p = Net::msg_new_local(Net::MessageType::Awk);
	{
		Ref<Awk> ref = a;
		serialize_ref(p, ref);
	}
	{
		Message t = Message::DashStart;
		serialize_enum(p, Message, t);
	}
	dir.normalize();
	serialize_r32_range(p, dir.x, -1.0f, 1.0f, 16);
	serialize_r32_range(p, dir.y, -1.0f, 1.0f, 16);
	serialize_r32_range(p, dir.z, -1.0f, 1.0f, 16);
	Net::msg_finalize(p);
	return true;
}

b8 finish_flying(Awk* a)
{
	using Stream = Net::StreamWrite;
	Net::StreamWrite* p = Net::msg_new_local(Net::MessageType::Awk);
	{
		Ref<Awk> ref = a;
		serialize_ref(p, ref);
	}
	{
		Message t = Message::FlyDone;
		serialize_enum(p, Message, t);
	}
	Net::msg_finalize(p);
	return true;
}

b8 finish_dashing(Awk* a)
{
	using Stream = Net::StreamWrite;
	Net::StreamWrite* p = Net::msg_new_local(Net::MessageType::Awk);
	{
		Ref<Awk> ref = a;
		serialize_ref(p, ref);
	}
	{
		Message t = Message::DashDone;
		serialize_enum(p, Message, t);
	}
	Net::msg_finalize(p);
	return true;
}

}

b8 Awk::net_msg(Net::StreamRead* p, Net::MessageSource src)
{
	using Stream = Net::StreamRead;

	Awk* awk;
	{
		Ref<Awk> ref;
		serialize_ref(p, ref);
		awk = ref.ref();
	}

	AwkNet::Message type;
	serialize_enum(p, AwkNet::Message, type);

	// should we actually pay attention to this message?
	// if it's a message from a remote, but we are a local entity, then ignore the message.
	b8 apply_msg = src == Net::MessageSource::Loopback || !awk->has<PlayerControlHuman>() || !awk->get<PlayerControlHuman>()->local();

	switch (type)
	{
		case AwkNet::Message::FlyStart:
		{
			Vec3 dir;
			serialize_r32_range(p, dir.x, -1.0f, 1.0f, 16);
			serialize_r32_range(p, dir.y, -1.0f, 1.0f, 16);
			serialize_r32_range(p, dir.z, -1.0f, 1.0f, 16);

			if (apply_msg && awk->charges > 0)
			{
				awk->velocity = dir * AWK_FLY_SPEED;
				awk->get<Transform>()->absolute_pos(awk->get<Transform>()->absolute_pos() + dir * AWK_RADIUS * 0.5f);
				awk->get<Transform>()->absolute_rot(Quat::look(dir));

				awk->get<Audio>()->post_event(awk->has<PlayerControlHuman>() && awk->get<PlayerControlHuman>()->local() ? AK::EVENTS::PLAY_LAUNCH_PLAYER : AK::EVENTS::PLAY_LAUNCH);

				awk->cooldown_setup();
				awk->detach_teleport();
			}

			break;
		}
		case AwkNet::Message::DashStart:
		{
			Vec3 dir;
			serialize_r32_range(p, dir.x, -1.0f, 1.0f, 16);
			serialize_r32_range(p, dir.y, -1.0f, 1.0f, 16);
			serialize_r32_range(p, dir.z, -1.0f, 1.0f, 16);

			if (apply_msg && awk->charges > 0)
			{
				awk->velocity = dir * AWK_DASH_SPEED;
				awk->dash_timer = AWK_DASH_TIME;

				awk->hit_targets.length = 0;

				awk->attach_time = Game::time.total;
				awk->cooldown_setup();

				for (s32 i = 0; i < AWK_LEGS; i++)
					awk->footing[i].parent = nullptr;
				awk->get<Animator>()->reset_overrides();
				awk->get<Animator>()->layers[0].animation = Asset::Animation::awk_dash;

				awk->particle_accumulator = 0;

				awk->get<Audio>()->post_event(awk->has<PlayerControlHuman>() && awk->get<PlayerControlHuman>()->local() ? AK::EVENTS::PLAY_LAUNCH_PLAYER : AK::EVENTS::PLAY_LAUNCH);

				awk->dashed.fire();
			}

			break;
		}
		case AwkNet::Message::DashDone:
		{
			if (apply_msg)
			{
				Vec3 p = awk->get<Transform>()->absolute_pos();
				awk->finish_flying_dashing_common();
				awk->done_dashing.fire();
			}
			break;
		}
		case AwkNet::Message::FlyDone:
		{
			if (apply_msg)
			{
				awk->finish_flying_dashing_common();
				awk->done_flying.fire();
			}
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}
	return true;
}

void Awk::finish_flying_dashing_common()
{
	get<Animator>()->layers[0].animation = AssetNull;

	get<Audio>()->post_event(has<PlayerControlHuman>() && get<PlayerControlHuman>()->local() ? AK::EVENTS::PLAY_LAND_PLAYER : AK::EVENTS::PLAY_LAND);
	attach_time = Game::time.total;
	dash_timer = 0.0f;

	velocity = Vec3::zero;
	get<Transform>()->absolute(&lerped_pos, &lerped_rotation);
	last_pos = lerped_pos;
}

Awk* Awk::closest(AI::TeamMask mask, const Vec3& pos, r32* distance)
{
	Awk* closest = nullptr;
	r32 closest_distance = FLT_MAX;

	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->get<AIAgent>()->team, mask))
		{
			r32 d = (i.item()->get<Transform>()->absolute_pos() - pos).length_squared();
			if (d < closest_distance)
			{
				closest = i.item();
				closest_distance = d;
			}
		}
	}

	if (distance)
	{
		if (closest)
			*distance = sqrtf(closest_distance);
		else
			*distance = FLT_MAX;
	}

	return closest;
}

Awk::Awk()
	: velocity(0.0f, -AWK_FLY_SPEED, 0.0f),
	done_flying(),
	done_dashing(),
	detached(),
	dashed(),
	shield_time(),
	dash_timer(),
	attach_time(Game::time.total),
	footing(),
	last_footstep(),
	shield(),
	overshield(),
	bounce(),
	hit_targets(),
	cooldown(),
	charges(AWK_CHARGES),
	overshield_timer(AWK_OVERSHIELD_TIME),
	particle_accumulator(),
	current_ability(Ability::None),
	ability_spawned()
{
}

void Awk::awake()
{
	link_arg<Entity*, &Awk::killed>(get<Health>()->killed);
	link_arg<const HealthEvent&, &Awk::health_changed>(get<Health>()->changed);
	link_arg<const TargetEvent&, &Awk::hit_by>(get<Target>()->target_hit);
	if (Game::level.local && !shield.ref())
	{
		{
			Entity* shield_entity = World::create<Empty>();
			shield_entity->get<Transform>()->parent = get<Transform>();
			shield_entity->add<RigidBody>(RigidBody::Type::Sphere, Vec3(AWK_SHIELD_RADIUS), 0.0f, CollisionShield, CollisionDefault, AssetNull, entity_id);
			shield = shield_entity;

			View* s = shield_entity->add<View>();
			s->team = (s8)get<AIAgent>()->team;
			s->mesh = Asset::Mesh::sphere_highres;
			s->offset.scale(Vec3(AWK_SHIELD_RADIUS));
			s->shader = Asset::Shader::fresnel;
			s->alpha();
			s->color.w = AWK_SHIELD_ALPHA;

			Net::finalize(shield_entity);
		}

		vi_assert(!overshield.ref());
		{
			// overshield
			Entity* shield_entity = World::create<Empty>();
			shield_entity->get<Transform>()->parent = get<Transform>();
			overshield = shield_entity;

			View* s = shield_entity->add<View>();
			s->team = (s8)get<AIAgent>()->team;
			s->mesh = Asset::Mesh::sphere_highres;
			s->offset.scale(Vec3(AWK_OVERSHIELD_RADIUS));
			s->shader = Asset::Shader::fresnel;
			s->alpha();
			s->color.w = AWK_OVERSHIELD_ALPHA;

			Net::finalize(shield_entity);
		}
	}
	shield_time = Game::time.total;
}

Awk::~Awk()
{
	if (Game::level.local)
	{
		if (shield.ref())
			World::remove_deferred(shield.ref());
		if (overshield.ref())
			World::remove_deferred(overshield.ref());
	}
}

Awk::State Awk::state() const
{
	if (dash_timer > 0.0f)
		return State::Dash;
	else if (get<Transform>()->parent.ref())
		return State::Crawl;
	else
		return State::Fly;
}

s16 Awk::ally_containment_field_mask() const
{
	return Team::containment_field_mask(get<AIAgent>()->team);
}

Vec3 Awk::center_lerped() const
{
	return lerped_pos;
}

Vec3 Awk::attach_point(r32 offset) const
{
	Quat rot;
	Vec3 pos;
	get<Transform>()->absolute(&pos, &rot);
	return pos + rot * Vec3(0, 0, offset + -AWK_RADIUS);
}

Entity* Awk::incoming_attacker() const
{
	Vec3 me = get<Transform>()->absolute_pos();

	// check incoming Awks
	for (auto i = PlayerCommon::list.iterator(); !i.is_last(); i.next())
	{
		if (PlayerCommon::visibility.get(PlayerCommon::visibility_hash(get<PlayerCommon>(), i.item())))
		{
			// determine if they're attacking us
			if (i.item()->get<Awk>()->state() != Awk::State::Crawl
				&& Vec3::normalize(i.item()->get<Awk>()->velocity).dot(Vec3::normalize(me - i.item()->get<Transform>()->absolute_pos())) > 0.98f)
			{
				return i.item()->entity();
			}
		}
	}

	// check incoming projectiles
	for (auto i = Projectile::list.iterator(); !i.is_last(); i.next())
	{
		Vec3 velocity = Vec3::normalize(i.item()->velocity);
		Vec3 projectile_pos = i.item()->get<Transform>()->absolute_pos();
		Vec3 to_me = me - projectile_pos;
		r32 dot = velocity.dot(to_me);
		if (dot > 0.0f && dot < AWK_MAX_DISTANCE && velocity.dot(Vec3::normalize(to_me)) > 0.98f)
		{
			// only worry about it if it can actually see us
			btCollisionWorld::ClosestRayResultCallback ray_callback(me, projectile_pos);
			Physics::raycast(&ray_callback, ~CollisionAwk & ~CollisionAwkIgnore & ~CollisionShield);
			if (!ray_callback.hasHit())
				return i.item()->entity();
		}
	}

	// check incoming rockets
	if (!get<AIAgent>()->stealth)
	{
		Rocket* rocket = Rocket::inbound(entity());
		if (rocket)
		{
			// only worry about it if the rocket can actually see us
			btCollisionWorld::ClosestRayResultCallback ray_callback(me, rocket->get<Transform>()->absolute_pos());
			Physics::raycast(&ray_callback, ~CollisionAwk & ~CollisionAwkIgnore & ~CollisionShield);
			if (!ray_callback.hasHit())
				return rocket->entity();
		}
	}

	return nullptr;
}

void Awk::hit_by(const TargetEvent& e)
{
	get<Audio>()->post_event(has<PlayerControlHuman>() && get<PlayerControlHuman>()->local() ? AK::EVENTS::PLAY_HURT_PLAYER : AK::EVENTS::PLAY_HURT);

	b8 damaged = false;

	// only take damage if we're attached to a wall and have no overshield
	if (state() == Awk::State::Crawl
		&& overshield_timer == 0.0f
		&& (!e.hit_by->has<Awk>() || e.hit_by->get<AIAgent>()->team != get<AIAgent>()->team))
	{
		get<Health>()->damage(e.hit_by, 1);
		damaged = true;
	}

	// let them know they didn't hurt us
	if (!damaged && e.hit_by->has<PlayerControlHuman>())
		e.hit_by->get<PlayerControlHuman>()->player.ref()->msg(_(strings::no_effect), false);
}

b8 Awk::hit_target(Entity* target)
{
	if (!Game::level.local) // target hit events are synced across the network
		return false;

	for (s32 i = 0; i < hit_targets.length; i++)
	{
		if (hit_targets[i].ref() == target)
			return false; // we've already hit this target once during this flight
	}
	if (hit_targets.length < hit_targets.capacity())
		hit_targets.add(target);

	Vec3 hit_pos = target->get<Target>()->absolute_pos();

	// particles
	{
		Quat rot = Quat::look(Vec3::normalize(velocity));
		for (s32 i = 0; i < 50; i++)
		{
			Particles::sparks.add
			(
				hit_pos,
				rot * Vec3(mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo()) * 10.0f,
				Vec4(1, 1, 1, 1)
			);
		}

		Entity* shockwave = World::create<ShockwaveEntity>(8.0f, 1.5f);
		shockwave->get<Transform>()->absolute_pos(hit_pos);
		Net::finalize(shockwave);
	}

	// award credits for hitting stuff
	if (target->has<MinionAI>())
	{
		if (target->get<AIAgent>()->team != get<AIAgent>()->team)
		{
			PlayerManager* owner = target->get<MinionCommon>()->owner.ref();
			if (owner)
			{
				owner->team.ref()->track(get<PlayerCommon>()->manager.ref());
				get<PlayerCommon>()->manager.ref()->add_credits(CREDITS_MINION_KILL);
			}
		}
	}
	else if (target->has<Sensor>())
	{
		b8 is_enemy = target->get<Sensor>()->team != get<AIAgent>()->team;
		if (is_enemy)
			get<PlayerCommon>()->manager.ref()->add_credits(CREDITS_SENSOR_DESTROY);
	}
	else if (target->has<ContainmentField>())
	{
		b8 is_enemy = target->get<ContainmentField>()->team != get<AIAgent>()->team;
		if (is_enemy)
			get<PlayerCommon>()->manager.ref()->add_credits(CREDITS_CONTAINMENT_FIELD_DESTROY);
	}

	if (current_ability == Ability::None && overshield_timer > 0.0f && target->has<Awk>())
		overshield_timer = 0.0f; // damaging an Awk takes our shield down

	if (target->has<Target>())
	{
		Ref<Target> t = target->get<Target>();

		t.ref()->hit(entity());

		if (t.ref() && t.ref()->has<RigidBody>()) // is it still around and does it have a rigidbody?
		{
			RigidBody* body = t.ref()->get<RigidBody>();
			body->btBody->applyImpulse(velocity * 0.1f, Vec3::zero);
			body->btBody->activate(true);
		}
	}

	hit.fire(target);

	return true;
}

b8 Awk::predict_intersection(const Target* target, Vec3* intersection, r32 speed) const
{
	if (current_ability == Ability::Sniper) // instant bullet travel time
	{
		*intersection = target->absolute_pos();
		return true;
	}
	else
		return target->predict_intersection(get<Transform>()->absolute_pos(), speed, intersection);
}

void Awk::health_changed(const HealthEvent& e)
{
	if (e.amount < 0)
	{
		// damaged

		if (e.shield != 0)
			shield_time = Game::time.total;

		if (get<Health>()->hp == 0)
		{
			// killed; notify everyone
			if (e.source)
			{
				PlayerCommon* enemy = nullptr;
				if (e.source->has<PlayerCommon>())
					enemy = e.source->get<PlayerCommon>();
				else if (e.source->has<Projectile>())
				{
					Entity* owner = e.source->get<Projectile>()->owner.ref();
					if (owner)
						enemy = owner->get<PlayerCommon>();
				}
				else if (e.source->has<Rocket>())
				{
					Entity* owner = e.source->get<Rocket>()->owner.ref();
					if (owner)
						enemy = owner->get<PlayerCommon>();
				}
				if (enemy)
					enemy->manager.ref()->add_kills(1);
			}

			AI::Team team = get<AIAgent>()->team;
			PlayerManager* manager = get<PlayerCommon>()->manager.ref();
			for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
			{
				if (i.item()->get<PlayerManager>() != manager) // don't need to notify ourselves
				{
					b8 friendly = i.item()->get<PlayerManager>()->team.ref()->team() == team;
					char buffer[512];
					sprintf(buffer, _(strings::player_killed), manager->username);
					i.item()->msg(buffer, !friendly);
				}
			}
		}
	}

	if (e.shield != 0)
		shield_time = Game::time.total;
}

void Awk::killed(Entity* e)
{
	get<Audio>()->post_event(AK::EVENTS::STOP_FLY);
	World::remove_deferred(entity());
}

b8 Awk::can_dash(const Target* target, Vec3* out_intersection) const
{
	Vec3 intersection;
	if (predict_intersection(target, &intersection, AWK_DASH_SPEED))
	{
		// the Target is situated at the base of the enemy Awk, where it attaches to the surface.
		// we need to calculate the vector starting from our own base attach point, otherwise the dot product will be messed up.
		Vec3 me = get<Target>()->absolute_pos();
		Vec3 to_intersection = intersection - me;
		r32 distance = to_intersection.length();
		to_intersection /= distance;
		if (distance < AWK_DASH_DISTANCE)
		{
			Vec3 dash_to_intersection = intersection - me;
			r32 dot = to_intersection.dot(get<Transform>()->absolute_rot() * Vec3(0, 0, 1));
			if (fabs(dot) < 0.1f)
			{
				if (out_intersection)
					*out_intersection = intersection;
				return true;
			}
		}
	}
	return false;
}

b8 Awk::can_shoot(const Target* target, Vec3* out_intersection, r32 speed, const Net::StateFrame* state_frame) const
{
	Vec3 intersection;
	if (predict_intersection(target, &intersection, speed))
	{
		Vec3 me = get<Transform>()->absolute_pos();
		Vec3 to_intersection = intersection - me;
		r32 distance = to_intersection.length();
		to_intersection /= distance;

		Vec3 final_pos;
		b8 hit_target;
		if (can_shoot(to_intersection, &final_pos, &hit_target, state_frame))
		{
			if (hit_target || (final_pos - me).length() > distance - AWK_RADIUS * 2.0f)
			{
				if (out_intersection)
					*out_intersection = intersection;
				return true;
			}
		}
	}
	return false;
}

b8 Awk::can_hit(const Target* target, Vec3* out_intersection) const
{
	// first try to dash there
	if (can_dash(target, out_intersection))
		return true;

	// now try to fly there
	if (can_shoot(target, out_intersection))
		return true;

	return false;
}

b8 Awk::direction_is_toward_attached_wall(const Vec3& dir) const
{
	Vec3 wall_normal = get<Transform>()->absolute_rot() * Vec3(0, 0, 1);
	return dir.dot(wall_normal) < 0.0f;
}

b8 awk_state_frame(const Awk* a, Net::StateFrame* state_frame)
{
	if (a->has<PlayerControlHuman>() && !a->get<PlayerControlHuman>()->local()) // this Awk is being controlled remotely; we need to rewind the world state to what it looks like from their side
		return Net::state_frame_by_timestamp(state_frame, Game::real_time.total - Net::rtt(a->get<PlayerControlHuman>()->player.ref()) - NET_INTERPOLATION_DELAY);
	else
		return false;
}

b8 Awk::can_shoot(const Vec3& dir, Vec3* final_pos, b8* hit_target, const Net::StateFrame* state_frame) const
{
	Vec3 trace_dir = Vec3::normalize(dir);

	// if we're attached to a wall, make sure we're not shooting into the wall
	if (state() == Awk::State::Crawl && direction_is_toward_attached_wall(trace_dir))
		return false;

	// can't shoot straight up or straight down
	// HACK: if it's a local player, let them do what they want because it's frustrating
	// in certain cases where the drone won't let you go where you should be able to go
	// due to the third-person camera offset
	// the AI however needs to know whether it can hit actually hit a target
	if (!has<PlayerControlHuman>() && fabs(trace_dir.y) > AWK_VERTICAL_DOT_LIMIT)
		return false;

	Vec3 trace_start = get<Transform>()->absolute_pos();
	Vec3 trace_end = trace_start + trace_dir * AWK_SNIPE_DISTANCE;

	Net::StateFrame state_frame_data;
	if (!state_frame && awk_state_frame(this, &state_frame_data))
		state_frame = &state_frame_data;

	Hits hits;
	raycast(trace_start, trace_end, state_frame, &hits);

	r32 r = range();
	const Hit* environment_hit = nullptr;
	b8 allow_further_end = false; // allow awk to shoot if we're aiming at an enemy awk in range but the backing behind it is out of range
	b8 hit_target_value = false;
	for (s32 i = 0; i < hits.hits.length; i++)
	{
		const Hit& hit = hits.hits[i];
		if (hit.type == Hit::Type::Environment)
			environment_hit = &hit;
		if (hit.fraction * AWK_SNIPE_DISTANCE < r)
		{
			if (hit.type == Hit::Type::Awk)
			{
				allow_further_end = true;
				if (hit.fraction <= hits.fraction_end)
					hit_target_value = true;
			}
			else if (hit.type == Hit::Type::Target && hit.fraction <= hits.fraction_end)
				hit_target_value = true;
		}
	}

	if (environment_hit)
	{
		// need to check that the environment hit is within range
		// however if we are shooting at an Awk, we can tolerate further environment hits
		b8 can_shoot = false;
		if (allow_further_end || environment_hit->fraction * AWK_SNIPE_DISTANCE < r)
			can_shoot = true;
		else
		{
			// check awk target predictions
			r32 end_distance_sq = hits.fraction_end * AWK_SNIPE_DISTANCE * hits.fraction_end * AWK_SNIPE_DISTANCE;
			for (auto i = list.iterator(); !i.is_last(); i.next())
			{
				if (i.item() != this && (i.item()->get<Transform>()->absolute_pos() - trace_start).length_squared() > AWK_SHIELD_RADIUS * 2.0f * AWK_SHIELD_RADIUS * 2.0f)
				{
					Vec3 intersection;
					if (predict_intersection(i.item()->get<Target>(), &intersection))
					{
						if ((intersection - trace_start).length_squared() <= end_distance_sq
							&& LMath::ray_sphere_intersect(trace_start, trace_end, intersection, AWK_SHIELD_RADIUS))
						{
							can_shoot = true;
							hit_target_value = true;
							break;
						}
					}
				}
			}
		}

		if (can_shoot)
		{
			if (final_pos)
				*final_pos = hits.hits[hits.index_end].pos;
			if (hit_target)
				*hit_target = hit_target_value;
			return true;
		}
	}
	return false;
}

b8 Awk::can_spawn(Ability a, const Vec3& dir, Vec3* final_pos, Vec3* final_normal, RigidBody** hit_parent, b8* hit_target) const
{
	Vec3 trace_dir = Vec3::normalize(dir);

	// can't shoot straight up or straight down
	// HACK: if it's a local player, let them do what they want because it's frustrating
	// in certain cases where the drone won't let you go where you should be able to go
	// due to the third-person camera offset
	// the AI however needs to know whether it can hit actually hit a target
	if (!has<PlayerControlHuman>() && fabs(trace_dir.y) > AWK_VERTICAL_DOT_LIMIT)
		return false;

	Vec3 trace_start = get<Transform>()->absolute_pos();
	Vec3 trace_end = trace_start + trace_dir * range();

	if (a == Ability::Sniper)
	{
		RaycastCallbackExcept ray_callback(trace_start, trace_end, entity());
		Physics::raycast(&ray_callback, ~CollisionAwkIgnore & ~ally_containment_field_mask());
		if (ray_callback.hasHit())
		{
			Entity* e = &Entity::list[ray_callback.m_collisionObject->getUserIndex()];
			if (final_pos)
				*final_pos = ray_callback.m_hitPointWorld;
			if (hit_target)
				*hit_target = e->has<Target>();
			if (hit_parent)
				*hit_parent = e->get<RigidBody>();
		}
		else
		{
			if (final_pos)
				*final_pos = trace_end;
			if (hit_target)
				*hit_target = false;
			if (hit_parent)
				*hit_parent = nullptr;
		}
		return true; // we can always snipe, even if the bullet goes into space
	}
	else
	{
		AwkRaycastCallback ray_callback(trace_start, trace_end, entity());
		Physics::raycast(&ray_callback, ~CollisionAwkIgnore & ~ally_containment_field_mask());

		b8 can_spawn = ray_callback.hasHit()
			&& !(ray_callback.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup & AWK_INACCESSIBLE_MASK);
		if (can_spawn)
		{
			if (final_pos)
				*final_pos = ray_callback.m_hitPointWorld;
			if (final_normal)
				*final_normal = ray_callback.m_hitNormalWorld;
			if (hit_parent)
				*hit_parent = Entity::list[ray_callback.m_collisionObject->getUserIndex()].get<RigidBody>();
		}
		return can_spawn;
	}
}

void Awk::cooldown_setup()
{
	if (Game::level.local)
	{
		vi_assert(charges > 0);
		charges--;
	}
	cooldown = AWK_COOLDOWN;
}

void Awk::detach_teleport()
{
	hit_targets.length = 0;

	attach_time = Game::time.total;

	get<Transform>()->reparent(nullptr);
	get<SkinnedModel>()->offset = Mat4::identity;

	for (s32 i = 0; i < AWK_LEGS; i++)
		footing[i].parent = nullptr;
	get<Animator>()->reset_overrides();
	get<Animator>()->layers[0].animation = Asset::Animation::awk_fly;

	particle_accumulator = 0;
	detached.fire();
}

b8 Awk::dash_start(const Vec3& dir)
{
	if (state() != State::Crawl || current_ability != Ability::None)
		return false;

	if (!direction_is_toward_attached_wall(dir))
		return false;

	AwkNet::start_dashing(this, dir);

	return true;
}

b8 Awk::cooldown_can_shoot() const
{
	return charges > 0;
}

r32 Awk::range() const
{
	return (current_ability == Ability::Sniper || current_ability == Ability::Teleporter) ? AWK_SNIPE_DISTANCE : AWK_MAX_DISTANCE;
}

void particle_trail(const Vec3& start, const Vec3& dir, r32 distance, r32 interval = 2.0f)
{
	s32 particle_count = distance / interval;
	Vec3 interval_pos = dir * interval;
	Vec3 pos = start;
	for (s32 i = 0; i < particle_count; i++)
	{
		pos += interval_pos;
		Particles::tracers.add
		(
			pos,
			Vec3::zero,
			0,
			vi_min(0.25f, i * 0.05f)
		);
	}
}

b8 Awk::go(const Vec3& dir)
{
	if (!cooldown_can_shoot())
		return false;

	Vec3 dir_normalized = Vec3::normalize(dir);

	if (current_ability == Ability::None)
		AwkNet::start_flying(this, dir_normalized);
	else
	{
		// ability spawn
		// todo: net sync

		PlayerManager* manager = get<PlayerCommon>()->manager.ref();

		if (!manager->ability_valid(current_ability))
			return false;

		Vec3 pos;
		Vec3 normal;
		RigidBody* parent;
		if (!can_spawn(current_ability, dir_normalized, &pos, &normal, &parent))
			return false;

		Quat rot = Quat::look(normal);

		cooldown_setup();

		const AbilityInfo& info = AbilityInfo::list[(s32)current_ability];
		manager->add_credits(-info.spawn_cost);

		Vec3 me = get<Transform>()->absolute_pos();
		particle_trail(me, dir_normalized, (pos - me).length());

		if (current_ability != Ability::Sniper)
		{
			Entity* shockwave = World::create<ShockwaveEntity>(8.0f, 1.5f);
			shockwave->get<Transform>()->absolute_pos(pos + rot * Vec3(0, 0, AWK_RADIUS));
			Net::finalize(shockwave);
		}

		switch (current_ability)
		{
			case Ability::Sensor:
			{
				// place a proximity sensor
				Entity* sensor = World::create<SensorEntity>(manager->team.ref()->team(), pos + rot * Vec3(0, 0, (ROPE_SEGMENT_LENGTH * 2.0f) - ROPE_RADIUS + SENSOR_RADIUS), rot);
				Net::finalize(sensor);

				Audio::post_global_event(AK::EVENTS::PLAY_SENSOR_SPAWN, pos);

				// attach it to the wall
				Rope* rope = Rope::start(parent, pos, rot * Vec3(0, 0, 1), rot);
				rope->end(pos + rot * Vec3(0, 0, ROPE_SEGMENT_LENGTH * 2.0f), rot * Vec3(0, 0, -1), sensor->get<RigidBody>());
				break;
			}
			case Ability::Rocket:
			{
				// spawn a rocket pod
				Net::finalize(World::create<RocketEntity>(entity(), parent->get<Transform>(), pos, rot, get<AIAgent>()->team));

				Audio::post_global_event(AK::EVENTS::PLAY_SENSOR_SPAWN, pos);

				// rocket base
				Entity* base = World::create<Prop>(Asset::Mesh::rocket_base);
				base->get<Transform>()->absolute(pos, rot);
				base->get<Transform>()->reparent(parent->get<Transform>());
				base->get<View>()->team = s8(get<AIAgent>()->team);
				Net::finalize(base);
				break;
			}
			case Ability::Minion:
			{
				// spawn a minion
				Vec3 forward = rot * Vec3(0, 0, 1.0f);
				Vec3 npos = pos + forward;
				forward.y = 0.0f;
				r32 angle;
				if (forward.length_squared() > 0.0f)
					angle = atan2f(forward.x, forward.z);
				else
					angle = get<PlayerCommon>()->angle_horizontal;
				Net::finalize(World::create<Minion>(npos, Quat::euler(0, angle, 0), get<AIAgent>()->team, manager));

				Audio::post_global_event(AK::EVENTS::PLAY_MINION_SPAWN, npos);
				break;
			}
			case Ability::ContainmentField:
			{
				// spawn a containment field
				Vec3 npos = pos + rot * Vec3(0, 0, CONTAINMENT_FIELD_BASE_OFFSET);

				Audio::post_global_event(AK::EVENTS::PLAY_SENSOR_SPAWN, npos);

				Net::finalize(World::create<ContainmentFieldEntity>(parent->get<Transform>(), npos, rot, manager));
				break;
			}
			case Ability::Sniper:
			{
				hit_targets.length = 0;

				Vec3 pos = get<Transform>()->absolute_pos();
				Vec3 ray_start = pos + dir_normalized * -AWK_RADIUS;
				Vec3 ray_end = pos + dir_normalized * range();
				velocity = dir_normalized * AWK_FLY_SPEED;
				r32 distance = movement_raycast(ray_start, ray_end);

				particle_trail(ray_start, dir_normalized, distance);

				// everyone instantly knows where we are
				AI::Team team = get<AIAgent>()->team;
				for (auto i = Team::list.iterator(); !i.is_last(); i.next())
				{
					if (i.item()->team() != team)
						i.item()->track(get<PlayerCommon>()->manager.ref());
				}

				break;
			}
			case Ability::Teleporter:
			{
				Entity* teleporter = World::create<TeleporterEntity>(parent->get<Transform>(), pos, rot, get<AIAgent>()->team);
				Net::finalize(teleporter);
				teleport(entity(), teleporter->get<Teleporter>());
				break;
			}
			case Ability::Decoy:
			{
				Entity* existing_decoy = manager->decoy();
				if (existing_decoy)
					existing_decoy->get<Decoy>()->destroy();
				Net::finalize(World::create<DecoyEntity>(manager, parent->get<Transform>(), pos, rot));
				break;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}

		Ability a = current_ability;
		current_ability = Ability::None;
		ability_spawned.fire(a);
	}

	return true;
}

#define REFLECTION_TRIES 20 // try 20 raycasts. if they all fail, just shoot off into space.
Vec2 reflection_angles[REFLECTION_TRIES] =
{
	Vec2(0.190952f, 0.736239f),
	Vec2(0.189025f, 0.562200f),
	Vec2(0.789319f, 0.662547f),
	Vec2(0.262385f, 0.185272f),
	Vec2(0.129426f, 0.272626f),
	Vec2(0.240692f, 0.235059f),
	Vec2(0.619383f, 0.698697f),
	Vec2(0.501038f, 0.036391f),
	Vec2(0.052299f, 0.005119f),
	Vec2(0.437537f, 0.568719f),
	Vec2(0.079736f, 0.764056f),
	Vec2(0.470466f, 0.241020f),
	Vec2(0.424566f, 0.469034f),
	Vec2(0.188084f, 0.757437f),
	Vec2(0.915419f, 0.271688f),
	Vec2(0.679743f, 0.694464f),
	Vec2(0.693782f, 0.884511f),
	Vec2(0.282588f, 0.337155f),
	Vec2(0.175331f, 0.765014f),
	Vec2(0.994534f, 0.367225f),
};

#define QUANTIZED_REFLECTION_VECTOR_COUNT 18
Vec3 quantized_reflection_vectors[QUANTIZED_REFLECTION_VECTOR_COUNT] =
{
	Vec3(-1, 0, 0),
	Vec3(1, 0, 0),
	Vec3(0, -1, 0),
	Vec3(0, 1, 0),
	Vec3(0, 0, -1),
	Vec3(0, 0, 1),
	Vec3(-0.70710678f, 0, -0.70710678f),
	Vec3(0.70710678f, 0, -0.70710678f),
	Vec3(-0.70710678f, 0, 0.70710678f),
	Vec3(0.70710678f, 0, 0.70710678f),
	Vec3(-0.57735026f, -0.57735026f, -0.57735026f),
	Vec3(0.57735026f, -0.57735026f, -0.57735026f),
	Vec3(-0.57735026f, -0.57735026f, 0.57735026f),
	Vec3(0.57735026f, -0.57735026f, 0.57735026f),
	Vec3(-0.57735026f, 0.57735026f, -0.57735026f),
	Vec3(0.57735026f, 0.57735026f, -0.57735026f),
	Vec3(-0.57735026f, 0.57735026f, 0.57735026f),
	Vec3(0.57735026f, 0.57735026f, 0.57735026f),
};

void Awk::reflect(Entity* entity, const Vec3& original_hit, const Vec3& original_normal, const Net::StateFrame* state_frame)
{
	vi_debug("Original hit: %f %f %f at %f", original_hit.x, original_hit.y, original_hit.z, Game::time.total);

	// it's possible to reflect off a shield while we are dashing (still parented to an object)
	// so we need to make sure we're not dashing anymore
	if (get<Transform>()->parent.ref())
	{
		get<Transform>()->reparent(nullptr);
		dash_timer = 0.0f;
		get<Animator>()->layers[0].animation = Asset::Animation::awk_fly;
	}

	Vec3 hit = original_hit;
	Vec3 normal = original_normal;
	if (entity->has<Awk>())
	{
		// quantize for better server/client synchronization
		Vec3 p;
		if (state_frame)
		{
			Quat rot;
			Net::transform_absolute(*state_frame, entity->get<Transform>()->id(), &p, &rot);
		}
		else
			p = entity->get<Transform>()->absolute_pos();
		r32 closest_dot = 0.0f;
		for (s32 i = 0; i < QUANTIZED_REFLECTION_VECTOR_COUNT; i++)
		{
			if (quantized_reflection_vectors[i].dot(velocity) < 0.0f)
			{
				r32 dot = quantized_reflection_vectors[i].dot(original_normal);
				if (dot > closest_dot)
				{
					closest_dot = dot;
					normal = quantized_reflection_vectors[i];
				}
			}
		}
		hit = p + normal * AWK_SHIELD_RADIUS;
	}

	get<Transform>()->absolute_pos(hit + normal * AWK_RADIUS * 0.5f);

	// our goal
	Vec3 target_dir = Vec3::normalize(velocity.reflect(normal));

	// the actual direction we end up going
	Vec3 new_velocity = target_dir * AWK_DASH_SPEED;

	b8 found_new_velocity = false;

	// first check for nearby targets
	AI::Team team = get<AIAgent>()->team;
	for (auto i = Target::list.iterator(); !i.is_last(); i.next())
	{
		Vec3 intersection;
		if (((i.item()->has<EnergyPickup>() && i.item()->get<EnergyPickup>()->team != team)
			|| (i.item()->has<ContainmentField>() && i.item()->get<ContainmentField>()->team != team)
			|| (i.item()->has<AIAgent>() && i.item()->get<AIAgent>()->team != team))
			&& i.item()->entity() != entity
			&& can_shoot(i.item(), &intersection, AWK_DASH_SPEED, state_frame))
		{
			Vec3 to_target = Vec3::normalize(intersection - get<Transform>()->absolute_pos());
			if (target_dir.dot(to_target) > 0.9f)
			{
				new_velocity = to_target * AWK_DASH_SPEED;
				found_new_velocity = true;
			}
		}
	}

	if (!found_new_velocity)
	{
		// couldn't find a target to hit

		Quat target_quat = Quat::look(target_dir);

		// make sure we have somewhere to land.
		r32 random_range = 0.0f;
		r32 best_score = AWK_MAX_DISTANCE;
		const r32 goal_distance = AWK_MAX_DISTANCE * 0.25f;
		for (s32 i = 0; i < REFLECTION_TRIES; i++)
		{
			const Vec2& angle = reflection_angles[i];
			Vec3 candidate_velocity = target_quat * (Quat::euler(PI + (angle.x - 0.5f) * random_range, (PI * 0.5f) + (angle.y - 0.5f) * random_range, 0) * Vec3(AWK_DASH_SPEED, 0, 0));
			Vec3 next_hit;
			if (can_shoot(candidate_velocity, &next_hit, nullptr, state_frame))
			{
				r32 distance = (next_hit - hit).length();
				r32 score = fabs(distance - goal_distance);

				if (score < best_score)
				{
					new_velocity = candidate_velocity;
					best_score = score;
				}

				if (distance > goal_distance && score < AWK_MAX_DISTANCE * 0.4f)
				{
					new_velocity = candidate_velocity;
					best_score = score;
					break;
				}
			}
			random_range += PI / r32(REFLECTION_TRIES);
		}
	}

	bounce.fire(new_velocity);
	get<Transform>()->rot = Quat::look(Vec3::normalize(new_velocity));
	velocity = new_velocity;
	vi_debug("Hit: %f %f %f Normal: %f %f %f Velocity: %f %f %f", hit.x, hit.y, hit.z, normal.x, normal.y, normal.z, velocity.x, velocity.y, velocity.z);
}

void Awk::crawl_wall_edge(const Vec3& dir, const Vec3& other_wall_normal, const Update& u, r32 speed)
{
	Vec3 wall_normal = get<Transform>()->absolute_rot() * Vec3(0, 0, 1);

	Vec3 orthogonal = wall_normal.cross(other_wall_normal);

	Vec3 dir_flattened = orthogonal * orthogonal.dot(dir);

	r32 dir_flattened_length = dir_flattened.length();
	if (dir_flattened_length > 0.1f)
	{
		dir_flattened /= dir_flattened_length;
		Vec3 pos = get<Transform>()->absolute_pos();
		Vec3 next_pos = pos + dir_flattened * u.time.delta * speed;
		Vec3 wall_ray_start = next_pos + wall_normal * AWK_RADIUS;
		Vec3 wall_ray_end = next_pos + wall_normal * AWK_RADIUS * -2.0f;

		btCollisionWorld::ClosestRayResultCallback ray_callback(wall_ray_start, wall_ray_end);
		Physics::raycast(&ray_callback, ~AWK_INACCESSIBLE_MASK & ~CollisionAwk & ~ally_containment_field_mask());

		if (ray_callback.hasHit())
		{
			// check for obstacles
			btCollisionWorld::ClosestRayResultCallback ray_callback2(pos, next_pos + dir_flattened * AWK_RADIUS);
			Physics::raycast(&ray_callback2, ~AWK_PERMEABLE_MASK & ~CollisionAwk & ~ally_containment_field_mask());
			if (!ray_callback2.hasHit())
			{
				// all good, go ahead
				move
				(
					ray_callback.m_hitPointWorld + ray_callback.m_hitNormalWorld * AWK_RADIUS,
					Quat::look(ray_callback.m_hitNormalWorld),
					ray_callback.m_collisionObject->getUserIndex()
				);
			}
		}
	}
}

// Return true if we actually switched to the other wall
b8 Awk::transfer_wall(const Vec3& dir, const btCollisionWorld::ClosestRayResultCallback& ray_callback)
{
	// Reparent to obstacle/wall
	Vec3 wall_normal = get<Transform>()->absolute_rot() * Vec3(0, 0, 1);
	Vec3 other_wall_normal = ray_callback.m_hitNormalWorld;
	Vec3 dir_flattened_other_wall = dir - other_wall_normal * other_wall_normal.dot(dir);
	// Check to make sure that our movement direction won't get flipped if we switch walls.
	// This prevents jittering back and forth between walls all the time.
	// Also, don't crawl onto inaccessible surfaces.
	if (dir_flattened_other_wall.dot(wall_normal) > 0.0f
		&& !(ray_callback.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup & AWK_INACCESSIBLE_MASK))
	{
		// check for obstacles
		btCollisionWorld::ClosestRayResultCallback obstacle_ray_callback(ray_callback.m_hitPointWorld, ray_callback.m_hitPointWorld + ray_callback.m_hitNormalWorld * (AWK_RADIUS * 1.1f));
		Physics::raycast(&obstacle_ray_callback, ~AWK_PERMEABLE_MASK & ~CollisionAwk & ~ally_containment_field_mask());
		if (!obstacle_ray_callback.hasHit())
		{
			move
			(
				ray_callback.m_hitPointWorld + ray_callback.m_hitNormalWorld * AWK_RADIUS,
				Quat::look(ray_callback.m_hitNormalWorld),
				ray_callback.m_collisionObject->getUserIndex()
			);
			return true;
		}
	}
	return false;
}

void Awk::move(const Vec3& new_pos, const Quat& new_rotation, const ID entity_id)
{
	get<Transform>()->absolute(new_pos, new_rotation);
	Entity* entity = &Entity::list[entity_id];
	if (entity->get<Transform>() != get<Transform>()->parent.ref())
		get<Transform>()->reparent(entity->get<Transform>());
	update_offset();
}

void Awk::crawl(const Vec3& dir_raw, const Update& u)
{
	r32 dir_length = dir_raw.length();

	State s = state();
	if (s != State::Fly && dir_length > 0.0f)
	{
		Vec3 dir_normalized = dir_raw / dir_length;

		r32 speed = s == State::Dash ? AWK_DASH_SPEED : (vi_min(dir_length, 1.0f) * AWK_CRAWL_SPEED);

		Vec3 wall_normal = get<Transform>()->absolute_rot() * Vec3(0, 0, 1);
		Vec3 pos = get<Transform>()->absolute_pos();

		if (dir_normalized.dot(wall_normal) > 0.0f)
		{
			// First, try to climb in the actual direction requested
			Vec3 next_pos = pos + dir_normalized * u.time.delta * speed;
			
			// Check for obstacles
			Vec3 ray_end = next_pos + (dir_normalized * AWK_RADIUS * 1.5f);
			btCollisionWorld::ClosestRayResultCallback ray_callback(pos, ray_end);
			Physics::raycast(&ray_callback, ~AWK_PERMEABLE_MASK & ~CollisionAwk & ~ally_containment_field_mask());

			if (ray_callback.hasHit())
			{
				if (transfer_wall(dir_normalized, ray_callback))
					return;
			}
		}

		Vec3 dir_flattened = dir_normalized - wall_normal * wall_normal.dot(dir_normalized);
		r32 dir_flattened_length = dir_flattened.length();
		if (dir_flattened_length < 0.005f)
			return;

		dir_flattened /= dir_flattened_length;

		Vec3 next_pos = pos + dir_flattened * u.time.delta * speed;

		// Check for obstacles
		{
			Vec3 ray_end = next_pos + (dir_flattened * AWK_RADIUS);
			btCollisionWorld::ClosestRayResultCallback ray_callback(pos, ray_end);
			Physics::raycast(&ray_callback, ~AWK_PERMEABLE_MASK & ~CollisionAwk & ~ally_containment_field_mask());

			if (ray_callback.hasHit())
			{
				if (!transfer_wall(dir_flattened, ray_callback))
				{
					// Stay on our current wall
					crawl_wall_edge(dir_normalized, ray_callback.m_hitNormalWorld, u, speed);
				}
				return;
			}
		}

		// No obstacle. Check if we still have wall to walk on.

		Vec3 wall_ray_start = next_pos + wall_normal * AWK_RADIUS;
		Vec3 wall_ray_end = next_pos + wall_normal * AWK_RADIUS * -2.0f;

		btCollisionWorld::ClosestRayResultCallback ray_callback(wall_ray_start, wall_ray_end);
		Physics::raycast(&ray_callback, ~AWK_PERMEABLE_MASK & ~CollisionAwk & ~ally_containment_field_mask());

		if (ray_callback.hasHit())
		{
			// All good, go ahead

			Vec3 other_wall_normal = ray_callback.m_hitNormalWorld;
			Vec3 dir_flattened_other_wall = dir_normalized - other_wall_normal * other_wall_normal.dot(dir_normalized);
			// Check to make sure that our movement direction won't get flipped if we switch walls.
			// This prevents jittering back and forth between walls all the time.
			if (dir_flattened_other_wall.dot(dir_flattened) > 0.0f
				&& !(ray_callback.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup & AWK_INACCESSIBLE_MASK))
			{
				Vec3 to_next_wall = Vec3(ray_callback.m_hitPointWorld) - attach_point();
				b8 next_wall_curves_away = wall_normal.dot(to_next_wall) < 0.0f;
				r32 dir_flattened_dot = dir_flattened_other_wall.dot(wall_normal);
				if ((next_wall_curves_away && dir_flattened_dot < 0.01f)
					|| (!next_wall_curves_away && dir_flattened_dot > -0.01f))
				{
					move
					(
						ray_callback.m_hitPointWorld + ray_callback.m_hitNormalWorld * AWK_RADIUS,
						Quat::look(ray_callback.m_hitNormalWorld),
						ray_callback.m_collisionObject->getUserIndex()
					);
				}
				else
				{
					// Stay on our current wall
					crawl_wall_edge(dir_normalized, other_wall_normal, u, speed);
				}
			}
		}
		else
		{
			// No wall left
			// See if we can walk around the corner
			Vec3 wall_ray2_start = next_pos + wall_normal * AWK_RADIUS * -1.25f;
			Vec3 wall_ray2_end = wall_ray2_start + dir_flattened * AWK_RADIUS * -2.0f;

			btCollisionWorld::ClosestRayResultCallback ray_callback(wall_ray2_start, wall_ray2_end);
			Physics::raycast(&ray_callback, ~AWK_PERMEABLE_MASK & ~CollisionAwk & ~ally_containment_field_mask());

			if (ray_callback.hasHit())
			{
				// Walk around the corner

				// Check to make sure that our movement direction won't get flipped if we switch walls.
				// This prevents jittering back and forth between walls all the time.
				if (dir_normalized.dot(wall_normal) < 0.05f
					&& !(ray_callback.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup & AWK_INACCESSIBLE_MASK))
				{
					// Transition to the other wall
					move
					(
						ray_callback.m_hitPointWorld + ray_callback.m_hitNormalWorld * AWK_RADIUS,
						Quat::look(ray_callback.m_hitNormalWorld),
						ray_callback.m_collisionObject->getUserIndex()
					);
				}
				else
				{
					// Stay on our current wall
					Vec3 other_wall_normal = Vec3(ray_callback.m_hitNormalWorld);
					crawl_wall_edge(dir_normalized, other_wall_normal, u, speed);
				}
			}
		}
	}
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

void Awk::update_offset()
{
	Quat offset_rot = lerped_rotation;
	Vec3 offset_pos = lerped_pos;
	get<Transform>()->to_local(&offset_pos, &offset_rot);
	get<SkinnedModel>()->offset.rotation(offset_rot);

	if (state() != State::Crawl)
		offset_pos = Vec3::zero;

	get<SkinnedModel>()->offset.translation(offset_pos);
	shield.ref()->get<View>()->offset.translation(offset_pos);
	overshield.ref()->get<View>()->offset.translation(offset_pos);
}

void Awk::stealth(b8 enable)
{
	if (enable != get<AIAgent>()->stealth)
	{
		if (enable)
		{
			get<AIAgent>()->stealth = true;
			get<SkinnedModel>()->alpha_depth();
			get<SkinnedModel>()->mask = 1 << (s32)get<AIAgent>()->team; // only display to fellow teammates
		}
		else
		{
			get<AIAgent>()->stealth = false;
			get<SkinnedModel>()->alpha_disable();
			get<SkinnedModel>()->color.w = MATERIAL_NO_OVERRIDE;
			get<SkinnedModel>()->mask = RENDER_MASK_DEFAULT; // display to everyone
		}
	}
}

void Awk::update_server(const Update& u)
{
	State s = state();

	overshield_timer = vi_max(overshield_timer - u.time.delta, 0.0f);

	if (cooldown > 0.0f)
	{
		cooldown = vi_max(0.0f, cooldown - u.time.delta);
		if (cooldown == 0.0f && Game::level.local)
			charges = AWK_CHARGES;
	}

	if (s != Awk::State::Crawl)
	{
		// flying or dashing
		if (u.time.total - attach_time > MAX_FLIGHT_TIME)
			get<Health>()->kill(entity()); // Kill self

		Vec3 position = get<Transform>()->absolute_pos();
		Vec3 next_position;
		if (s == State::Dash)
		{
			dash_timer -= u.time.delta;
			if (dash_timer <= 0.0f)
			{
				AwkNet::finish_dashing(this);
				return;
			}
			else
			{
				get<Awk>()->crawl(velocity, u);
				next_position = get<Transform>()->absolute_pos();
			}
		}
		else
		{
			next_position = position + velocity * u.time.delta;
			get<Transform>()->absolute_pos(next_position);
		}

		if (!btVector3(velocity).fuzzyZero())
		{
			Vec3 dir = Vec3::normalize(velocity);
			Vec3 ray_start = position + dir * -AWK_RADIUS;
			Vec3 ray_end = next_position + dir * AWK_RADIUS;
			movement_raycast(ray_start, ray_end);
		}
	}
}

void Awk::update_client(const Update& u)
{
	State s = state();

	if (s == Awk::State::Crawl)
	{
		{
			// update lerped pos so we crawl smoothly
			Quat abs_rot;
			Vec3 abs_pos;
			get<Transform>()->absolute(&abs_pos, &abs_rot);
			{
				r32 angle = Quat::angle(lerped_rotation, abs_rot);
				if (angle > 0)
				{
					const r32 tolerance = PI * 0.7f;
					while (angle > tolerance)
					{
						lerped_rotation = Quat::slerp(0.5f / angle, lerped_rotation, abs_rot);
						angle = Quat::angle(lerped_rotation, abs_rot);
					}
					lerped_rotation = Quat::slerp(vi_min(1.0f, (LERP_ROTATION_SPEED / angle) * u.time.delta), lerped_rotation, abs_rot);
				}
			}

			{
				r32 distance = (abs_pos - lerped_pos).length();
				if (distance > 0.0f)
				{
					const r32 tolerance = AWK_SHIELD_RADIUS;
					if (distance < tolerance)
						lerped_pos = Vec3::lerp(vi_min(1.0f, (LERP_TRANSLATION_SPEED / distance) * u.time.delta), lerped_pos, abs_pos);
					else // keep the lerped pos within tolerance of the absolute pos
						lerped_pos = Vec3::lerp(vi_min(1.0f, (tolerance * 0.9f) / distance), lerped_pos, abs_pos);
				}
			}
		}
		update_offset();

		// update footing

		Mat4 inverse_offset = get<SkinnedModel>()->offset.inverse();

		r32 leg_blend_speed = vi_max(AWK_MIN_LEG_BLEND_SPEED, AWK_LEG_BLEND_SPEED * (velocity.length() / AWK_CRAWL_SPEED));

		const Armature* arm = Loader::armature(get<Animator>()->armature);
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
					find_footing_offset = Vec3(vi_max(target_leg_space.x, AWK_LEG_LENGTH * 0.25f), -target_leg_space.y, 0);
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
				btCollisionWorld::ClosestRayResultCallback ray_callback(ray_start, ray_end);
				Physics::raycast(&ray_callback, ~AWK_PERMEABLE_MASK & ~CollisionAwk & ~ally_containment_field_mask());
				if (ray_callback.hasHit())
					set_footing(i, Entity::list[ray_callback.m_collisionObject->getUserIndex()].get<Transform>(), ray_callback.m_hitPointWorld);
				else
				{
					Vec3 new_ray_start = get<Transform>()->to_world((bind_pose_mat * Vec4(AWK_LEG_LENGTH * 1.5f, 0, 0, 1)).xyz());
					Vec3 new_ray_end = get<Transform>()->to_world((bind_pose_mat * Vec4(AWK_LEG_LENGTH * -1.0f, find_footing_offset.y, AWK_LEG_LENGTH * -1.0f, 1)).xyz());
					btCollisionWorld::ClosestRayResultCallback ray_callback(new_ray_start, new_ray_end);
					Physics::raycast(&ray_callback, ~AWK_PERMEABLE_MASK & ~CollisionAwk & ~ally_containment_field_mask());
					if (ray_callback.hasHit())
						set_footing(i, Entity::list[ray_callback.m_collisionObject->getUserIndex()].get<Transform>(), ray_callback.m_hitPointWorld);
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

					footing[i].blend = vi_min(1.0f, footing[i].blend + u.time.delta * leg_blend_speed);
					target_leg_space = Vec3::lerp(footing[i].blend, last_target_leg_space, target_leg_space);
					if (footing[i].blend == 1.0f && Game::real_time.total - last_footstep > 0.07f)
					{
						get<Audio>()->post_event(has<PlayerControlHuman>() && get<PlayerControlHuman>()->local() ? AK::EVENTS::PLAY_AWK_FOOTSTEP_PLAYER : AK::EVENTS::PLAY_AWK_FOOTSTEP);
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

		if (get<Animator>()->layers[0].animation != AssetNull)
		{
			// this means that we were flying or dashing, but we were interrupted. the animation is still playing.
			// this probably happened because the server never started flying or dashing in the first place, while we did
			// and now we're snapping back to the server's state
			finish_flying_dashing_common();
			done_dashing.fire();
		}
	}
	else
	{
		// flying or dashing

		Quat rot;
		Vec3 pos;
		get<Transform>()->absolute(&pos, &rot);

		lerped_pos = pos;
		lerped_rotation = rot;
		update_offset();

		// emit particles
		// but don't start until the awk has cleared the camera radius
		// we do this so that the particles don't block the camera
		r32 particle_start_delay = AWK_THIRD_PERSON_OFFSET / velocity.length();
		if (u.time.total > attach_time + particle_start_delay)
		{
			const r32 particle_interval = 0.05f;
			particle_accumulator += u.time.delta;
			while (particle_accumulator > particle_interval)
			{
				particle_accumulator -= particle_interval;
				Particles::tracers.add
				(
					Vec3::lerp((particle_accumulator - particle_start_delay) / u.time.delta, last_pos, pos),
					Vec3::zero,
					0
				);
			}
		}
	}

	{
		// shield
		View* v = shield.ref()->get<View>();
		if (get<Health>()->shield > 0 || u.time.total - shield_time < AWK_SHIELD_ANIM_TIME)
		{
			if (get<AIAgent>()->stealth)
				v->mask = 1 << (s32)get<AIAgent>()->team; // only display to fellow teammates
			else
				v->mask = RENDER_MASK_DEFAULT; // everyone can see

			r32 blend = vi_min((u.time.total - shield_time) / AWK_SHIELD_ANIM_TIME, 1.0f);
			v->offset = get<SkinnedModel>()->offset;
			if (get<Health>()->shield > 0)
			{
				// shield is coming in; blend from zero to normal size
				blend = Ease::cubic_out<r32>(blend);
				v->color.w = LMath::lerpf(blend, 0.0f, AWK_SHIELD_ALPHA);
				v->offset.make_transform(v->offset.translation(), Vec3(LMath::lerpf(blend, 0.0f, AWK_SHIELD_RADIUS)), Quat::identity);
			}
			else
			{
				// we just lost our shield; blend from normal size to large and faded out
				blend = Ease::cubic_in<r32>(blend);
				v->color.w = LMath::lerpf(blend, 0.75f, 0.0f);
				v->offset.make_transform(v->offset.translation(), Vec3(LMath::lerpf(blend, AWK_SHIELD_RADIUS, 8.0f)), Quat::identity);
			}
		}
		else
			v->mask = 0;
	}

	{
		// overshield
		View* v = overshield.ref()->get<View>();
		if (overshield_timer > 0.0f)
		{
			const r32 anim_time = 0.25f;
			r32 blend;
			if (overshield_timer > AWK_OVERSHIELD_TIME - anim_time)
				blend = Ease::cubic_out<r32>((AWK_OVERSHIELD_TIME - overshield_timer) / anim_time); // fade in from 0 to 1
			else
				blend = Ease::cubic_out<r32>(vi_min(1.0f, overshield_timer / anim_time)); // fade out from 1 to 0
			v->offset.make_transform(v->offset.translation(), Vec3(LMath::lerpf(blend, AWK_SHIELD_RADIUS, AWK_OVERSHIELD_RADIUS)), Quat::identity);
			v->color.w = blend * AWK_OVERSHIELD_ALPHA;
			if (get<AIAgent>()->stealth)
				v->mask = 1 << (s32)get<AIAgent>()->team; // only display to fellow teammates
			else
				v->mask = RENDER_MASK_DEFAULT; // everyone can see
		}
		else
			v->mask = 0;
	}

	// update velocity
	{
		Vec3 pos = lerped_pos;
		if (s == State::Crawl)
			velocity = velocity * 0.9f + ((pos - last_pos) / u.time.delta) * 0.1f;
		last_pos = pos;
	}
}

void Awk::raycast(const Vec3& ray_start, const Vec3& ray_end, const Net::StateFrame* state_frame, Hits* result) const
{
	r32 distance_total = (ray_end - ray_start).length();

	// check environment
	{
		RaycastCallbackExcept ray_callback(ray_start, ray_end, entity());
		Physics::raycast(&ray_callback, (btBroadphaseProxy::StaticFilter | CollisionAllTeamsContainmentField) & ~ally_containment_field_mask());

		if (ray_callback.hasHit())
		{
			b8 inaccessible = ray_callback.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup & (CollisionInaccessible | (CollisionAllTeamsContainmentField & ~ally_containment_field_mask()));
			result->hits.add(
			{
				ray_callback.m_hitPointWorld,
				ray_callback.m_hitNormalWorld,
				(ray_callback.m_hitPointWorld - ray_start).length() / distance_total,
				inaccessible ? Hit::Type::Inaccessible : Hit::Type::Environment,
				&Entity::list[ray_callback.m_collisionObject->getUserIndex()],
			});
		}
	}

	// check targets
	for (auto i = Target::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item() == get<Target>())
			continue;

		Vec3 p;
		if (state_frame)
		{
			Vec3 pos;
			Quat rot;
			Net::transform_absolute(*state_frame, i.item()->get<Transform>()->id(), &pos, &rot);
			p = pos + (rot * i.item()->local_offset); // todo possibly: rewind local_offset as well?
		}
		else
			p = i.item()->absolute_pos();

		Vec3 intersection;
		if (LMath::ray_sphere_intersect(ray_start, ray_end, p, i.item()->radius(), &intersection))
		{
			result->hits.add(
			{
				intersection,
				Vec3::normalize(intersection - p),
				(intersection - ray_start).length() / distance_total,
				i.item()->has<Awk>() ? Hit::Type::Awk : Hit::Type::Target,
				i.item()->entity(),
			});
		}
	}

	// determine which collision is the one we stop at
	result->fraction_end = 2.0f;
	result->index_end = -1;
	for (s32 i = 0; i < result->hits.length; i++)
	{
		const Hit& hit = result->hits[i];
		if (hit.fraction < result->fraction_end)
		{
			b8 stop = false;
			if (hit.type == Hit::Type::Awk)
			{
				if (hit.entity.ref()->get<Awk>()->state() != Awk::State::Crawl) // it's flying or dashing; always bounce off
					stop = true;
				else if (hit.entity.ref()->get<Health>()->total() > 1)
					stop = true; // they have health or shield to spare; we'll bounce off
			}
			else if (hit.type == Hit::Type::Target)
			{
				// go through it, don't stop
			}
			else
				stop = true; // we can't go through it

			if (stop)
			{
				// stop raycast here
				result->fraction_end = hit.fraction;
				result->index_end = i;
			}
		}
	}
}

r32 Awk::movement_raycast(const Vec3& ray_start, const Vec3& ray_end)
{
	State s = state();

	const Net::StateFrame* state_frame = nullptr;
	Net::StateFrame state_frame_data;
	if (awk_state_frame(this, &state_frame_data))
		state_frame = &state_frame_data;

	Hits hits;
	raycast(ray_start, ray_end, state_frame, &hits);

	// handle collisions
	for (s32 i = 0; i < hits.hits.length; i++)
	{
		const Hit& hit = hits.hits[i];
		if (i == hits.index_end || hit.fraction < hits.fraction_end)
		{
			if (hit.type == Hit::Type::Target)
				hit_target(hit.entity.ref());
			else if (hit.type == Hit::Type::Awk)
			{
				b8 do_reflect;
				if (!Game::level.local && has<PlayerControlHuman>() && get<PlayerControlHuman>()->local())
				{
					// client-side prediction
					do_reflect = s != State::Crawl
						&& hit.entity.ref()
						&& hit.entity.ref()->get<Health>()->total() > 1;
					b8 already_hit = false;
					if (do_reflect)
					{
						for (s32 i = 0; i < hit_targets.length; i++)
						{
							if (hit_targets[i].equals(hit.entity))
							{
								already_hit = true;
								do_reflect = false;
								break;
							}
						}
					}
					if (!already_hit)
						hit_targets.add(hit.entity);
				}
				else
				{
					// server
					do_reflect = hit_target(hit.entity.ref()) // go through them if we've already hit them once on this flight
						&& s != State::Crawl
						&& hit.entity.ref()
						&& hit.entity.ref()->get<Health>()->total() > 0; // if we didn't destroy them, then bounce off
				}
				if (do_reflect)
					reflect(hit.entity.ref(), hit.pos, hit.normal, state_frame);
			}
			else if (hit.type == Hit::Type::Inaccessible)
			{
				if (s == State::Fly) // this shouldn't normally happen, but if it does, bounce off
					reflect(hit.entity.ref(), hit.pos, hit.normal, state_frame);
			}
			else if (hit.type == Hit::Type::Environment)
			{
				if (s == State::Fly)
				{
					// we hit a normal surface; attach to it

					Vec3 point = hit.pos;
					// check for obstacles
					{
						btCollisionWorld::ClosestRayResultCallback obstacle_ray_callback(hit.pos, hit.pos + hit.normal * (AWK_RADIUS * 1.1f));
						Physics::raycast(&obstacle_ray_callback, ~AWK_PERMEABLE_MASK & ~CollisionAwk & ~ally_containment_field_mask());
						if (obstacle_ray_callback.hasHit())
						{
							// push us away from the obstacle
							Vec3 obstacle_normal_flattened = obstacle_ray_callback.m_hitNormalWorld - hit.normal * hit.normal.dot(obstacle_ray_callback.m_hitNormalWorld);
							point += obstacle_normal_flattened * AWK_RADIUS;
						}
					}

					get<Transform>()->parent = hit.entity.ref()->get<Transform>();
					get<Transform>()->absolute(point + hit.normal * AWK_RADIUS, Quat::look(hit.normal));

					AwkNet::finish_flying(this);
				}
			}

			if (current_ability == Ability::Sniper
				&& i == hits.index_end
				&& (hit.type == Hit::Type::Environment || hit.type == Hit::Type::Inaccessible))
			{
				// we just shot at a wall; spawn some particles
				Quat rot = Quat::look(hit.normal);
				for (s32 j = 0; j < 50; j++)
				{
					Particles::sparks.add
					(
						hit.pos,
						rot * Vec3(mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo()) * 10.0f,
						Vec4(1, 1, 1, 1)
					);
				}
			}
		}
	}

	return hits.fraction_end * (ray_end - ray_start).length();
}

}
