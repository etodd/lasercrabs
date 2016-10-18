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

namespace VI
{

#define LERP_ROTATION_SPEED 10.0f
#define LERP_TRANSLATION_SPEED 3.0f
#define MAX_FLIGHT_TIME 6.0f
#define AWK_LEG_LENGTH (0.277f - 0.101f)
#define AWK_LEG_BLEND_SPEED (1.0f / 0.03f)
#define AWK_MIN_LEG_BLEND_SPEED (AWK_LEG_BLEND_SPEED * 0.1f)

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
	dash_timer(),
	attach_time(Game::time.total),
	footing(),
	last_speed(),
	last_footstep(),
	shield(),
	bounce(),
	hit_targets(),
	cooldown(),
	charges(AWK_CHARGES),
	invincible_timer(AWK_INVINCIBLE_TIME),
	particle_accumulator(),
	current_ability(Ability::None),
	ability_spawned()
{
}

void Awk::awake()
{
	link_arg<Entity*, &Awk::killed>(get<Health>()->killed);
	link_arg<const DamageEvent&, &Awk::damaged>(get<Health>()->damaged);
	link_arg<const TargetEvent&, &Awk::hit_by>(get<Target>()->target_hit);
	if (Game::session.local && !shield.ref())
	{
		Entity* shield_entity = World::create<Empty>();
		shield_entity->get<Transform>()->parent = get<Transform>();
		shield_entity->add<RigidBody>(RigidBody::Type::Sphere, Vec3(AWK_SHIELD_RADIUS), 0.0f, CollisionShield, CollisionDefault, AssetNull, entity_id);
		shield = shield_entity;

		View* s = shield_entity->add<View>();
		s->team = (u8)get<AIAgent>()->team;
		s->mesh = Asset::Mesh::sphere_highres;
		s->offset.scale(Vec3(AWK_SHIELD_RADIUS));
		s->shader = Asset::Shader::fresnel;
		s->alpha();
		s->color.w = 0.35f;

		Net::finalize(shield_entity);
	}
}

Awk::~Awk()
{
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

s16 Awk::ally_containment_field_mask() const
{
	return Team::containment_field_mask(get<AIAgent>()->team);
}

Vec3 Awk::center_lerped() const
{
	return get<Transform>()->to_world((get<SkinnedModel>()->offset * Vec4(0, 0, 0, 1)).xyz());
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
	get<Audio>()->post_event(has<PlayerControlHuman>() ? AK::EVENTS::PLAY_HURT_PLAYER : AK::EVENTS::PLAY_HURT);

	b8 damaged = false;

	// only take damage if we're attached to a wall and not invincible
	if (state() == Awk::State::Crawl
		&& invincible_timer == 0.0f
		&& (!e.hit_by->has<Awk>() || e.hit_by->get<AIAgent>()->team != get<AIAgent>()->team))
	{
		get<Health>()->damage(e.hit_by, 1);
		damaged = true;
	}

	// let them know they didn't hurt us
	if (!damaged && e.hit_by->has<PlayerControlHuman>())
		e.hit_by->get<PlayerControlHuman>()->player.ref()->msg(_(strings::no_effect), false);
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

	if (current_ability == Ability::None && invincible_timer > 0.0f && target->has<Awk>())
		invincible_timer = 0.0f; // damaging an Awk takes our shield down

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

void Awk::damaged(const DamageEvent& e)
{
	if (get<Health>()->hp == 0)
	{
		// killed; notify everyone
		if (e.damager)
		{
			PlayerCommon* enemy = nullptr;
			if (e.damager->has<PlayerCommon>())
				enemy = e.damager->get<PlayerCommon>();
			else if (e.damager->has<Projectile>())
			{
				Entity* owner = e.damager->get<Projectile>()->owner.ref();
				if (owner)
					enemy = owner->get<PlayerCommon>();
			}
			else if (e.damager->has<Rocket>())
			{
				Entity* owner = e.damager->get<Rocket>()->owner.ref();
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
			if (i.item()->manager.ref() != manager) // don't need to notify ourselves
			{
				b8 friendly = i.item()->manager.ref()->team.ref()->team() == team;
				char buffer[512];
				sprintf(buffer, _(strings::player_killed), manager->username);
				i.item()->msg(buffer, !friendly);
			}
		}
	}
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

b8 Awk::can_shoot(const Target* target, Vec3* out_intersection, r32 speed) const
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

b8 Awk::can_shoot(const Vec3& dir, Vec3* final_pos, b8* hit_target) const
{
	Vec3 trace_dir = Vec3::normalize(dir);

	// if we're attached to a wall, make sure we're not shooting into the wall
	State s = state();
	if (s == Awk::State::Crawl)
	{
		if (direction_is_toward_attached_wall(trace_dir))
			return false;
	}

	// can't shoot straight up or straight down
	// HACK: if it's a local player, let them do what they want because it's frustrating
	// in certain cases where the drone won't let you go where you should be able to go
	// due to the third-person camera offset
	// the AI however needs to know whether it can hit actually hit a target
	if (!has<PlayerControlHuman>() && fabs(trace_dir.y) > AWK_VERTICAL_DOT_LIMIT)
		return false;

	Vec3 trace_start = get<Transform>()->absolute_pos();
	Vec3 trace_end = trace_start + trace_dir * AWK_SNIPE_DISTANCE;

	AwkRaycastCallback ray_callback(trace_start, trace_end, entity());
	Physics::raycast(&ray_callback, ~CollisionAwkIgnore & ~ally_containment_field_mask());

	if (ray_callback.hasHit()
		&& !(ray_callback.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup & (AWK_INACCESSIBLE_MASK & ~CollisionShield)))
	{
		r32 r = range();
		b8 can_shoot = false;
		if (ray_callback.m_closestHitFraction * AWK_SNIPE_DISTANCE < r)
			can_shoot = true;
		else if ((ray_callback.closest_target_hit_group & (CollisionAwk | CollisionShield))
			&& ray_callback.closest_target_hit_fraction * AWK_SNIPE_DISTANCE < r)
		{
			can_shoot = true; // allow awk to shoot if we're aiming at an enemy awk in range but the backing behind it is out of range
		}
		else
		{
			// check target predictions
			for (auto i = list.iterator(); !i.is_last(); i.next())
			{
				if (i.item() != this && (i.item()->get<Transform>()->absolute_pos() - trace_start).length_squared() > AWK_SHIELD_RADIUS * 2.0f * AWK_SHIELD_RADIUS * 2.0f)
				{
					Vec3 intersection;
					if (predict_intersection(i.item()->get<Target>(), &intersection))
					{
						if ((intersection - trace_start).length_squared() < r * r
							&& LMath::ray_sphere_intersect(trace_start, trace_end, intersection, AWK_SHIELD_RADIUS))
						{
							can_shoot = true;
							break;
						}
					}
				}
			}
		}

		if (can_shoot)
		{
			if (final_pos)
				*final_pos = ray_callback.m_hitPointWorld;
			if (hit_target)
				*hit_target = ray_callback.hit_target();
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

	AwkRaycastCallback ray_callback(trace_start, trace_end, entity());
	Physics::raycast(&ray_callback, ~CollisionAwkIgnore & ~ally_containment_field_mask());

	if (a == Ability::Sniper)
	{
		if (ray_callback.hasHit())
		{
			if (final_pos)
				*final_pos = ray_callback.m_hitPointWorld;
			if (hit_target)
				*hit_target = ray_callback.hit_target();
			if (hit_parent)
				*hit_parent = Entity::list[ray_callback.m_collisionObject->getUserIndex()].get<RigidBody>();
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
	vi_assert(charges > 0);
	charges--;
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
	if (state() != State::Crawl || current_ability == Ability::Sniper)
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

	get<Audio>()->post_event(has<PlayerControlHuman>() ? AK::EVENTS::PLAY_LAUNCH_PLAYER : AK::EVENTS::PLAY_LAUNCH);

	dashed.fire();

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
	{
		velocity = dir_normalized * AWK_FLY_SPEED;
		get<Transform>()->absolute_pos(get<Transform>()->absolute_pos() + dir_normalized * AWK_RADIUS * 0.5f);
		get<Transform>()->absolute_rot(Quat::look(dir_normalized));

		get<Audio>()->post_event(has<PlayerControlHuman>() ? AK::EVENTS::PLAY_LAUNCH_PLAYER : AK::EVENTS::PLAY_LAUNCH);

		cooldown_setup();
		detach_teleport();
	}
	else
	{
		// ability spawn

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
				Entity* sensor = World::create<SensorEntity>(manager, pos + rot * Vec3(0, 0, (rope_segment_length * 2.0f) - rope_radius + SENSOR_RADIUS), rot);
				Net::finalize(sensor);

				Audio::post_global_event(AK::EVENTS::PLAY_SENSOR_SPAWN, pos);

				// attach it to the wall
				Rope* rope = Rope::start(parent, pos, rot * Vec3(0, 0, 1), rot);
				rope->end(pos + rot * Vec3(0, 0, rope_segment_length * 2.0f), rot * Vec3(0, 0, -1), sensor->get<RigidBody>());
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
				base->get<View>()->team = u8(get<AIAgent>()->team);
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

void Awk::reflect(const Vec3& hit, const Vec3& normal)
{
	if (btVector3(velocity).fuzzyZero())
		return;

	// it's possible to reflect off a shield while we are dashing (still parented to an object)
	// so we need to make sure we're not dashing anymore
	if (get<Transform>()->parent.ref())
	{
		get<Transform>()->reparent(nullptr);
		dash_timer = 0.0f;
		get<Animator>()->layers[0].animation = Asset::Animation::awk_fly;
	}

	get<Transform>()->absolute_pos(hit + normal * AWK_RADIUS);

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
			&& can_shoot(i.item(), &intersection, AWK_DASH_SPEED))
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
		const s32 tries = 20; // try 20 raycasts. if they all fail, just shoot off into space.
		r32 random_range = 0.0f;
		r32 farthest_distance = 0;
		for (s32 i = 0; i < tries; i++)
		{
			Vec3 candidate_velocity = target_quat * (Quat::euler(PI + (mersenne::randf_co() - 0.5f) * random_range, (PI * 0.5f) + (mersenne::randf_co() - 0.5f) * random_range, 0) * Vec3(AWK_DASH_SPEED, 0, 0));
			Vec3 next_hit;
			if (can_shoot(candidate_velocity, &next_hit))
			{
				r32 distance_to_next_hit = (next_hit - hit).length_squared();
				if (distance_to_next_hit > farthest_distance)
				{
					new_velocity = candidate_velocity;
					farthest_distance = distance_to_next_hit;

					if (distance_to_next_hit > (AWK_MAX_DISTANCE * 0.5f * AWK_MAX_DISTANCE * 0.5f)) // try to bounce to a spot at least X units away
						break;
				}
			}
			random_range += PI / (r32)tries;
		}
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
	if (s != State::Fly && dir_length > 0.0f)
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

	get<Audio>()->post_event(has<PlayerControlHuman>() ? AK::EVENTS::PLAY_LAND_PLAYER : AK::EVENTS::PLAY_LAND);
	attach_time = Game::time.total;

	velocity = Vec3::zero;
}

b8 msg_send(Awk* a, Awk::NetMessage t)
{
	using Stream = Net::StreamWrite;
	Net::StreamWrite* p = Net::msg_awk(a);
	serialize_enum(p, Awk::NetMessage, t);
	Net::msg_finalize(p);
	return true;
}

void Awk::finish_flying()
{
	msg_send(this, NetMessage::DoneFlying);
}

void Awk::finish_dashing()
{
	msg_send(this, NetMessage::DoneDashing);
}

b8 Awk::msg(Net::StreamRead* p)
{
	using Stream = Net::StreamRead;
	NetMessage type;
	serialize_enum(p, NetMessage, type);
	switch (type)
	{
		case NetMessage::DoneDashing:
		{
			finish_flying_dashing_common();
			done_dashing.fire();
			break;
		}
		case NetMessage::DoneFlying:
		{
			finish_flying_dashing_common();
			done_flying.fire();
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

void Awk::update_server(const Update& u)
{
	State s = state();

	invincible_timer = vi_max(invincible_timer - u.time.delta, 0.0f);

	if (cooldown > 0.0f)
	{
		cooldown = vi_max(0.0f, cooldown - u.time.delta);
		if (cooldown == 0.0f)
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
				finish_dashing();
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
						get<Audio>()->post_event(has<PlayerControlHuman>() ? AK::EVENTS::PLAY_AWK_FOOTSTEP_PLAYER : AK::EVENTS::PLAY_AWK_FOOTSTEP);
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
		if (s == State::Dash)
		{
			update_lerped_pos(5.0f, u);
			update_offset();
		}

		// emit particles
		// but don't start until the awk has cleared the camera radius
		// we do this so that the particles don't block the camera
		r32 particle_start_delay = AWK_THIRD_PERSON_OFFSET / velocity.length();
		if (u.time.total > attach_time + particle_start_delay)
		{
			Vec3 pos = get<Transform>()->absolute_pos();
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
		Vec3 pos = get<Transform>()->absolute_pos();
		if (s != State::Fly)
			velocity = velocity * 0.75f + ((pos - last_pos) / u.time.delta) * 0.25f;
		last_pos = pos;
	}
}

r32 Awk::movement_raycast(const Vec3& ray_start, const Vec3& ray_end)
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
			s16 group = ray_callback.m_collisionObjects[i]->getBroadphaseHandle()->m_collisionFilterGroup;
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
				if (current_ability == Ability::Sniper) // we just shot at a wall; spawn some particles
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

	return fraction_end * (ray_end - ray_start).length();
}

}
