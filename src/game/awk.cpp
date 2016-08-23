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

namespace VI
{

#define LERP_ROTATION_SPEED 10.0f
#define LERP_TRANSLATION_SPEED 3.0f
#define MAX_FLIGHT_TIME 2.0f
#define AWK_LEG_LENGTH (0.277f - 0.101f)
#define AWK_LEG_BLEND_SPEED (1.0f / 0.05f)
#define AWK_MIN_LEG_BLEND_SPEED (AWK_LEG_BLEND_SPEED * 0.1f)
#define AWK_SHIELD_RADIUS 0.75f
#define AWK_STUN_TIME 1.5f

AwkRaycastCallback::AwkRaycastCallback(const Vec3& a, const Vec3& b, const Entity* awk)
	: btCollisionWorld::ClosestRayResultCallback(a, b)
{
	closest_target_hit = FLT_MAX;
	entity_id = awk->id();
}

b8 AwkRaycastCallback::hit_target() const
{
	return closest_target_hit < FLT_MAX;
}

btScalar AwkRaycastCallback::addSingleResult(btCollisionWorld::LocalRayResult& ray_result, b8 normalInWorldSpace)
{
	short filter_group = ray_result.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup;
	if (filter_group & CollisionWalker)
	{
		Entity* entity = &Entity::list[ray_result.m_collisionObject->getUserIndex()];
		if (entity->has<MinionCommon>() && entity->get<MinionCommon>()->headshot_test(m_rayFromWorld, m_rayToWorld))
		{
			closest_target_hit = ray_result.m_hitFraction;
			return m_closestHitFraction; // keep going
		}
	}
	else if (filter_group & CollisionShield)
	{
		closest_target_hit = ray_result.m_hitFraction;
		return m_closestHitFraction; // keep going
	}
	else if (filter_group & CollisionTarget)
	{
		if (ray_result.m_collisionObject->getUserIndex() != entity_id)
		{
			closest_target_hit = ray_result.m_hitFraction;
			return m_closestHitFraction; // keep going
		}
	}

	m_closestHitFraction = ray_result.m_hitFraction;
	m_collisionObject = ray_result.m_collisionObject;
	if (normalInWorldSpace)
		m_hitNormalWorld = ray_result.m_hitNormalLocal;
	else
	{
		// need to transform normal into worldspace
		m_hitNormalWorld = m_collisionObject->getWorldTransform().getBasis() * ray_result.m_hitNormalLocal;
	}
	m_hitPointWorld.setInterpolate3(m_rayFromWorld, m_rayToWorld, ray_result.m_hitFraction);
	return ray_result.m_hitFraction;
}

Awk::Awk()
	: velocity(0.0f, -AWK_FLY_SPEED, 0.0f),
	done_flying(),
	done_dashing(),
	detached(),
	dashed(),
	dash_timer(),
	attach_time(),
	footing(),
	last_speed(),
	last_footstep(),
	shield(),
	bounce(),
	hit_targets(),
	cooldowns(),
	cooldown_index(),
	stun_timer(),
	invincible_timer(),
	particle_accumulator(),
	snipe()
{
}

void Awk::awake()
{
	link_arg<Entity*, &Awk::killed>(get<Health>()->killed);
	link_arg<const DamageEvent&, &Awk::damaged>(get<Health>()->damaged);
	link_arg<const TargetEvent&, &Awk::hit_by>(get<Target>()->target_hit);
	if (!shield.ref())
	{
		Entity* shield_entity = World::create<Empty>();
		shield_entity->get<Transform>()->parent = get<Transform>();
		shield_entity->add<RigidBody>(RigidBody::Type::Sphere, Vec3(AWK_SHIELD_RADIUS), 0.0f, CollisionShield, CollisionDefault, AssetNull, entity_id);
		shield = shield_entity;

		View* s = shield_entity->add<View>();
		s->alpha_depth();
		s->color = Vec4(1, 1, 1, 0.05f);
		s->mesh = Asset::Mesh::sphere;
		s->offset.scale(Vec3(AWK_SHIELD_RADIUS));
		s->shader = Asset::Shader::flat;
	}
}

Awk::~Awk()
{
	// find all health pickups owned by us and reset them
	Health* health = get<Health>();
	for (auto i = HealthPickup::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->owner.ref() == health)
			i.item()->reset();
	}

	if (shield.ref())
		World::remove_deferred(shield.ref());
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

Vec3 Awk::calculated_velocity() const
{
	if (state() == State::Fly)
		return velocity;
	else
		return (get<Transform>()->absolute_pos() - last_pos) / Game::time.delta;
}

s16 Awk::ally_containment_field_mask() const
{
	return Team::containment_field_mask(get<AIAgent>()->team);
}

Vec3 Awk::center() const
{
	return get<Transform>()->to_world((get<SkinnedModel>()->offset * Vec4(0, 0, 0, 1)).xyz());
}

Vec3 Awk::attach_point() const
{
	Quat rot;
	Vec3 pos;
	get<Transform>()->absolute(&pos, &rot);
	return pos + rot * Vec3(0, 0, -AWK_RADIUS);
}

Entity* Awk::incoming_attacker() const
{
	Vec3 me = center();

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
	get<Audio>()->post_event(has<LocalPlayerControl>() ? AK::EVENTS::PLAY_HURT_PLAYER : AK::EVENTS::PLAY_HURT);

	b8 damaged = false;
	b8 shield_taken_down = false;

	// only take damage if we're attached to a wall and not invincible
	if (state() == Awk::State::Crawl)
	{
		if (invincible_timer == 0.0f)
		{
			// we can take damage
			if (!e.hit_by->has<Awk>() || e.hit_by->get<AIAgent>()->team != get<AIAgent>()->team)
			{
				get<Health>()->damage(e.hit_by, 1);
				damaged = true;
				invincible_timer = AWK_INVINCIBLE_TIME;
			}
		}
		else
		{
			// we're invincible
			shield_taken_down = true;
			invincible_timer = 0.0f; // shield is now down
		}
		if (e.hit_by->has<Awk>() && !e.hit_by->get<Awk>()->snipe) // if they sniped us, they won't get stunned
			e.hit_by->get<Awk>()->stun_timer = AWK_STUN_TIME;
	}

	// let them know they didn't hurt us
	if (!damaged && e.hit_by->has<LocalPlayerControl>())
		e.hit_by->get<LocalPlayerControl>()->player.ref()->msg(_(shield_taken_down ? strings::target_shield_down : strings::no_effect), shield_taken_down);
}

void Awk::hit_target(Entity* target, const Vec3& hit_pos)
{
	for (s32 i = 0; i < hit_targets.length; i++)
	{
		if (hit_targets[i].ref() == target)
			return; // we've already hit this target once during this flight
	}
	if (hit_targets.length < hit_targets.capacity())
		hit_targets.add(target);

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
		World::create<ShockwaveEntity>(8.0f, 1.5f)->get<Transform>()->absolute_pos(hit_pos);
	}

	if (target->has<Target>())
	{
		Ref<Target> t = target->get<Target>();

		// you can't capture health pickups by sniping them
		if (!snipe || !t.ref()->has<HealthPickup>())
			t.ref()->hit(entity());

		if (t.ref() && t.ref()->has<RigidBody>()) // is it still around and does it have a rigidbody?
		{
			RigidBody* body = t.ref()->get<RigidBody>();
			body->btBody->applyImpulse(velocity * 0.1f, Vec3::zero);
			body->btBody->activate(true);
		}
	}

	if (!snipe && invincible_timer > 0.0f && target->has<Awk>())
	{
		invincible_timer = 0.0f; // damaging an Awk takes our shield down
		if (target->has<LocalPlayerControl>()) // let them know they our shield is down
			target->get<LocalPlayerControl>()->player.ref()->msg(_(strings::target_shield_down), true);
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

	hit.fire(target);
}

b8 Awk::predict_intersection(const Target* target, Vec3* intersection) const
{
	Vec3 target_pos = target->absolute_pos();
	if (snipe) // instant bullet travel time
	{
		*intersection = target_pos;
		return true;
	}
	else
	{
		Vec3 target_velocity;
		if (target->has<Awk>())
			target->get<Awk>()->calculated_velocity();
		else
			target_velocity = target->get<RigidBody>()->btBody->getInterpolationLinearVelocity();
		Vec3 to_target = target_pos - get<Transform>()->absolute_pos();
		r32 intersect_time_squared = to_target.dot(to_target) / ((AWK_FLY_SPEED * AWK_FLY_SPEED) - 2.0f * to_target.dot(target_velocity) - target_velocity.dot(target_velocity));
		if (intersect_time_squared > 0.0f)
		{
			*intersection = target_pos + target_velocity * sqrtf(intersect_time_squared);
			return true;
		}
		else
			return false;
	}
}

void Awk::damaged(const DamageEvent& e)
{
	if (e.damager)
	{
		if (get<Health>()->hp > 0 && e.damager->has<LocalPlayerControl>())
			e.damager->get<LocalPlayerControl>()->player.ref()->msg(_(strings::target_damaged), true);
	}
	s32 new_health_pickup_count = get<Health>()->hp - 1;
	s32 health_pickup_count = 0;
	for (auto i = HealthPickup::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->owner.ref() == get<Health>())
			health_pickup_count++;
	}

	if (health_pickup_count > 0)
	{
		// reset health pickups that belong to us
		// starting with the farthest first

		Array<Ref<HealthPickup>> farthest_owned_health_pickups;
		HealthPickup::sort_all(get<Transform>()->absolute_pos(), &farthest_owned_health_pickups, false, get<Health>());

		for (s32 i = 0; i < farthest_owned_health_pickups.length && new_health_pickup_count < health_pickup_count; i++)
		{
			farthest_owned_health_pickups[i].ref()->reset();
			new_health_pickup_count--;
		}
	}
}

void Awk::killed(Entity* e)
{
	for (auto i = HealthPickup::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->owner.ref() == get<Health>())
			i.item()->reset();
	}
	get<Audio>()->post_event(AK::EVENTS::STOP_FLY);
	World::remove_deferred(entity());
}

b8 Awk::can_hit(const Target* target, Vec3* out_intersection) const
{
	Vec3 intersection;
	if (predict_intersection(target, &intersection))
	{
		Vec3 me = center();
		Vec3 to_intersection = intersection - me;
		r32 distance = to_intersection.length();
		to_intersection /= distance;
		if (distance < AWK_DASH_DISTANCE
			&& fabs(to_intersection.dot(get<Transform>()->absolute_rot() * Vec3(0, 0, 1))) < 0.1f)
		{
			if (out_intersection)
				*out_intersection = intersection;
			return true;
		}
		Vec3 final_pos;
		b8 hit_target;
		if (can_shoot(to_intersection, &final_pos, &hit_target))
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

b8 Awk::direction_is_toward_attached_wall(const Vec3& dir) const
{
	Vec3 wall_normal = get<Transform>()->absolute_rot() * Vec3(0, 0, 1);
	return dir.dot(wall_normal) < 0.0f;
}

b8 Awk::can_shoot(const Vec3& dir, Vec3* final_pos, b8* hit_target) const
{
	Vec3 trace_dir = Vec3::normalize(dir);

	if (fabs(trace_dir.y) > AWK_VERTICAL_DOT_LIMIT) // can't shoot straight up or straight down
		return false;

	// if we're attached to a wall, make sure we're not shooting into the wall
	if (state() == Awk::State::Crawl)
	{
		if (direction_is_toward_attached_wall(trace_dir))
			return false;
	}

	Vec3 trace_start = center();
	Vec3 trace_end = trace_start + trace_dir * range();

	AwkRaycastCallback ray_callback(trace_start, trace_end, entity());
	Physics::raycast(&ray_callback, ~CollisionAwkIgnore & ~ally_containment_field_mask());

	if (snipe)
	{
		if (ray_callback.hasHit())
		{
			if (final_pos)
				*final_pos = ray_callback.m_hitPointWorld;
			if (hit_target)
				*hit_target = ray_callback.hit_target();
		}
		else
		{
			if (final_pos)
				*final_pos = trace_end;
			if (hit_target)
				*hit_target = false;
		}
		return true; // we can always snipe, even if the bullet goes into space
	}
	else
	{
		if (ray_callback.hasHit() && !(ray_callback.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup & (AWK_INACCESSIBLE_MASK & ~CollisionShield)))
		{
			if (final_pos)
				*final_pos = ray_callback.m_hitPointWorld;
			if (hit_target)
				*hit_target = ray_callback.hit_target();
			return true;
		}
		else
			return false;
	}
}

void Awk::cooldown_setup(r32 value)
{
	cooldown_index = AWK_CHARGES;
	for (s32 i = 0; i < AWK_CHARGES; i++)
	{
		if (cooldowns[i] == 0.0f)
		{
			cooldown_index = i;
			break;
		}
	}
	vi_assert(cooldown_index < AWK_CHARGES);
	cooldowns[cooldown_index] = value;
}

void Awk::detach_teleport()
{
	hit_targets.length = 0;

	attach_time = Game::time.total;

	cooldown_setup();

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
	if (state() != State::Crawl || snipe)
		return false;

	if (!direction_is_toward_attached_wall(dir))
		return false;

	velocity = Vec3::normalize(dir) * AWK_DASH_SPEED;
	dash_timer = AWK_DASH_TIME;

	hit_targets.length = 0;

	attach_time = Game::time.total;
	cooldown_setup();

	for (s32 i = 0; i < AWK_LEGS; i++)
		footing[i].parent = nullptr;
	get<Animator>()->reset_overrides();
	get<Animator>()->layers[0].animation = Asset::Animation::awk_dash;

	particle_accumulator = 0;

	get<Audio>()->post_event(has<LocalPlayerControl>() ? AK::EVENTS::PLAY_LAUNCH_PLAYER : AK::EVENTS::PLAY_LAUNCH);

	dashed.fire();

	return true;
}

s32 Awk::charges() const
{
	s32 count = 0;
	for (s32 i = 0; i < AWK_CHARGES; i++)
	{
		if (cooldowns[i] == 0.0f)
			count++;
	}
	return count;
}

b8 Awk::cooldown_can_shoot() const
{
	return charges() > 0;
}

#define SNIPE_RANGE_TIME 0.5f

r32 Awk::range() const
{
	if (snipe)
		return Ease::cubic_out(vi_min(1.0f, (Game::time.total - snipe_time) / SNIPE_RANGE_TIME), AWK_MAX_DISTANCE, AWK_SNIPE_DISTANCE);
	else
		return AWK_MAX_DISTANCE;
}

void Awk::snipe_start()
{
	snipe = true;
	snipe_time = Game::time.total;
}

b8 Awk::detach(const Vec3& dir)
{
	if (cooldown_can_shoot() && stun_timer == 0.0f)
	{
		Vec3 dir_normalized = Vec3::normalize(dir);
		if (snipe)
		{
			hit_targets.length = 0;

			for (s32 i = 0; i < AWK_CHARGES; i++)
				cooldowns[i] = vi_max(cooldowns[i], AWK_MAX_DISTANCE_COOLDOWN * 0.25f);

			Vec3 pos = center();
			Vec3 ray_start = pos + dir_normalized * -AWK_RADIUS;
			Vec3 ray_end = pos + dir_normalized * range();
			velocity = dir_normalized * AWK_FLY_SPEED;
			movement_raycast(ray_start, ray_end);

			const r32 interval = 2.0f;
			s32 particle_count = range() / interval;
			Vec3 interval_pos = dir_normalized * interval;
			for (s32 i = 0; i < particle_count; i++)
			{
				pos += interval_pos;
				Particles::tracers.add
				(
					pos,
					Vec3::zero,
					0
				);
			}

			// everyone instantly knows where we are
			AI::Team team = get<AIAgent>()->team;
			for (s32 i = 0; i < Team::list.length; i++)
			{
				if (Team::list[i].team() != team)
					Team::list[i].track(get<PlayerCommon>()->manager.ref());
			}

			snipe = false;
		}
		else
		{
			velocity = dir_normalized * AWK_FLY_SPEED;
			get<Transform>()->absolute_pos(center() + dir_normalized * AWK_RADIUS * 0.5f);
			get<Transform>()->absolute_rot(Quat::look(dir_normalized));

			get<Audio>()->post_event(has<LocalPlayerControl>() ? AK::EVENTS::PLAY_LAUNCH_PLAYER : AK::EVENTS::PLAY_LAUNCH);

			detach_teleport();
		}

		return true;
	}
	return false;
}

void Awk::reflect(const Vec3& hit, const Vec3& normal)
{
	// it's possible to reflect off a shield while we are dashing (still parented to an object)
	// so we need to make sure we're not dashing anymore
	get<Transform>()->reparent(nullptr);
	dash_timer = 0.0f;
	get<Animator>()->layers[0].animation = Asset::Animation::awk_fly;

	// our goal
	Vec3 target_velocity = velocity.reflect(normal);

	// the actual direction we end up going
	Vec3 new_velocity = target_velocity;

	get<Transform>()->absolute_pos(hit + normal * AWK_RADIUS);

	r32 target_speed = target_velocity.length();
	Quat target_quat = Quat::look(target_velocity / target_speed);

	// make sure we have somewhere to land.
	const s32 tries = 20; // try 20 raycasts. if they all fail, just shoot off into space.
	r32 random_range = 0.0f;
	r32 farthest_distance = 0;
	for (s32 i = 0; i < tries; i++)
	{
		Vec3 candidate_velocity = target_quat * (Quat::euler(PI + (mersenne::randf_co() - 0.5f) * random_range, (PI * 0.5f) + (mersenne::randf_co() - 0.5f) * random_range, 0) * Vec3(target_speed, 0, 0));
		Vec3 next_hit;
		if (can_shoot(candidate_velocity, &next_hit))
		{
			r32 distance_to_next_hit = (next_hit - hit).length_squared();
			if (distance_to_next_hit > farthest_distance)
			{
				new_velocity = candidate_velocity;
				farthest_distance = distance_to_next_hit;
			}

			if (distance_to_next_hit > (AWK_MAX_DISTANCE * 0.5f * AWK_MAX_DISTANCE * 0.5f)) // try to bounce to a spot at least X units away
			{
				new_velocity = candidate_velocity;
				break;
			}
		}
		random_range += PI / (r32)tries;
	}

	bounce.fire(new_velocity);
	get<Transform>()->rot = Quat::look(Vec3::normalize(new_velocity));
	velocity = new_velocity;
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
	if ((new_pos - get<Transform>()->absolute_pos()).length() > 5.0f)
		vi_debug_break();
	lerped_rotation = new_rotation.inverse() * get<Transform>()->absolute_rot() * lerped_rotation;
	get<Transform>()->absolute(new_pos, new_rotation);
	Entity* entity = &Entity::list[entity_id];
	if (entity->get<Transform>() != get<Transform>()->parent.ref())
	{
		if (state() == State::Crawl)
		{
			Vec3 abs_lerped_pos = get<Transform>()->parent.ref()->to_world(lerped_pos);
			lerped_pos = entity->get<Transform>()->to_local(abs_lerped_pos);
		}
		else
			lerped_pos = entity->get<Transform>()->to_local(get<Transform>()->pos);
		get<Transform>()->reparent(entity->get<Transform>());
	}
	update_offset();
}

void Awk::crawl(const Vec3& dir_raw, const Update& u)
{
	r32 dir_length = dir_raw.length();

	State s = state();
	if (s != State::Fly && dir_length > 0.0f && stun_timer == 0.0f)
	{
		Vec3 dir_normalized = dir_raw / dir_length;

		r32 speed = last_speed = s == State::Dash ? AWK_DASH_SPEED : (vi_min(dir_length, 1.0f) * AWK_CRAWL_SPEED);

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
	get<SkinnedModel>()->offset.rotation(lerped_rotation);
	if (state() == State::Crawl)
	{
		Vec3 abs_lerped_pos = get<Transform>()->parent.ref()->to_world(lerped_pos);
		get<SkinnedModel>()->offset.translation(get<Transform>()->to_local(abs_lerped_pos));
	}
	else
		get<SkinnedModel>()->offset.translation(Vec3::zero);
}

void Awk::stealth(b8 enable)
{
	if (enable != get<AIAgent>()->stealth)
	{
		if (enable)
		{
			get<AIAgent>()->stealth = true;
			get<SkinnedModel>()->alpha_depth();
			get<SkinnedModel>()->color.w = 0.05f;
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

void Awk::finish_flying_dashing_common()
{
	lerped_pos = get<Transform>()->pos;

	get<Animator>()->layers[0].animation = AssetNull;

	get<Audio>()->post_event(has<LocalPlayerControl>() ? AK::EVENTS::PLAY_LAND_PLAYER : AK::EVENTS::PLAY_LAND);
	attach_time = Game::time.total;

	velocity = Vec3::zero;
}

void Awk::finish_flying()
{
	finish_flying_dashing_common();
	done_flying.fire();
}

void Awk::finish_dashing()
{
	finish_flying_dashing_common();
	done_dashing.fire();
}

void Awk::update_lerped_pos(r32 speed_multiplier, const Update& u)
{
	{
		r32 angle = Quat::angle(lerped_rotation, Quat::identity);
		if (angle > 0)
			lerped_rotation = Quat::slerp(vi_min(1.0f, (speed_multiplier * LERP_ROTATION_SPEED / angle) * u.time.delta), lerped_rotation, Quat::identity);
	}

	{
		Vec3 to_transform = get<Transform>()->pos - lerped_pos;
		r32 distance = to_transform.length();
		if (distance > 0.0f)
			lerped_pos = Vec3::lerp(vi_min(1.0f, (speed_multiplier * LERP_TRANSLATION_SPEED / distance) * u.time.delta), lerped_pos, get<Transform>()->pos);
	}
}

void Awk::update(const Update& u)
{
	last_pos = get<Transform>()->absolute_pos();

	stun_timer = vi_max(stun_timer - u.time.delta, 0.0f);

	State s = state();

	invincible_timer = vi_max(invincible_timer - u.time.delta, 0.0f);
	if (invincible_timer > 0.0f || s != Awk::State::Crawl)
	{
		if (get<AIAgent>()->stealth)
			shield.ref()->get<View>()->mask = 1 << (s32)get<AIAgent>()->team; // only display to fellow teammates
		else
			shield.ref()->get<View>()->mask = RENDER_MASK_DEFAULT; // everyone can see
	}
	else
		shield.ref()->get<View>()->mask = 0;

	if (s == Awk::State::Crawl)
	{
		if (stun_timer == 0.0f)
		{
			for (s32 i = 0; i < AWK_CHARGES; i++)
				cooldowns[i] = vi_max(0.0f, cooldowns[i] - u.time.delta);
		}

		update_lerped_pos(1.0f, u);
		update_offset();

		// update footing

		Mat4 inverse_offset = get<SkinnedModel>()->offset.inverse();

		r32 leg_blend_speed = vi_max(AWK_MIN_LEG_BLEND_SPEED, AWK_LEG_BLEND_SPEED * (last_speed / AWK_CRAWL_SPEED));
		last_speed = 0.0f;

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
		// flying or dashing

		if (attach_time > 0.0f && u.time.total - attach_time > MAX_FLIGHT_TIME)
			get<Health>()->damage(entity(), AWK_HEALTH); // Kill self
		else
		{
			Vec3 position = get<Transform>()->absolute_pos();
			Vec3 next_position;
			if (s == State::Dash)
			{
				dash_timer -= u.time.delta;
				if (dash_timer <= 0.0f)
				{
					finish_dashing();
					return;
				}
				else
				{
					get<Awk>()->crawl(velocity, u);
					next_position = get<Transform>()->absolute_pos();
					update_lerped_pos(5.0f, u);
					update_offset();
				}
			}
			else
			{
				next_position = position + velocity * u.time.delta;
				get<Transform>()->absolute_pos(next_position);
			}

			// emit particles
			{
				const r32 particle_interval = 0.05f;
				particle_accumulator += u.time.delta;
				while (particle_accumulator > particle_interval)
				{
					particle_accumulator -= particle_interval;
					Particles::tracers.add
					(
						Vec3::lerp(particle_accumulator / u.time.delta, position, next_position),
						Vec3::zero,
						0
					);
				}
			}

			if (!btVector3(velocity).fuzzyZero())
			{
				Vec3 dir = Vec3::normalize(velocity);
				Vec3 ray_start = position + dir * -AWK_RADIUS;
				Vec3 ray_end = next_position + dir * AWK_RADIUS;
				movement_raycast(ray_start, ray_end);
			}
			cooldowns[cooldown_index] = vi_min(cooldowns[cooldown_index] + (next_position - position).length() * AWK_COOLDOWN_DISTANCE_RATIO, AWK_MAX_DISTANCE_COOLDOWN);
		}
	}
}

void Awk::movement_raycast(const Vec3& ray_start, const Vec3& ray_end)
{
	btCollisionWorld::AllHitsRayResultCallback ray_callback(ray_start, ray_end);
	ray_callback.m_flags = btTriangleRaycastCallback::EFlags::kF_FilterBackfaces
		| btTriangleRaycastCallback::EFlags::kF_KeepUnflippedNormal;
	ray_callback.m_collisionFilterMask = ray_callback.m_collisionFilterGroup = btBroadphaseProxy::AllFilter;

	Physics::btWorld->rayTest(ray_start, ray_end, ray_callback);

	// determine which ray collision is the one we stop at
	r32 fraction_end = 2.0f;
	s32 index_end = -1;
	for (s32 i = 0; i < ray_callback.m_collisionObjects.size(); i++)
	{
		if (ray_callback.m_hitFractions[i] < fraction_end)
		{
			short group = ray_callback.m_collisionObjects[i]->getBroadphaseHandle()->m_collisionFilterGroup;
			Entity* entity = &Entity::list[ray_callback.m_collisionObjects[i]->getUserIndex()];

			b8 stop = false;
			if ((entity->has<Awk>() && (group & CollisionShield))) // it's an AWK shield
			{
				if (entity->get<Awk>()->state() != Awk::State::Crawl) // it's flying or dashing; always bounce off
					stop = true;
				else if (entity->get<Health>()->hp > 1)
					stop = true; // they have shield to spare; we'll bounce off the shield
			}
			else if (!(group & (AWK_PERMEABLE_MASK | CollisionWalker | ally_containment_field_mask())))
			{
				stop = true; // we can't go through it
				if (snipe) // we just shot at a wall; spawn some particles
				{
					Quat rot = Quat::look(ray_callback.m_hitNormalWorld[i]);
					for (s32 j = 0; j < 50; j++)
					{
						Particles::sparks.add
						(
							ray_callback.m_hitPointWorld[i],
							rot * Vec3(mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo()) * 10.0f,
							Vec4(1, 1, 1, 1)
						);
					}
				}
			}

			if (stop)
			{
				// stop raycast here
				fraction_end = ray_callback.m_hitFractions[i];
				index_end = i;
			}
		}
	}

	State s = state();

	for (s32 i = 0; i < ray_callback.m_collisionObjects.size(); i++)
	{
		if (i == index_end || ray_callback.m_hitFractions[i] < fraction_end)
		{
			s16 group = ray_callback.m_collisionObjects[i]->getBroadphaseHandle()->m_collisionFilterGroup;
			if (group & CollisionWalker)
			{
				Entity* t = &Entity::list[ray_callback.m_collisionObjects[i]->getUserIndex()];
				if (t->has<MinionCommon>() && t->get<MinionCommon>()->headshot_test(ray_start, ray_end))
					hit_target(t, t->get<MinionCommon>()->head_pos());
			}
			else if (group & (CollisionInaccessible | (CollisionAllTeamsContainmentField & ~ally_containment_field_mask())))
			{
				// this shouldn't happen, but if it does, bounce off
				if (s == State::Fly)
					reflect(ray_callback.m_hitPointWorld[i], ray_callback.m_hitNormalWorld[i]);
			}
			else if (group & (CollisionTarget | CollisionShield))
			{
				Ref<Entity> hit = &Entity::list[ray_callback.m_collisionObjects[i]->getUserIndex()];
				if (hit.ref() != entity())
				{
					hit_target(hit.ref(), hit.ref()->get<Transform>()->absolute_pos());
					if (group & CollisionShield)
					{
						// if we didn't destroy the shield, then bounce off it
						if (s != State::Crawl && hit.ref() && hit.ref()->get<Health>()->hp > 0)
							reflect(ray_callback.m_hitPointWorld[i], ray_callback.m_hitNormalWorld[i]);
					}
				}
			}
			else if (group & (CollisionAllTeamsContainmentField | CollisionAwkIgnore))
			{
				// ignore
			}
			else if (s == State::Fly)
			{
				// we hit a normal surface; attach to it
				Vec3 point = ray_callback.m_hitPointWorld[i];
				Vec3 normal = ray_callback.m_hitNormalWorld[i];

				// check for obstacles
				{
					btCollisionWorld::ClosestRayResultCallback obstacle_ray_callback(point, point + normal * (AWK_RADIUS * 1.1f));
					Physics::raycast(&obstacle_ray_callback, ~AWK_PERMEABLE_MASK & ~CollisionAwk & ~ally_containment_field_mask());
					if (obstacle_ray_callback.hasHit())
					{
						// push us away from the obstacle
						Vec3 obstacle_normal_flattened = obstacle_ray_callback.m_hitNormalWorld - normal * normal.dot(obstacle_ray_callback.m_hitNormalWorld);
						point += obstacle_normal_flattened * AWK_RADIUS;
					}
				}

				Entity* entity = &Entity::list[ray_callback.m_collisionObjects[i]->getUserIndex()];
				get<Transform>()->parent = entity->get<Transform>();
				get<Transform>()->absolute(point + normal * AWK_RADIUS, Quat::look(ray_callback.m_hitNormalWorld[i]));

				finish_flying();
			}
		}
	}
}

}