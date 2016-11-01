#include "entities.h"
#include "data/animator.h"
#include "render/skinned_model.h"
#include "walker.h"
#include "asset/armature.h"
#include "asset/animation.h"
#include "asset/shader.h"
#include "asset/texture.h"
#include "asset/mesh.h"
#include "recast/Detour/Include/DetourNavMeshQuery.h"
#include "mersenne/mersenne-twister.h"
#include "game.h"
#include "audio.h"
#include "asset/Wwise_IDs.h"
#include "render/views.h"
#include "awk.h"
#include "bullet/src/BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "menu.h"
#include "data/ragdoll.h"
#include "usernames.h"
#include "console.h"
#include "minion.h"
#include "render/particles.h"
#include "strings.h"
#include "data/priority_queue.h"
#include "net.h"
#include "net_serialize.h"
#include "team.h"

namespace VI
{


void explosion(const Vec3& pos, const Quat& rot)
{
	for (s32 i = 0; i < 50; i++)
	{
		Particles::sparks.add
		(
			pos,
			rot * Vec3(mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo()) * 10.0f,
			Vec4(1, 1, 1, 1)
		);
	}
	Entity* shockwave = World::create<ShockwaveEntity>(8.0f, 1.5f);
	shockwave->get<Transform>()->absolute_pos(pos);
	Net::finalize(shockwave);
}

AwkEntity::AwkEntity(AI::Team team)
{
	create<Audio>();
	create<Transform>();
	create<Awk>();
	create<AIAgent>()->team = team;

	Health* health = create<Health>(1, AWK_HEALTH, AWK_SHIELD, AWK_SHIELD);

	SkinnedModel* model = create<SkinnedModel>();
	model->mesh = Asset::Mesh::awk;
	model->shader = Asset::Shader::armature;
	model->team = s8(team);

	Animator* anim = create<Animator>();
	anim->armature = Asset::Armature::awk;

	create<Target>()->local_offset = Vec3(0, 0, AWK_RADIUS * -1.1f);

	create<RigidBody>(RigidBody::Type::Sphere, Vec3(AWK_RADIUS), 0.0f, CollisionAwk | CollisionTarget, CollisionDefault & ~CollisionTarget & ~CollisionAwkIgnore);
}

Health::Health(s8 hp, s8 hp_max, s8 shield, s8 shield_max)
	: hp(hp),
	hp_max(hp_max),
	shield(shield),
	shield_max(shield_max),
	changed(),
	killed(),
	regen_timer()
{
}

#define REGEN_DELAY 9.0f
#define REGEN_TIME 1.0f

void Health::update(const Update& u)
{
	if (shield < shield_max)
	{
		r32 old_timer = regen_timer;
		regen_timer -= u.time.delta;
		if (regen_timer < REGEN_TIME)
		{
			const r32 regen_interval = REGEN_TIME / shield_max;
			if ((s32)(old_timer / regen_interval) != (s32)(regen_timer / regen_interval))
			{
				shield += 1;
				changed.fire(
				{
					nullptr,
					1,
					0,
					1,
				});
			}
		}
	}
}

void Health::damage(Entity* e, s8 damage)
{
	if (hp > 0 && damage > 0)
	{
		s8 damage_accumulator = damage;
		s8 damage_shield;
		if (damage_accumulator > shield)
		{
			damage_shield = shield;
			damage_accumulator -= shield;
			shield = 0;
		}
		else
		{
			damage_shield = damage_accumulator;
			shield -= damage_accumulator;
			damage_accumulator = 0;
		}

		s8 damage_hp;
		if (damage_accumulator > hp)
		{
			damage_hp = hp;
			hp = 0;
		}
		else
		{
			damage_hp = damage_accumulator;
			hp -= damage_accumulator;
		}

		regen_timer = REGEN_TIME + REGEN_DELAY;

		changed.fire(
		{
			e,
			s8(-damage),
			s8(-damage_hp),
			s8(-damage_shield),
		});
		if (hp == 0)
			killed.fire(e);
	}
}

void Health::take_shield()
{
	damage(nullptr, shield);
}

void Health::kill(Entity* e)
{
	damage(e, hp_max + shield_max);
}

void Health::add(s8 amount)
{
	s16 old_hp = hp;
	hp = vi_min((s8)(hp + amount), hp_max);
	if (hp > old_hp)
	{
		changed.fire(
		{
			nullptr,
			amount,
			amount,
			0,
		});
	}
}

s8 Health::total() const
{
	return hp + shield;
}

EnergyPickupEntity::EnergyPickupEntity(const Vec3& p)
{
	create<Transform>()->pos = p;
	View* model = create<View>();
	model->color = Vec4(0.6f, 0.6f, 0.6f, MATERIAL_NO_OVERRIDE);
	model->mesh = Asset::Mesh::target;
	model->shader = Asset::Shader::standard;

	create<AICue>(AICue::Type::Sensor | AICue::Type::Rocket);

	PointLight* light = create<PointLight>();
	light->type = PointLight::Type::Override;
	light->radius = 0.0f;

	create<Sensor>();

	Target* target = create<Target>();

	EnergyPickup* pickup = create<EnergyPickup>();

	model->offset.scale(Vec3(HEALTH_PICKUP_RADIUS - 0.2f));

	RigidBody* body = create<RigidBody>(RigidBody::Type::Sphere, Vec3(HEALTH_PICKUP_RADIUS), 0.1f, CollisionAwkIgnore | CollisionTarget, ~CollisionAwk & ~CollisionShield);
	body->set_damping(0.5f, 0.5f);
	body->set_ccd(true);

	Entity* e = World::create<Empty>();
	e->get<Transform>()->parent = get<Transform>();
	e->add<PointLight>()->radius = 8.0f;
	Net::finalize(e);
	pickup->light = e;
}

r32 EnergyPickup::Key::priority(EnergyPickup* p)
{
	return (p->get<Transform>()->absolute_pos() - me).length_squared() * (closest_first ? 1.0f : -1.0f);
}

EnergyPickup* EnergyPickup::closest(AI::TeamMask mask, const Vec3& pos, r32* distance)
{
	EnergyPickup* closest = nullptr;
	r32 closest_distance = FLT_MAX;

	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask))
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

s32 EnergyPickup::count(AI::TeamMask m)
{
	s32 count = 0;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, m))
			count++;
	}
	return count;
}

void EnergyPickup::sort_all(const Vec3& pos, Array<Ref<EnergyPickup>>* result, b8 closest_first, AI::TeamMask mask)
{
	Key key;
	key.me = pos;
	key.closest_first = closest_first;
	PriorityQueue<EnergyPickup*, Key> pickups(&key);

	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask))
			pickups.push(i.item());
	}

	result->length = 0;
	while (pickups.size() > 0)
		result->add(pickups.pop());
}

void EnergyPickup::awake()
{
	link_arg<const TargetEvent&, &EnergyPickup::hit>(get<Target>()->target_hit);
	set_team(team);
}

EnergyPickup::~EnergyPickup()
{
	if (Game::level.local && light.ref())
		World::remove_deferred(light.ref());
}

void EnergyPickup::reset()
{
	set_team(AI::TeamNone);
}

void EnergyPickup::hit(const TargetEvent& e)
{
	if (e.hit_by->has<Awk>() && e.hit_by->get<Awk>()->current_ability == Ability::Sniper)
		set_team(AI::TeamNone, e.hit_by);
	else
		set_team(e.hit_by->get<AIAgent>()->team, e.hit_by);
}

b8 EnergyPickup::net_msg(Net::StreamRead* p)
{
	using Stream = Net::StreamRead;
	Ref<EnergyPickup> ref;
	serialize_ref(p, ref);
	AI::Team t;
	serialize_s8(p, t);
	Ref<Entity> caused_by;
	serialize_ref(p, caused_by);

	EnergyPickup* pickup = ref.ref();
	pickup->team = t;
	pickup->get<View>()->team = (s8)t;
	pickup->get<PointLight>()->team = (s8)t;
	pickup->get<PointLight>()->radius = (t == AI::TeamNone) ? 0.0f : SENSOR_RANGE;
	pickup->get<Sensor>()->team = t;
	if (caused_by.ref() && t == caused_by.ref()->get<AIAgent>()->team)
	{
		if (Game::level.local)
			caused_by.ref()->get<PlayerCommon>()->manager.ref()->add_credits(CREDITS_CAPTURE_ENERGY_PICKUP);
		if (caused_by.ref()->has<PlayerControlHuman>())
			caused_by.ref()->get<PlayerControlHuman>()->player.ref()->msg(_(strings::battery_captured), true);
	}

	return true;
}

// returns true if we were successfully captured
// the second parameter is the entity that caused the ownership change
b8 EnergyPickup::set_team(AI::Team t, Entity* caused_by)
{
	// must be neutral or owned by an enemy
	if (t != team)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::EnergyPickup);
		Ref<EnergyPickup> ref = this;
		serialize_ref(p, ref);
		serialize_s8(p, t);
		Ref<Entity> caused_by_ref = caused_by;
		serialize_ref(p, caused_by_ref);
		Net::msg_finalize(p);
		return true;
	}

	return false;
}

r32 EnergyPickup::power_particle_timer;
r32 EnergyPickup::particle_accumulator;
void EnergyPickup::update_all(const Update& u)
{
	{
		// normal particles
		const r32 interval = 0.1f;
		particle_accumulator += u.time.delta;
		while (particle_accumulator > interval)
		{
			particle_accumulator -= interval;
			for (auto i = list.iterator(); !i.is_last(); i.next())
			{
				Vec3 pos = i.item()->get<Transform>()->absolute_pos();

				Particles::tracers.add
				(
					pos + Quat::euler(0.0f, mersenne::randf_co() * PI * 2.0f, (mersenne::randf_co() - 0.5f) * PI) * Vec3(0, 0, mersenne::randf_co() * 0.6f),
					Vec3::zero,
					0
				);
			}
		}
	}

	// power particles
	const r32 particle_interval = 0.1f;
	const r32 particle_reset = 2.0f;
	b8 emit_particles = (s32)(power_particle_timer / particle_interval) < (s32)((power_particle_timer + u.time.delta) / particle_interval);
	power_particle_timer += u.time.delta;
	while (power_particle_timer > particle_reset)
		power_particle_timer -= particle_reset;
	r32 particle_blend = power_particle_timer / particle_reset;

	// clear powered state for all containment fields; we're going to update this flag
	for (auto field = ContainmentField::list.iterator(); !field.is_last(); field.next())
		field.item()->powered = false;

	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team == AI::TeamNone)
			continue;

		Vec3 control_point_pos = i.item()->get<Transform>()->absolute_pos();

		// update powered state of all containment fields in range
		StaticArray<Vec3, 10> containment_fields;
		for (auto field = ContainmentField::list.iterator(); !field.is_last(); field.next())
		{
			if (field.item()->team == i.item()->team)
			{
				Vec3 field_pos = field.item()->get<Transform>()->absolute_pos();
				if ((field_pos - control_point_pos).length_squared() < CONTAINMENT_FIELD_RADIUS * CONTAINMENT_FIELD_RADIUS)
				{
					if (containment_fields.length < containment_fields.capacity())
						containment_fields.add(field_pos);
					field.item()->powered = true;
				}
			}
		}

		if (emit_particles)
		{
			// particle effects to all containment fields in range
			for (s32 i = 0; i < containment_fields.length; i++)
			{
				Particles::tracers.add
				(
					Vec3::lerp(particle_blend, control_point_pos, containment_fields[i]),
					Vec3::zero,
					0
				);
			}
		}
	}
}

ControlPointEntity::ControlPointEntity(AI::Team team, const Vec3& pos)
{
	create<Transform>()->absolute_pos(pos);

	View* view = create<View>();
	view->mesh = Asset::Mesh::control_point;
	view->shader = Asset::Shader::culled;
	view->color = Vec4(0.6f, 0.6f, 0.6f, MATERIAL_NO_OVERRIDE);

	create<PlayerTrigger>()->radius = CONTROL_POINT_RADIUS;

	PointLight* light = create<PointLight>();
	light->offset.z = 2.0f;
	light->radius = 12.0f;

	create<ControlPoint>(team);

	create<RigidBody>(RigidBody::Type::Mesh, Vec3::zero, 0.0f, btBroadphaseProxy::StaticFilter, ~btBroadphaseProxy::StaticFilter, Asset::Mesh::control_point);
}

PlayerSpawnEntity::PlayerSpawnEntity(AI::Team team)
{
	create<Transform>();

	View* view = create<View>();
	view->mesh = Asset::Mesh::spawn;
	view->shader = Asset::Shader::culled;
	view->team = s8(team);

	create<PlayerTrigger>()->radius = CONTROL_POINT_RADIUS;

	PointLight* light = create<PointLight>();
	light->offset.z = 2.0f;
	light->radius = 12.0f;
	light->team = s8(team);

	create<PlayerSpawn>()->team = team;
}

ControlPoint::ControlPoint(AI::Team t)
	: team(t),
	obstacle_id(u32(-1))
{
}

void ControlPoint::awake()
{
	set_team(team);
	if (Game::level.local && obstacle_id == u32(-1))
	{
		Vec3 pos = get<Transform>()->absolute_pos();
		pos.y -= 1.5f;
		obstacle_id = AI::obstacle_add(pos, 1.0f, 3.0f);
	}
}

ControlPoint::~ControlPoint()
{
	if (obstacle_id != u32(-1))
		AI::obstacle_remove(obstacle_id);
}

void ControlPoint::set_team(AI::Team t)
{
	team = t;
	get<PointLight>()->team = (s8)team;
	get<View>()->team = (s8)team;
}

void ControlPoint::capture_start(AI::Team t)
{
	vi_assert(team_next == AI::TeamNone);
	// no capture in progress; start capturing
	team_next = t;
	capture_timer = CONTROL_POINT_CAPTURE_TIME * (team == AI::TeamNone ? 0.5f : 1.0f);
}

void ControlPoint::capture_cancel()
{
	team_next = AI::TeamNone;
	capture_timer = 0.0f;
}

b8 ControlPoint::owned_by(AI::Team t) const
{
	return (team == t && team_next == AI::TeamNone) || team_next == t;
}

void ControlPoint::update(const Update& u)
{
	if (capture_timer > 0.0f)
	{
		capture_timer -= u.time.delta;
		if (capture_timer <= 0.0f)
		{
			// capture complete
			set_team(team_next);
			capture_cancel();
		}
		else if (capture_timer < CONTROL_POINT_CAPTURE_TIME * 0.5f && capture_timer + u.time.delta >= CONTROL_POINT_CAPTURE_TIME * 0.5f)
		{
			// halfway point
			set_team(AI::TeamNone);
		}
	}
}

s32 ControlPoint::count(AI::TeamMask mask)
{
	s32 count = 0;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask))
			count++;
	}
	return count;
}

s32 ControlPoint::count_capturing()
{
	s32 count = 0;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->capture_timer > 0.0f)
			count++;
	}
	return count;
}

ControlPoint* ControlPoint::closest(AI::TeamMask mask, const Vec3& pos, r32* distance)
{
	ControlPoint* closest = nullptr;
	r32 closest_distance = FLT_MAX;

	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask))
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

SensorEntity::SensorEntity(AI::Team team, const Vec3& abs_pos, const Quat& abs_rot)
{
	Transform* transform = create<Transform>();
	transform->pos = abs_pos;
	transform->rot = abs_rot;

	View* model = create<View>();
	model->mesh = Asset::Mesh::sphere;
	model->team = (s8)team;
	model->shader = Asset::Shader::standard;
	model->offset.scale(Vec3(SENSOR_RADIUS * 1.2f)); // a little bigger for aesthetic reasons

	create<Health>(SENSOR_HEALTH, SENSOR_HEALTH);

	PointLight* light = create<PointLight>();
	light->type = PointLight::Type::Override;
	light->team = (s8)team;
	light->radius = SENSOR_RANGE;

	create<Sensor>(team);

	create<Target>();

	RigidBody* body = create<RigidBody>(RigidBody::Type::Sphere, Vec3(SENSOR_RADIUS), 1.0f, CollisionAwkIgnore | CollisionTarget, ~CollisionAwk & ~CollisionShield);
	body->set_damping(0.5f, 0.5f);
}

Sensor::Sensor(AI::Team t)
	: team(t)
{
}

void Sensor::awake()
{
	if (!has<EnergyPickup>())
	{
		link_arg<Entity*, &Sensor::killed_by>(get<Health>()->killed);
		link_arg<const TargetEvent&, &Sensor::hit_by>(get<Target>()->target_hit);
	}
}

void Sensor::hit_by(const TargetEvent& e)
{
	vi_assert(!has<EnergyPickup>());
	get<Health>()->damage(e.hit_by, get<Health>()->hp_max);
}

void Sensor::killed_by(Entity* e)
{
	vi_assert(!has<EnergyPickup>());
	World::remove_deferred(entity());
}

#define sensor_shockwave_interval 3.0f
void Sensor::update_all_server(const Update& u)
{
	r32 time = u.time.total;
	r32 last_time = time - u.time.delta;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team != AI::TeamNone)
		{
			r32 offset = i.index * sensor_shockwave_interval * 0.3f;
			if ((s32)((time + offset) / sensor_shockwave_interval) != (s32)((last_time + offset) / sensor_shockwave_interval))
			{
				Entity* shockwave = World::create<ShockwaveEntity>(10.0f, 1.5f);
				shockwave->get<Transform>()->absolute_pos(i.item()->get<Transform>()->absolute_pos());
				Net::finalize(shockwave);
			}
		}
	}
}

b8 Sensor::can_see(AI::Team team, const Vec3& pos, const Vec3& normal)
{
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team == team)
		{
			Vec3 to_sensor = i.item()->get<Transform>()->absolute_pos() - pos;
			if (to_sensor.length_squared() < SENSOR_RANGE * SENSOR_RANGE && to_sensor.dot(normal) > 0.0f)
				return true;
		}
	}
	return false;
}

Sensor* Sensor::closest(AI::TeamMask mask, const Vec3& pos, r32* distance)
{
	Sensor* closest = nullptr;
	r32 closest_distance = FLT_MAX;

	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask))
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

AICue::AICue(TypeMask t)
	: type(t)
{
}

AICue::AICue() : type() {}

// returns the closest sensor interest point within range of the given position, or null
AICue* AICue::in_range(AICue::TypeMask mask, const Vec3& pos, r32 radius, s32* count)
{
	AICue* closest = nullptr;
	r32 closest_distance = radius * radius;

	s32 c = 0;

	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (mask & i.item()->type)
		{
			r32 d = (i.item()->get<Transform>()->absolute_pos() - pos).length_squared();
			if (d < closest_distance)
			{
				closest = i.item();
				closest_distance = d;
			}

			if (d < radius * radius)
				c++;
		}
	}

	if (count)
		*count = c;

	return closest;
}

void Rocket::awake()
{
	get<Health>()->killed.link<Rocket, Entity*, &Rocket::killed>(this);
}

void Rocket::killed(Entity*)
{
	World::remove_deferred(entity());
}

void Rocket::launch(Entity* t)
{
	vi_assert(!target.ref() && get<Transform>()->parent.ref());
	target = t;

	PointLight* light = entity()->add<PointLight>();
	light->radius = 10.0f;
	light->color = Vec3(1, 1, 1);

	get<Transform>()->reparent(nullptr);
}

Rocket* Rocket::inbound(Entity* target)
{
	if (target->has<AIAgent>() && target->get<AIAgent>()->stealth)
		return nullptr;

	for (auto rocket = Rocket::list.iterator(); !rocket.is_last(); rocket.next())
	{
		if (rocket.item()->target.ref() == target)
			return rocket.item();
	}
	return nullptr;
}

Rocket* Rocket::closest(AI::TeamMask mask, const Vec3& pos, r32* distance)
{
	Rocket* closest = nullptr;
	r32 closest_distance = FLT_MAX;

	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask))
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

Rocket::Rocket()
	: team(),
	target(),
	owner(),
	particle_accumulator(),
	remaining_lifetime(15.0f)
{
}

void Rocket::explode()
{
	Vec3 pos;
	Quat rot;
	get<Transform>()->absolute(&pos, &rot);
	explosion(pos, rot.inverse());

	World::remove_deferred(entity());
}

void Rocket::update(const Update& u)
{
	if (!get<Transform>()->parent.ref())
	{
		remaining_lifetime -= u.time.delta;
		if (remaining_lifetime < 0.0f)
		{
			explode();
			return;
		}

		// we're in flight
		if (target.ref() && !target.ref()->get<AIAgent>()->stealth)
		{
			if (target.ref()->has<PlayerCommon>()) // we're locked on to the actual player; see if we need to switch to targeting a decoy
			{
				Entity* decoy = target.ref()->get<PlayerCommon>()->manager.ref()->decoy();
				if (decoy)
					target = decoy;
			}

			// aim toward target
			Vec3 to_target = target.ref()->get<Transform>()->absolute_pos() - get<Transform>()->pos;
			r32 distance = to_target.length();
			to_target /= distance;
			Quat target_rot = Quat::look(to_target);

			const r32 whisker_length = 3.0f;
			const s32 whisker_count = 4;
			static const Vec3 whiskers[whisker_count] =
			{
				Vec3(0, 0, 1.0f) * whisker_length,
				Vec3(0, 0.7f, 1.0f) * whisker_length,
				Vec3(-1.0f, -0.7f, 1.0f) * whisker_length,
				Vec3(1.0f, -0.7f, 1.0f) * whisker_length,
			};

			if (distance > 5.0f)
			{
				// avoid walls
				for (s32 i = 0; i < whisker_count; i++)
				{
					btCollisionWorld::ClosestRayResultCallback ray_callback(get<Transform>()->pos, get<Transform>()->pos + get<Transform>()->rot * whiskers[i]);
					Physics::raycast(&ray_callback, ~CollisionTarget & ~CollisionAwkIgnore);
					if (ray_callback.hasHit())
					{
						// avoid the obstacle
						Vec3 wall_normal = ray_callback.m_hitNormalWorld;

						Vec3 dir_flattened = to_target - (wall_normal * wall_normal.dot(to_target));
						if (dir_flattened.length_squared() < 0.00001f)
							dir_flattened = -wall_normal; // we are headed smack into the wall; nowhere to go, just try to turn around

						target_rot = Quat::look(dir_flattened);
						break;
					}
				}
			}

			r32 angle = Quat::angle(get<Transform>()->rot, target_rot);
			if (angle > 0)
				get<Transform>()->rot = Quat::slerp(vi_min(1.0f, 5.0f * u.time.delta), get<Transform>()->rot, target_rot);
		}

		Vec3 velocity = get<Transform>()->rot * Vec3(0, 0, 15.0f);
		Vec3 next_pos = get<Transform>()->pos + velocity * u.time.delta;

		btCollisionWorld::ClosestRayResultCallback ray_callback(get<Transform>()->pos, next_pos + get<Transform>()->rot * Vec3(0, 0, 0.1f));
		Physics::raycast(&ray_callback, ~CollisionTarget & ~CollisionAwkIgnore);
		if (ray_callback.hasHit())
		{
			// we hit something
			Entity* hit = &Entity::list[ray_callback.m_collisionObject->getUserIndex()];
			if (!hit->has<AIAgent>() || hit->get<AIAgent>()->team != team) // fly through friendlies
			{
				// kaboom

				// do damage
				if (hit->has<Awk>() || hit->has<Decoy>())
					hit->get<Target>()->hit(entity());

				explode();
				return;
			}
		}

		// keep flying
		{
			particle_accumulator += u.time.delta;
			const r32 interval = 0.07f;
			while (particle_accumulator > interval)
			{
				particle_accumulator -= interval;
				Particles::tracers.add
				(
					get<Transform>()->pos + velocity * particle_accumulator,
					Vec3::zero,
					0
				);
			}
			get<Transform>()->pos = next_pos;
		}
	}
}

RocketEntity::RocketEntity(Entity* owner, Transform* parent, const Vec3& pos, const Quat& rot, AI::Team team)
{
	Transform* transform = create<Transform>();
	transform->parent = parent;
	transform->absolute(pos + rot * Vec3(0, 0, 0.11f), rot);

	Rocket* rocket = create<Rocket>();
	rocket->team = team;
	rocket->owner = owner;

	create<Health>(SENSOR_HEALTH, SENSOR_HEALTH);

	View* model = create<View>();
	model->mesh = Asset::Mesh::rocket_pod;
	model->team = (s8)team;
	model->shader = Asset::Shader::standard;

	create<RigidBody>(RigidBody::Type::CapsuleZ, Vec3(0.1f, 0.3f, 0.3f), 0.0f, CollisionAwkIgnore, btBroadphaseProxy::AllFilter);
}

DecoyEntity::DecoyEntity(PlayerManager* owner, Transform* parent, const Vec3& pos, const Quat& rot)
{
	create<Audio>();

	AI::Team team = owner->team.ref()->team();
	Transform* transform = create<Transform>();
	transform->parent = parent;
	transform->absolute(pos + rot * Vec3(0, 0, AWK_RADIUS), rot);
	create<AIAgent>()->team = team;

	create<Health>(AWK_HEALTH, AWK_HEALTH, AWK_SHIELD, AWK_SHIELD);

	SkinnedModel* model = create<SkinnedModel>();
	model->mesh = Asset::Mesh::awk;
	model->shader = Asset::Shader::armature;
	model->team = s8(team);

	Animator* anim = create<Animator>();
	anim->armature = Asset::Armature::awk;
	anim->layers[0].loop = true;
	anim->layers[0].play(Asset::Animation::awk_dash);

	create<Target>()->local_offset = Vec3(0, 0, AWK_RADIUS * -1.1f);

	create<RigidBody>(RigidBody::Type::Sphere, Vec3(AWK_SHIELD_RADIUS), 0.0f, CollisionShield, CollisionDefault);

	create<Decoy>()->owner = owner;
}

void Decoy::awake()
{
	link_arg<const TargetEvent&, &Decoy::hit_by>(get<Target>()->target_hit);
	link_arg<Entity*, &Decoy::killed>(get<Health>()->killed);
}

void Decoy::killed(Entity*)
{
	destroy();
}

void Decoy::destroy()
{
	Vec3 pos;
	Quat rot;
	get<Transform>()->absolute(&pos, &rot);
	explosion(pos, rot);
	World::remove_deferred(entity());
}

void Decoy::hit_by(const TargetEvent& e)
{
	get<Audio>()->post_event(has<PlayerControlHuman>() ? AK::EVENTS::PLAY_HURT_PLAYER : AK::EVENTS::PLAY_HURT);
	get<Health>()->damage(e.hit_by, 1);
}

// returns true if the given position is inside an enemy containment field
ContainmentField* ContainmentField::inside(AI::TeamMask mask, const Vec3& pos)
{
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask) && i.item()->contains(pos))
			return i.item();
	}
	return nullptr;
}

b8 ContainmentField::contains(const Vec3& pos) const
{
	return (pos - get<Transform>()->absolute_pos()).length_squared() < CONTAINMENT_FIELD_RADIUS * CONTAINMENT_FIELD_RADIUS;
}

// describes which enemy containment fields you are currently inside
u32 ContainmentField::hash(AI::Team my_team, const Vec3& pos)
{
	u32 result = 0;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team != my_team && (pos - i.item()->get<Transform>()->absolute_pos()).length_squared() < CONTAINMENT_FIELD_RADIUS * CONTAINMENT_FIELD_RADIUS)
		{
			if (result == 0)
				result = 1;
			result += MAX_ENTITIES % (i.index + 37); // todo: learn how to math
		}
	}
	return result;
}

ContainmentField::ContainmentField()
	: team(AI::TeamNone), owner(), remaining_lifetime(CONTAINMENT_FIELD_LIFETIME), powered()
{
}

void ContainmentField::awake()
{
	link_arg<const TargetEvent&, &ContainmentField::hit_by>(get<Target>()->target_hit);
	link_arg<Entity*, &ContainmentField::killed>(get<Health>()->killed);
}

ContainmentField::~ContainmentField()
{
	if (Game::level.local && field.ref())
		World::remove_deferred(field.ref());
}

void ContainmentField::hit_by(const TargetEvent& e)
{
	get<Health>()->damage(e.hit_by, get<Health>()->hp_max);
}

void ContainmentField::killed(Entity*)
{
	World::remove_deferred(entity());
}

ContainmentField* ContainmentField::closest(AI::TeamMask mask, const Vec3& pos, r32* distance)
{
	ContainmentField* closest = nullptr;
	r32 closest_distance = FLT_MAX;

	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask))
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

// must be called after EnergyPickup::update_all(), which sets the powered flag
r32 ContainmentField::particle_accumulator;
void ContainmentField::update_all(const Update& u)
{
	const r32 interval = 0.1f;
	particle_accumulator += u.time.delta;
	while (particle_accumulator > interval)
	{
		particle_accumulator -= interval;
		for (auto i = list.iterator(); !i.is_last(); i.next())
		{
			Vec3 pos = i.item()->get<Transform>()->absolute_pos();

			// spawn particle effect
			Particles::eased_particles.add
			(
				pos + Quat::euler(0.0f, mersenne::randf_co() * PI * 2.0f, (mersenne::randf_co() - 0.5f) * PI) * Vec3(0, 0, 2.0f),
				pos,
				0
			);
		}
	}

	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		i.item()->remaining_lifetime -= u.time.delta * (i.item()->powered ? 0.25f : 1.0f);
		if (i.item()->remaining_lifetime < 0.0f)
			i.item()->destroy();
	}
}

void ContainmentField::destroy()
{
	Vec3 pos;
	Quat rot;
	get<Transform>()->absolute(&pos, &rot);
	explosion(pos, rot);
	World::remove_deferred(entity());
}

#define CONTAINMENT_FIELD_BASE_RADIUS 0.385f
ContainmentFieldEntity::ContainmentFieldEntity(Transform* parent, const Vec3& abs_pos, const Quat& abs_rot, PlayerManager* m)
{
	Transform* transform = create<Transform>();
	transform->absolute(abs_pos, abs_rot);
	transform->reparent(parent);

	AI::Team team = m->team.ref()->team();

	// destroy any overlapping friendly containment field
	for (auto i = ContainmentField::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team == team
			&& (i.item()->get<Transform>()->absolute_pos() - abs_pos).length_squared() < CONTAINMENT_FIELD_RADIUS * 2.0f * CONTAINMENT_FIELD_RADIUS * 2.0f)
		{
			i.item()->destroy();
		}
	}

	View* model = create<View>();
	model->team = (s8)m->team.ref()->team();
	model->mesh = Asset::Mesh::containment_field_base;
	model->shader = Asset::Shader::standard;

	create<Target>();
	create<Health>(SENSOR_HEALTH, SENSOR_HEALTH);
	create<RigidBody>(RigidBody::Type::Sphere, Vec3(CONTAINMENT_FIELD_BASE_RADIUS), 0.0f, btBroadphaseProxy::StaticFilter | CollisionAwkIgnore | CollisionTarget, ~btBroadphaseProxy::StaticFilter & ~CollisionAwk & ~CollisionShield);

	ContainmentField* field = create<ContainmentField>();
	field->team = team;
	field->owner = m;

	Entity* f = World::create<Empty>();
	f->get<Transform>()->absolute_pos(abs_pos);

	View* view = f->add<View>();
	view->team = (s8)team;
	view->mesh = Asset::Mesh::containment_field_sphere;
	view->shader = Asset::Shader::fresnel;
	view->alpha();
	view->color.w = 0.35f;

	CollisionGroup team_mask = (CollisionGroup)(1 << (8 + team));

	f->add<RigidBody>(RigidBody::Type::Mesh, Vec3::zero, 0.0f, team_mask, CollisionAwkIgnore, view->mesh);

	Net::finalize(f);

	field->field = f;
}

#define TELEPORTER_RADIUS 0.5f
TeleporterEntity::TeleporterEntity(Transform* parent, const Vec3& pos, const Quat& rot, AI::Team team)
{
	Transform* transform = create<Transform>();
	transform->parent = parent;
	transform->absolute(pos, rot);
	create<Teleporter>()->team = team;
	create<RigidBody>(RigidBody::Type::Sphere, Vec3(TELEPORTER_RADIUS), 0.0f, CollisionAwkIgnore, CollisionAwkIgnore);

	View* model = create<View>();
	model->mesh = Asset::Mesh::teleporter;
	model->team = (s8)team;
	model->shader = Asset::Shader::standard;

	create<Health>(SENSOR_HEALTH, SENSOR_HEALTH);
}

Teleporter* Teleporter::closest(AI::TeamMask mask, const Vec3& pos, r32* distance)
{
	r32 closest_distance = FLT_MAX;
	Teleporter* closest = nullptr;
	for (auto i = Teleporter::list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask))
		{
			r32 distance = (pos - i.item()->get<Transform>()->absolute_pos()).length_squared();
			if (distance < closest_distance)
			{
				closest = i.item();
				closest_distance = distance;
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

void Teleporter::awake()
{
	if (has<Health>())
		link_arg<Entity*, &Teleporter::killed>(get<Health>()->killed);
}

void Teleporter::killed(Entity*)
{
	destroy();
}

void Teleporter::destroy()
{
	Vec3 pos;
	Quat rot;
	get<Transform>()->absolute(&pos, &rot);
	explosion(pos, rot);
	World::remove_deferred(entity());
}

void teleport(Entity* e, Teleporter* target)
{
	{
		Entity* shockwave = World::create<ShockwaveEntity>(8.0f, 1.5f);
		shockwave->get<Transform>()->absolute_pos(e->get<Transform>()->absolute_pos());
		Net::finalize(shockwave);
	}

	Vec3 pos;
	Quat rot;
	target->get<Transform>()->absolute(&pos, &rot);

	if (e->has<Walker>())
	{
		// space minions out around the teleporter
		Vec3 teleport_pos = pos + rot * Quat::euler(0, 0, e->get<Walker>()->id() * PI * 0.25f) * Vec3(1, 0, 1);
		teleport_pos.y = vi_max(teleport_pos.y, pos.y + 1.0f);
		e->get<Walker>()->absolute_pos(teleport_pos);
		{
			Entity* shockwave = World::create<ShockwaveEntity>(8.0f, 1.5f);
			shockwave->get<Transform>()->absolute_pos(pos);
			Net::finalize(shockwave);
		}
	}
	else if (e->has<Awk>())
	{
		e->get<Awk>()->detach_teleport();
		e->get<Transform>()->absolute(pos + rot * Quat::euler(0, 0, e->get<Awk>()->id() * PI * 0.25f) * Vec3(0, 0, AWK_RADIUS * 4.0f), rot);
		e->get<Awk>()->velocity = rot * Vec3(0.0f, 0.0f, -AWK_FLY_SPEED); // make sure it shoots into the wall
		e->get<Awk>()->overshield_timer = AWK_OVERSHIELD_TIME;
	}
	else
		vi_assert(false);
}

#define PROJECTILE_LENGTH 0.5f
#define PROJECTILE_THICKNESS 0.05f
#define PROJECTILE_MAX_LIFETIME 10.0f
#define PROJECTILE_DAMAGE 1
ProjectileEntity::ProjectileEntity(Entity* owner, const Vec3& abs_pos, const Vec3& velocity)
{
	Vec3 dir = Vec3::normalize(velocity);
	Transform* transform = create<Transform>();
	transform->absolute_pos(abs_pos);
	transform->absolute_rot(Quat::look(dir));

	PointLight* light = create<PointLight>();
	light->radius = 10.0f;
	light->color = Vec3(1, 1, 1);

	create<Audio>();

	Projectile* p = create<Projectile>();
	p->owner = owner;
	p->velocity = dir * PROJECTILE_SPEED;
}

void Projectile::awake()
{
	get<Audio>()->post_event(AK::EVENTS::PLAY_LASER);
}

void Projectile::update(const Update& u)
{
	lifetime += u.time.delta;
	if (lifetime > PROJECTILE_MAX_LIFETIME)
	{
		World::remove(entity());
		return;
	}

	Vec3 pos = get<Transform>()->absolute_pos();
	Vec3 next_pos = pos + velocity * u.time.delta;
	btCollisionWorld::ClosestRayResultCallback ray_callback(pos, next_pos + Vec3::normalize(velocity) * PROJECTILE_LENGTH);

	// if we have an owner, we can go through their team's force fields. otherwise, collide with everything.
	s16 mask = owner.ref() ? ~Team::containment_field_mask(owner.ref()->get<AIAgent>()->team) : -1;
	Physics::raycast(&ray_callback, mask);
	if (ray_callback.hasHit())
	{
		Entity* hit_object = &Entity::list[ray_callback.m_collisionObject->getUserIndex()];
		if (hit_object != owner.ref())
		{
			Vec3 basis;
			if (hit_object->has<Awk>())
			{
				basis = Vec3::normalize(velocity);
				hit_object->get<Target>()->hit(entity());
			}
			else if (hit_object->has<Health>())
			{
				basis = Vec3::normalize(velocity);
				hit_object->get<Health>()->damage(owner.ref(), PROJECTILE_DAMAGE);
			}
			else
				basis = ray_callback.m_hitNormalWorld;

			Quat rot = Quat::look(basis);
			for (s32 i = 0; i < 50; i++)
			{
				Particles::sparks.add
				(
					ray_callback.m_hitPointWorld,
					rot * Vec3(mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo()) * 10.0f,
					Vec4(1, 1, 1, 1)
				);
			}
			{
				Entity* shockwave = World::create<ShockwaveEntity>(8.0f, 1.5f);
				shockwave->get<Transform>()->absolute_pos(ray_callback.m_hitPointWorld);
				Net::finalize(shockwave);
			}
			World::remove(entity());
		}
	}
	else
		get<Transform>()->absolute_pos(next_pos);
}

GrenadeEntity::GrenadeEntity(Entity* owner, const Vec3& abs_pos, const Vec3& velocity)
{
	AI::Team team = owner->get<AIAgent>()->team;
	Transform* transform = create<Transform>();
	transform->absolute_pos(abs_pos);

	create<Audio>();

	Grenade* p = create<Grenade>();
	p->owner = owner;

	Vec3 dir = Vec3::normalize(velocity);
	RigidBody* body = create<RigidBody>(RigidBody::Type::Sphere, Vec3(GRENADE_RADIUS), 1.0f, CollisionAwkIgnore | CollisionTarget, ~CollisionAwk & ~CollisionShield);
	body->set_damping(0.5f, 0.5f);
	body->set_ccd(true);
	body->set_restitution(1.0f);
	body->rebuild();
	body->btBody->setLinearVelocity(dir * GRENADE_LAUNCH_SPEED);

	View* model = create<View>();
	model->mesh = Asset::Mesh::cube;
	model->team = s8(team);
	model->shader = Asset::Shader::standard;
	model->offset.scale(Vec3(GRENADE_RADIUS * 0.577350269189626f));

	create<Health>(SENSOR_HEALTH, SENSOR_HEALTH);

	create<Target>();
}

void Grenade::update_server(const Update& u)
{
}

void Grenade::update_client(const Update& u)
{
	Vec3 pos = get<Transform>()->absolute_pos();
	if ((pos - last_particle).length_squared() > 0.5f * 0.5f)
	{
		Particles::tracers.add(pos, Vec3::zero, 0);
		last_particle = pos;
	}
}

void Grenade::awake()
{
	link_arg<Entity*, &Grenade::killed_by>(get<Health>()->killed);
	link_arg<const TargetEvent&, &Grenade::hit_by>(get<Target>()->target_hit);
}

void Grenade::hit_by(const TargetEvent& e)
{
	get<Health>()->damage(e.hit_by, get<Health>()->hp_max);
}

void Grenade::killed_by(Entity* e)
{
	World::remove_deferred(entity());
}

b8 Target::predict_intersection(const Vec3& from, r32 speed, Vec3* intersection) const
{
	Vec3 velocity;
	if (has<Awk>())
		velocity = get<Awk>()->velocity;
	else
		velocity = get<RigidBody>()->btBody->getInterpolationLinearVelocity();
	Vec3 pos = absolute_pos();
	Vec3 to_target = pos - from;
	r32 intersect_time_squared = to_target.dot(to_target) / ((speed * speed) - 2.0f * to_target.dot(velocity) - velocity.dot(velocity));
	if (intersect_time_squared > 0.0f)
	{
		*intersection = pos + velocity * sqrtf(intersect_time_squared);
		return true;
	}
	else
		return false;
}

r32 Target::radius() const
{
	if (has<Awk>())
		return AWK_SHIELD_RADIUS;
	else if (has<MinionCommon>())
		return MINION_HEAD_RADIUS;
	else
		return get<RigidBody>()->size.x;
}

void Target::hit(Entity* hit_by)
{
	TargetEvent e;
	e.hit_by = hit_by;
	e.target = entity();
	target_hit.fire(e);
}

Vec3 Target::absolute_pos() const
{
	return get<Transform>()->to_world(local_offset);
}

PlayerTrigger::PlayerTrigger()
	: entered(), exited(), triggered(), radius(1.0f)
{

}

b8 PlayerTrigger::is_triggered(const Entity* e) const
{
	for (s32 i = 0; i < max_trigger; i++)
	{
		if (e == triggered[i].ref())
			return true;
	}
	return false;
}

void PlayerTrigger::update(const Update& u)
{
	Vec3 pos = get<Transform>()->absolute_pos();
	r32 radius_squared = radius * radius;
	for (s32 i = 0; i < max_trigger; i++)
	{
		Entity* e = triggered[i].ref();
		if (e && (e->get<Transform>()->absolute_pos() - pos).length_squared() > radius_squared)
		{
			triggered[i] = nullptr;
			exited.fire(e);
		}
	}

	for (auto i = PlayerCommon::list.iterator(); !i.is_last(); i.next())
	{
		Entity* e = i.item()->entity();
		if ((e->get<Transform>()->absolute_pos() - pos).length_squared() < radius_squared)
		{
			b8 already_triggered = false;
			s32 free_slot = -1;
			for (s32 i = 0; i < max_trigger; i++)
			{
				if (free_slot == -1 && !triggered[i].ref())
					free_slot = i;

				if (triggered[i].ref() == e)
				{
					already_triggered = true;
					break;
				}
			}

			if (!already_triggered && free_slot != -1)
			{
				triggered[free_slot] = e;
				entered.fire(e);
			}
		}
	}
}

s32 PlayerTrigger::count() const
{
	s32 count = 0;
	for (s32 i = 0; i < max_trigger; i++)
	{
		if (triggered[i].ref())
			count++;
	}
	return count;
}

Array<Mat4> Rope::instances;

// draw rope segments and projectiles
void Rope::draw_opaque(const RenderParams& params)
{
	instances.length = 0;

	const Mesh* mesh_data = Loader::mesh_instanced(Asset::Mesh::tri_tube);
	Vec3 radius = (Vec4(mesh_data->bounds_radius, mesh_data->bounds_radius, mesh_data->bounds_radius, 0)).xyz();
	r32 f_radius = vi_max(radius.x, vi_max(radius.y, radius.z));

	// ropes
	{
		static Mat4 scale = Mat4::make_scale(Vec3(ROPE_RADIUS, ROPE_RADIUS, ROPE_SEGMENT_LENGTH * 0.5f));

		for (auto i = Rope::list.iterator(); !i.is_last(); i.next())
		{
			Mat4 m;
			i.item()->get<Transform>()->mat(&m);

			if (params.camera->visible_sphere(m.translation(), ROPE_SEGMENT_LENGTH * f_radius))
				instances.add(scale * m);
		}
	}

	// projectiles
	if (!(params.camera->mask & RENDER_MASK_SHADOW)) // projectiles don't cast shadows
	{
		static Mat4 scale = Mat4::make_scale(Vec3(PROJECTILE_THICKNESS, PROJECTILE_THICKNESS, PROJECTILE_LENGTH * 0.5f));
		static Mat4 offset = Mat4::make_translation(0, 0, PROJECTILE_LENGTH * 0.5f);
		for (auto i = Projectile::list.iterator(); !i.is_last(); i.next())
		{
			Mat4 m;
			i.item()->get<Transform>()->mat(&m);
			m = offset * m;
			if (params.camera->visible_sphere(m.translation(), PROJECTILE_LENGTH * f_radius))
				instances.add(scale * m);
		}
	}

	if (instances.length == 0)
		return;

	Loader::shader_permanent(Asset::Shader::standard_instanced);

	RenderSync* sync = params.sync;
	sync->write(RenderOp::Shader);
	sync->write(Asset::Shader::standard_instanced);
	sync->write(params.technique);

	Mat4 vp = params.view_projection;

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::vp);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(vp);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::v);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(params.view);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType::Vec4);
	sync->write<s32>(1);
	sync->write<Vec4>(Vec4(1, 1, 1, MATERIAL_NO_OVERRIDE));

	sync->write(RenderOp::Instances);
	sync->write(Asset::Mesh::tri_tube);
	sync->write(instances.length);
	sync->write<Mat4>(instances.data, instances.length);
}

RigidBody* rope_add(RigidBody* start, const Vec3& start_relative_pos, const Vec3& pos, const Quat& rot, r32 slack, RigidBody::Constraint::Type constraint_type)
{
	RigidBody* last_segment = start;
	Vec3 last_segment_relative_pos = start_relative_pos;
	Vec3 forward = rot * Vec3(0, 0, 1);
	while (true)
	{
		if (last_segment)
		{
			Vec3 last_segment_pos = last_segment->get<Transform>()->to_world(last_segment_relative_pos);
			Vec3 diff = pos - last_segment_pos;
			r32 length = diff.dot(forward);
			r32 rope_interval = ROPE_SEGMENT_LENGTH / (1.0f + slack);
			Vec3 scale = Vec3(ROPE_RADIUS, ROPE_RADIUS, ROPE_SEGMENT_LENGTH * 0.5f);

			if (length > rope_interval * 0.5f)
			{
				Vec3 spawn_pos = last_segment_pos + (diff / length) * rope_interval * 0.5f;
				Entity* box = World::create<PhysicsEntity>(AssetNull, spawn_pos, rot, RigidBody::Type::CapsuleZ, Vec3(ROPE_RADIUS, ROPE_SEGMENT_LENGTH - ROPE_RADIUS * 2.0f, 0.0f), 0.05f, CollisionAwkIgnore, CollisionInaccessibleMask);
				box->add<Rope>();

				static Quat rotation_a = Quat::look(Vec3(0, 0, 1)) * Quat::euler(0, PI * -0.5f, 0);
				static Quat rotation_b = Quat::look(Vec3(0, 0, -1)) * Quat::euler(PI, PI * -0.5f, 0);

				RigidBody::Constraint constraint;
				constraint.type = constraint_type;
				constraint.frame_a = btTransform(rotation_a, last_segment_relative_pos),
				constraint.frame_b = btTransform(rotation_b, Vec3(0, 0, ROPE_SEGMENT_LENGTH * -0.5f));
				constraint.limits = Vec3(PI, PI, 0);
				constraint.a = last_segment;
				constraint.b = box->get<RigidBody>();
				RigidBody::add_constraint(constraint);

				box->get<RigidBody>()->set_ccd(true);
				box->get<RigidBody>()->set_damping(0.5f, 0.5f);
				last_segment = box->get<RigidBody>();
				last_segment_relative_pos = Vec3(0, 0, ROPE_SEGMENT_LENGTH * 0.5f);
				Net::finalize(box);
			}
			else
				break;
		}
		else
			break;
	}

	if (last_segment == start) // we didn't add any rope segments
		return nullptr;
	else
		return last_segment;
}

Rope* Rope::start(RigidBody* start, const Vec3& abs_pos, const Vec3& abs_normal, const Quat& abs_rot, r32 slack)
{
	Entity* base = World::create<Prop>(Asset::Mesh::rope_base);
	base->get<Transform>()->absolute(abs_pos, Quat::look(abs_normal));
	base->get<Transform>()->reparent(start->get<Transform>());
	Net::finalize(base);

	// add the first rope segment
	Vec3 p = abs_pos + abs_normal * ROPE_RADIUS;
	Transform* start_trans = start->get<Transform>();
	RigidBody* rope = rope_add(start, start_trans->to_local(p), p + abs_rot * Vec3(0, 0, ROPE_SEGMENT_LENGTH), abs_rot, slack, RigidBody::Constraint::Type::PointToPoint);
	vi_assert(rope); // should never happen
	return rope->get<Rope>();
}

void Rope::end(const Vec3& pos, const Vec3& normal, RigidBody* end, r32 slack)
{
	Vec3 abs_pos = pos + normal * ROPE_RADIUS;
	RigidBody* start = get<RigidBody>();
	Vec3 start_relative_pos = Vec3(0, 0, ROPE_SEGMENT_LENGTH * 0.5f);
	RigidBody* last = rope_add(start, start_relative_pos, abs_pos, Quat::look(Vec3::normalize(abs_pos - get<Transform>()->to_world(start_relative_pos))), slack, RigidBody::Constraint::Type::ConeTwist);
	if (!last) // we didn't need to add any rope segments; just attach ourselves to the end point
		last = start;

	RigidBody::Constraint constraint;
	constraint.type = RigidBody::Constraint::Type::PointToPoint;
	constraint.frame_a = btTransform(Quat::identity, start_relative_pos);
	constraint.frame_b = btTransform(Quat::identity, end->get<Transform>()->to_local(abs_pos));
	constraint.a = last;
	constraint.b = end;
	RigidBody::add_constraint(constraint);
}

void Rope::spawn(const Vec3& pos, const Vec3& dir, r32 max_distance, r32 slack)
{
	Vec3 dir_normalized = Vec3::normalize(dir);
	Vec3 start_pos = pos;
	Vec3 end = start_pos + dir_normalized * max_distance;
	btCollisionWorld::ClosestRayResultCallback ray_callback(start_pos, end);
	Physics::raycast(&ray_callback, btBroadphaseProxy::AllFilter);
	if (ray_callback.hasHit())
	{
		Vec3 end2 = start_pos + dir_normalized * -max_distance;

		btCollisionWorld::ClosestRayResultCallback ray_callback2(start_pos, end2);
		Physics::raycast(&ray_callback2, btBroadphaseProxy::AllFilter);

		if (ray_callback2.hasHit())
		{
			RigidBody* a = Entity::list[ray_callback.m_collisionObject->getUserIndex()].get<RigidBody>();
			RigidBody* b = Entity::list[ray_callback2.m_collisionObject->getUserIndex()].get<RigidBody>();

			Transform* a_trans = a->get<Transform>();
			Transform* b_trans = b->get<Transform>();

			Rope* rope = Rope::start(a, ray_callback.m_hitPointWorld, ray_callback.m_hitNormalWorld, Quat::look(ray_callback.m_hitNormalWorld), slack);
			if (rope)
				rope->end(ray_callback2.m_hitPointWorld, ray_callback2.m_hitNormalWorld, b, slack);
		}
	}
}

WaterEntity::WaterEntity(AssetID mesh_id)
{
	create<Transform>();
	create<Water>(mesh_id);
}

ShockwaveEntity::ShockwaveEntity(r32 max_radius, r32 duration)
{
	create<Transform>();

	PointLight* light = create<PointLight>();
	light->radius = 0.0f;
	light->type = PointLight::Type::Shockwave;

	Shockwave* shockwave = create<Shockwave>();
	shockwave->max_radius = max_radius;
	shockwave->duration = duration;
}

r32 Shockwave::radius() const
{
	return get<PointLight>()->radius;
}

void Shockwave::update(const Update& u)
{
	timer += u.time.delta;
	if (timer > duration)
		World::remove(entity());
	else
	{
		PointLight* light = get<PointLight>();
		r32 fade_radius = max_radius * (2.0f / 15.0f);
		light->radius = Ease::cubic_out(timer / duration, 0.0f, max_radius);
		r32 fade = 1.0f - vi_max(0.0f, ((light->radius - (max_radius - fade_radius)) / fade_radius));
		light->color = Vec3(fade * 0.8f);
	}
}


}
