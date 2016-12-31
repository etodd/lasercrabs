#include "entities.h"
#include "data/animator.h"
#include "data/components.h"
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
#include "parkour.h"
#include "overworld.h"
#include "common.h"
#include "player.h"
#include "load.h"

namespace VI
{


AwkEntity::AwkEntity(AI::Team team)
{
	create<Audio>();
	create<Transform>();
	create<Awk>();
	create<AIAgent>()->team = team;
	create<Health>(AWK_HEALTH, AWK_HEALTH, AWK_SHIELD, AWK_SHIELD);

	SkinnedModel* model = create<SkinnedModel>();
	model->mesh = Asset::Mesh::awk;
	model->shader = Asset::Shader::armature;
	model->team = s8(team);

	Animator* anim = create<Animator>();
	anim->armature = Asset::Armature::awk;

	create<Target>();
	create<RigidBody>(RigidBody::Type::Sphere, Vec3(AWK_SHIELD_RADIUS), 0.0f, CollisionShield, CollisionDefault, AssetNull);
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

void spawn_sparks(const Vec3& pos, const Quat& rot, Transform* parent)
{
	for (s32 i = 0; i < 15; i++)
	{
		Particles::sparks.add
		(
			pos,
			rot * Vec3(mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo()) * 10.0f,
			Vec4(1, 1, 1, 1)
		);
	}

	Shockwave::add(pos, 3.0f, 0.25f, Shockwave::Type::Light, parent);
}

template<typename Stream> b8 serialize_health_event(Stream* p, Health* h, HealthEvent* e)
{
	serialize_ref(p, e->source);
	if (h->hp_max > 0)
		serialize_int(p, s8, e->hp, -h->hp_max, h->hp_max);
	else if (Stream::IsReading)
		e->hp = 0;
	if (h->shield_max > 0)
		serialize_int(p, s8, e->shield, -h->shield_max, h->shield_max);
	else if (Stream::IsReading)
		e->shield = 0;
	return true;
}

b8 Health::net_msg(Net::StreamRead* p)
{
	using Stream = Net::StreamRead;
	Ref<Health> ref;
	serialize_ref(p, ref);

	HealthEvent e;
	if (!serialize_health_event(p, ref.ref(), &e))
		net_error();

	Health* h = ref.ref();
	h->hp += e.hp;
	h->shield += e.shield;
	h->changed.fire(e);
	if (h->hp == 0)
		h->killed.fire(e.source.ref());

	return true;
}

// only called on server
b8 send_health_event(Health* h, HealthEvent* e)
{
	using Stream = Net::StreamWrite;
	Net::StreamWrite* p = Net::msg_new(Net::MessageType::Health);

	Ref<Health> ref = h;
	serialize_ref(p, ref);

	if (!serialize_health_event(p, ref.ref(), e))
		net_error();

	Net::msg_finalize(p);

	return true;
}

#define REGEN_DELAY 9.0f
#define REGEN_TIME 1.0f

// only called on server
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
				HealthEvent e =
				{
					nullptr,
					0,
					1,
				};
				send_health_event(this, &e);
			}
		}
	}
}

void Health::damage(Entity* e, s8 damage)
{
	vi_assert(Game::level.local);
	if (hp > 0 && damage > 0)
	{
		s8 damage_accumulator = damage;
		s8 damage_shield;
		if (damage_accumulator > shield)
		{
			damage_shield = shield;
			damage_accumulator -= shield;
		}
		else
		{
			damage_shield = damage_accumulator;
			damage_accumulator = 0;
		}

		s8 damage_hp;
		if (damage_accumulator > hp)
			damage_hp = hp;
		else
			damage_hp = damage_accumulator;

		regen_timer = REGEN_TIME + REGEN_DELAY;

		HealthEvent ev =
		{
			e,
			s8(-damage_hp),
			s8(-damage_shield),
		};
		send_health_event(this, &ev);
	}
}

void Health::take_shield()
{
	damage(nullptr, shield);
}

void Health::reset_hp()
{
	if (hp < hp_max)
		add(hp_max - hp);
}

void Health::kill(Entity* e)
{
	damage(e, hp_max + shield_max);
}

void Health::add(s8 amount)
{
	vi_assert(Game::level.local);
	amount = vi_min(amount, s8(hp_max - hp));
	if (amount > 0)
	{
		HealthEvent e =
		{
			nullptr,
			amount,
			0,
		};
		send_health_event(this, &e);
	}
}

s8 Health::total() const
{
	return hp + shield;
}

EnergyPickupEntity::EnergyPickupEntity(const Vec3& p, AI::Team team)
{
	create<Transform>()->pos = p;
	View* model = create<View>();
	model->color = Vec4(0.6f, 0.6f, 0.6f, MATERIAL_NO_OVERRIDE);
	model->mesh = Asset::Mesh::target;
	model->shader = Asset::Shader::standard;

	create<AICue>(AICue::Type::Sensor | AICue::Type::Rocket);

	PointLight* light = create<PointLight>();
	light->type = PointLight::Type::Override;
	light->team = s8(team);
	light->radius = (team == AI::TeamNone) ? 0.0f : SENSOR_RANGE;

	create<Sensor>()->team = team;

	Target* target = create<Target>();

	create<Health>(SENSOR_HEALTH, SENSOR_HEALTH);

	EnergyPickup* pickup = create<EnergyPickup>();
	pickup->team = team;
	model->team = s8(team);

	model->offset.scale(Vec3(ENERGY_PICKUP_RADIUS - 0.2f));

	RigidBody* body = create<RigidBody>(RigidBody::Type::Sphere, Vec3(ENERGY_PICKUP_RADIUS), 0.1f, CollisionAwkIgnore | CollisionTarget, ~CollisionShield & ~CollisionAllTeamsContainmentField & ~CollisionWalker);
	body->set_damping(0.5f, 0.5f);
	body->set_ccd(true);

	Entity* e = World::create<Empty>();
	e->get<Transform>()->parent = get<Transform>();
	e->add<PointLight>()->radius = 8.0f;
	Net::finalize(e);
	pickup->light = e;
}

void EnergyPickup::killed(Entity* e)
{
	if (Game::level.local)
		set_team(AI::TeamNone, e);
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
	link_arg<Entity*, &EnergyPickup::killed>(get<Health>()->killed);
	set_team_client(team);
}

EnergyPickup::~EnergyPickup()
{
	if (Game::level.local && light.ref())
		World::remove_deferred(light.ref());
}

void EnergyPickup::hit(const TargetEvent& e)
{
	if (e.hit_by->has<Awk>() && e.hit_by->get<Awk>()->current_ability == Ability::Sniper)
		set_team(AI::TeamNone, e.hit_by);
	else
		set_team(e.hit_by->get<AIAgent>()->team, e.hit_by);
}

void EnergyPickup::set_team_client(AI::Team t)
{
	team = t;
	get<View>()->team = s8(t);
	get<PointLight>()->team = s8(t);
	get<PointLight>()->radius = (t == AI::TeamNone) ? 0.0f : SENSOR_RANGE;
	get<Sensor>()->team = t;
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
	pickup->set_team_client(t);
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
	vi_assert(Game::level.local);

	// must be neutral or owned by an enemy
	if (t != team)
	{
		get<Health>()->reset_hp();

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
	if (!Overworld::active())
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
	b8 emit_particles = !Overworld::active() && (s32)(power_particle_timer / particle_interval) < (s32)((power_particle_timer + u.time.delta) / particle_interval);
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

	create<RigidBody>(RigidBody::Type::Mesh, Vec3::zero, 0.0f, CollisionStatic | CollisionParkour, ~CollisionStatic, Asset::Mesh::control_point);
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
	get<PointLight>()->team = s8(team);
	get<View>()->team = s8(team);
	if (Game::level.local && obstacle_id == u32(-1))
	{
		Vec3 pos = get<Transform>()->absolute_pos();
		pos.y -= 1.5f;
		obstacle_id = AI::obstacle_add(pos, 1.0f, 3.0f);
	}
}

namespace ControlPointNet
{
	template<typename Stream> b8 serialize_update(Stream* p, ControlPoint* c)
	{
		serialize_r32_range(p, c->capture_timer, 0, CONTROL_POINT_CAPTURE_TIME, 16);
		serialize_s8(p, c->team_next);
		serialize_s8(p, c->team);
		return true;
	}

	b8 send_update(ControlPoint* c)
	{
		vi_assert(Game::level.local);
		using Stream = Net::StreamWrite;
		Stream* p = Net::msg_new(Net::MessageType::ControlPoint);
		{
			Ref<ControlPoint> ref = c;
			serialize_ref(p, ref);
		}
		if (!serialize_update(p, c))
			vi_assert(false);
		Net::msg_finalize(p);
		return true;
	}
}

void control_point_notify_captured(ControlPoint* c)
{
	for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->get<PlayerManager>()->team.ref()->team() == c->team)
			i.item()->msg(_(strings::control_point_captured), true);
		else
			i.item()->msg(_(strings::control_point_lost), false);
	}
}

b8 ControlPoint::net_msg(Net::StreamRead* p)
{
	using Stream = Net::StreamRead;
	Ref<ControlPoint> ref;
	serialize_ref(p, ref);
	ControlPoint* c = ref.ref();
	AI::Team team_original = c->team;
	if (!c || !ControlPointNet::serialize_update(p, c))
		net_error();
	if (!Game::level.local && c->team != team_original && c->team == 1) // captured by attackers
		control_point_notify_captured(c);
	c->get<PointLight>()->team = s8(c->team);
	c->get<View>()->team = s8(c->team);
	return true;
}

ControlPoint::~ControlPoint()
{
	if (obstacle_id != u32(-1))
		AI::obstacle_remove(obstacle_id);
}

void ControlPoint::capture_start(AI::Team t)
{
	vi_assert(Game::level.local);
	vi_assert(team_next == AI::TeamNone);
	// no capture in progress; start capturing
	team_next = t;
	capture_timer = CONTROL_POINT_CAPTURE_TIME * (team == AI::TeamNone ? 0.5f : 1.0f);
	ControlPointNet::send_update(this);
}

void ControlPoint::capture_cancel()
{
	vi_assert(Game::level.local);
	team_next = AI::TeamNone;
	capture_timer = 0.0f;
	ControlPointNet::send_update(this);
}

b8 ControlPoint::owned_by(AI::Team t) const
{
	return (team == t && team_next == AI::TeamNone) || team_next == t;
}

b8 ControlPoint::can_be_captured_by(AI::Team t) const
{
	if (team_next == AI::TeamNone)
		return team != t && (team != 1 || t != 0); // once attackers take a control point, defenders can't take it back
	else
		return team_next != t;
}

void ControlPoint::update(const Update& u)
{
	if (capture_timer > 0.0f)
	{
		if (Game::level.feature_level != Game::FeatureLevel::All) // tutorial mode
			capture_timer -= u.time.delta * 10.0f; // go faster
		else
			capture_timer -= u.time.delta;
		if (Game::level.local)
		{
			if (capture_timer <= 0.0f)
			{
				// capture complete
				team = team_next;
				team_next = AI::TeamNone;
				capture_timer = 0.0f;
				if (team == 1)
					control_point_notify_captured(this);
				ControlPointNet::send_update(this);
			}
			else if (capture_timer < CONTROL_POINT_CAPTURE_TIME * 0.5f && capture_timer + u.time.delta >= CONTROL_POINT_CAPTURE_TIME * 0.5f)
			{
				// halfway point
				team = AI::TeamNone;
				ControlPointNet::send_update(this);
			}
		}
	}
}

void ControlPoint::set_team(AI::Team t)
{
	team = t;
	team_next = AI::TeamNone;
	capture_timer = 0.0f;
	ControlPointNet::send_update(this);
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
	model->color = Team::color_enemy;
	model->team = s8(team);
	model->shader = Asset::Shader::standard;
	model->offset.scale(Vec3(SENSOR_RADIUS * 1.2f)); // a little bigger for aesthetic reasons

	create<Health>(SENSOR_HEALTH, SENSOR_HEALTH);

	PointLight* light = create<PointLight>();
	light->type = PointLight::Type::Override;
	light->team = s8(team);
	light->radius = SENSOR_RANGE;

	create<Sensor>(team);

	create<Target>();

	RigidBody* body = create<RigidBody>(RigidBody::Type::Sphere, Vec3(SENSOR_RADIUS), 1.0f, CollisionAwkIgnore | CollisionTarget, ~CollisionShield);
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
	if (Game::level.local)
		World::remove_deferred(entity());
}

void Sensor::update_all_client(const Update& u)
{
	r32 time = u.time.total;
	r32 last_time = time - u.time.delta;
	const r32 sensor_shockwave_interval = 3.0f;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team != AI::TeamNone)
		{
			r32 offset = i.index * sensor_shockwave_interval * 0.3f;
			if ((s32)((time + offset) / sensor_shockwave_interval) != (s32)((last_time + offset) / sensor_shockwave_interval))
				Shockwave::add(i.item()->get<Transform>()->absolute_pos(), 10.0f, 1.5f, Shockwave::Type::Wave);
		}
	}
}

void Sensor::set_team(AI::Team t)
{
	// not synced over network
	team = t;
	get<View>()->team = s8(t);
	get<PointLight>()->team = s8(t);
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
	if (Game::level.local)
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
		if (AI::match(i.item()->team(), mask))
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
	: target(),
	owner(),
	remaining_lifetime(15.0f)
{
}

void Rocket::set_owner(PlayerManager* m)
{
	// not synced over network
	owner = m;
	get<View>()->team = m ? s8(m->team.ref()->team()) : AI::TeamNone;
}

AI::Team Rocket::team() const
{
	return owner.ref() ? owner.ref()->team.ref()->team() : AI::TeamNone;
}

void Rocket::explode()
{
	vi_assert(Game::level.local);
	Vec3 pos;
	Quat rot;
	get<Transform>()->absolute(&pos, &rot);
	ParticleEffect::spawn(ParticleEffect::Type::Impact, pos, rot.inverse());

	World::remove_deferred(entity());
}

void Rocket::update_server(const Update& u)
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
			Vec3 target_pos;
			if (!target.ref()->get<Target>()->predict_intersection(get<Transform>()->pos, ROCKET_SPEED, nullptr, &target_pos))
				target_pos = target.ref()->get<Transform>()->absolute_pos();
			Vec3 to_target = target_pos - get<Transform>()->pos;
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
					Physics::raycast(&ray_callback, ~CollisionTarget & ~CollisionAwkIgnore & ~CollisionShield);
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

		Vec3 velocity = get<Transform>()->rot * Vec3(0, 0, ROCKET_SPEED);
		Vec3 next_pos = get<Transform>()->pos + velocity * u.time.delta;

		btCollisionWorld::ClosestRayResultCallback ray_callback(get<Transform>()->pos, next_pos + get<Transform>()->rot * Vec3(0, 0, 0.1f));
		Physics::raycast(&ray_callback, ~CollisionTarget & ~CollisionAwkIgnore);
		if (ray_callback.hasHit())
		{
			// we hit something
			Entity* hit = &Entity::list[ray_callback.m_collisionObject->getUserIndex()];
			if (!hit->has<AIAgent>() || hit->get<AIAgent>()->team != team()) // fly through friendlies
			{
				// kaboom

				// do damage
				if (hit->has<Awk>() || hit->has<Decoy>())
					hit->get<Health>()->damage(entity(), 1);

				explode();
				return;
			}
		}
		else // keep flying
			get<Transform>()->pos = next_pos;
	}
}

r32 Rocket::particle_accumulator;
void Rocket::update_client_all(const Update& u)
{
	particle_accumulator += u.time.delta;
	const r32 interval = 0.07f;
	if (particle_accumulator > interval)
	{
		particle_accumulator = fmodf(particle_accumulator, interval);
		for (auto i = list.iterator(); !i.is_last(); i.next())
		{
			if (!i.item()->get<Transform>()->parent.ref())
			{
				Particles::tracers.add
				(
					i.item()->get<Transform>()->pos,
					Vec3::zero,
					0
				);
			}
		}
	}
}

RocketEntity::RocketEntity(PlayerManager* owner, Transform* parent, const Vec3& pos, const Quat& rot, AI::Team team)
{
	Transform* transform = create<Transform>();
	transform->parent = parent;
	transform->absolute(pos + rot * Vec3(0, 0, 0.11f), rot);

	Rocket* rocket = create<Rocket>();
	rocket->owner = owner;

	create<Health>(SENSOR_HEALTH, SENSOR_HEALTH);

	View* model = create<View>();
	model->mesh = Asset::Mesh::rocket_pod;
	model->color = Team::color_enemy;
	model->team = s8(team);
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
	anim->layers[0].behavior = Animator::Behavior::Loop;
	anim->layers[0].play(Asset::Animation::awk_dash);

	create<Target>();

	create<RigidBody>(RigidBody::Type::Sphere, Vec3(AWK_SHIELD_RADIUS), 0.0f, CollisionShield, CollisionDefault);

	create<Decoy>()->owner = owner;
}

void Decoy::awake()
{
	link_arg<const TargetEvent&, &Decoy::hit_by>(get<Target>()->target_hit);
	link_arg<Entity*, &Decoy::killed>(get<Health>()->killed);

	if (Game::level.local)
	{
		Entity* shield_entity = World::create<Empty>();
		shield_entity->get<Transform>()->parent = get<Transform>();
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
}

Decoy::~Decoy()
{
	if (Game::level.local)
	{
		if (shield.ref())
			World::remove_deferred(shield.ref());
	}
}

AI::Team Decoy::team() const
{
	return owner.ref() ? owner.ref()->team.ref()->team() : AI::TeamNone;
}

void Decoy::killed(Entity*)
{
	if (Game::level.local)
		destroy();
}

void Decoy::destroy()
{
	vi_assert(Game::level.local);
	Vec3 pos;
	Quat rot;
	get<Transform>()->absolute(&pos, &rot);
	ParticleEffect::spawn(ParticleEffect::Type::Explosion, pos, rot);
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
	if (Game::level.local)
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
		if (Game::level.local && i.item()->remaining_lifetime < 0.0f)
			i.item()->destroy();
	}
}

void ContainmentField::destroy()
{
	vi_assert(Game::level.local);
	Vec3 pos;
	Quat rot;
	get<Transform>()->absolute(&pos, &rot);
	ParticleEffect::spawn(ParticleEffect::Type::Explosion, pos, rot);
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
	create<RigidBody>(RigidBody::Type::Sphere, Vec3(CONTAINMENT_FIELD_BASE_RADIUS), 0.0f, CollisionAwkIgnore | CollisionTarget, ~CollisionStatic & ~CollisionShield);

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

	CollisionGroup team_mask = CollisionGroup(1 << (8 + team));

	f->add<RigidBody>(RigidBody::Type::Mesh, Vec3::zero, 0.0f, team_mask, CollisionAwkIgnore, view->mesh);

	Net::finalize(f);

	field->field = f;
}

#define TELEPORTER_RADIUS 0.5f
#define PROJECTILE_LENGTH 0.5f
#define PROJECTILE_THICKNESS 0.05f
#define PROJECTILE_MAX_LIFETIME 10.0f
#define PROJECTILE_DAMAGE 1
ProjectileEntity::ProjectileEntity(PlayerManager* owner, const Vec3& abs_pos, const Vec3& velocity)
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

AI::Team Projectile::team() const
{
	return owner.ref() ? owner.ref()->team.ref()->team() : AI::TeamNone;
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
	s16 mask = ~Team::containment_field_mask(team());
	Physics::raycast(&ray_callback, mask);
	if (ray_callback.hasHit())
	{
		Entity* hit_object = &Entity::list[ray_callback.m_collisionObject->getUserIndex()];
		if (!owner.ref() || hit_object != owner.ref()->instance.ref())
		{
			Entity* owner_instance = owner.ref() ? owner.ref()->instance.ref() : nullptr;
			Vec3 basis;
			if (hit_object->has<Health>())
			{
				basis = Vec3::normalize(velocity);
				b8 do_damage = true;
				if (hit_object->has<Parkour>()) // player is invincible while rolling and sliding
				{
					Parkour::State state = hit_object->get<Parkour>()->fsm.current;
					if (state == Parkour::State::Roll || state == Parkour::State::Slide)
						do_damage = false;
				}
				if (hit_object->has<Awk>()) // player is invincible while flying or dashing
				{
					if (hit_object->get<Awk>()->state() != Awk::State::Crawl)
						do_damage = false;
				}
				if (do_damage)
					hit_object->get<Health>()->damage(owner_instance, PROJECTILE_DAMAGE);
				if (hit_object->has<RigidBody>())
				{
					RigidBody* body = hit_object->get<RigidBody>();
					body->btBody->applyImpulse(velocity * 0.1f, Vec3::zero);
					body->btBody->activate(true);
				}
			}
			else
				basis = ray_callback.m_hitNormalWorld;

			ParticleEffect::spawn(ParticleEffect::Type::Impact, ray_callback.m_hitPointWorld, Quat::look(basis));
			World::remove(entity());
		}
	}
	else
		get<Transform>()->absolute_pos(next_pos);
}

b8 ParticleEffect::spawn(Type t, const Vec3& pos, const Quat& rot)
{
	vi_assert(Game::level.local);
	using Stream = Net::StreamWrite;
	Net::StreamWrite* p = Net::msg_new(Net::MessageType::ParticleEffect);

	serialize_enum(p, Type, t);
	Vec3 pos2 = pos;
	Net::serialize_position(p, &pos2, Net::Resolution::Low);
	Quat rot2 = rot;
	Net::serialize_quat(p, &rot2, Net::Resolution::Low);

	Net::msg_finalize(p);

	return true;
}

b8 ParticleEffect::net_msg(Net::StreamRead* p)
{
	using Stream = Net::StreamRead;
	Type t;
	serialize_enum(p, Type, t);

	Vec3 pos;
	if (!Net::serialize_position(p, &pos, Net::Resolution::Low))
		net_error();
	Quat rot;
	if (!Net::serialize_quat(p, &rot, Net::Resolution::Low))
		net_error();

	if (t == Type::Grenade || t == Type::Explosion)
	{
		Audio::post_global_event(AK::EVENTS::PLAY_EXPLOSION, pos);
		Shockwave::add(pos, 8.0f, 0.35f, Shockwave::Type::Alpha);
	}

	if (t == Type::Grenade)
	{
		for (auto i = PlayerControlHuman::list.iterator(); !i.is_last(); i.next())
		{
			r32 distance = (i.item()->get<Transform>()->absolute_pos() - pos).length();
			if (distance < GRENADE_RANGE * 1.5f)
				i.item()->camera_shake(LMath::lerpf(vi_max(0.0f, (distance - (GRENADE_RANGE * 0.66f)) / (GRENADE_RANGE * (1.5f - 0.66f))), 1.0f, 0.0f));
		}

		for (auto i = Health::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->has<RigidBody>())
			{
				Vec3 to_item = i.item()->get<Transform>()->absolute_pos() - pos;
				r32 distance = to_item.length();
				to_item /= distance;
				if (distance < GRENADE_RANGE)
				{
					RigidBody* body = i.item()->get<RigidBody>();
					body->btBody->applyImpulse(to_item * LMath::lerpf(distance / GRENADE_RANGE, 1.0f, 0.0f) * 10.0f, Vec3::zero);
					body->btBody->activate(true);
				}
			}
		}
	}

	if (t == Type::Impact)
		Shockwave::add(pos, GRENADE_RANGE, 1.5f, Shockwave::Type::Wave);

	for (s32 i = 0; i < 50; i++)
	{
		Particles::sparks.add
		(
			pos,
			rot * Vec3(mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo()) * 10.0f,
			Vec4(1, 1, 1, 1)
		);
	}
	return true;
}

GrenadeEntity::GrenadeEntity(PlayerManager* owner, const Vec3& abs_pos, const Vec3& velocity)
{
	Transform* transform = create<Transform>();
	transform->absolute_pos(abs_pos);

	create<Audio>();

	Grenade* g = create<Grenade>();
	g->owner = owner;
	g->velocity = Vec3::normalize(velocity) * GRENADE_LAUNCH_SPEED;

	create<RigidBody>(RigidBody::Type::Sphere, Vec3(GRENADE_RADIUS), 0.0f, CollisionAwkIgnore | CollisionTarget, ~CollisionShield);

	View* model = create<View>();
	model->mesh = Asset::Mesh::sphere_highres;
	model->color = Team::color_enemy;
	model->team = s8(owner->team.ref()->team());
	model->shader = Asset::Shader::standard;
	model->offset.scale(Vec3(GRENADE_RADIUS));

	create<Health>(SENSOR_HEALTH, SENSOR_HEALTH);

	create<Target>();
}

template<typename T> b8 grenade_trigger_filter(T* e, AI::Team team)
{
	return (e->template has<AIAgent>() && e->template get<AIAgent>()->team != team && !e->template get<AIAgent>()->stealth)
		|| (e->template has<ContainmentField>() && e->template get<ContainmentField>()->team != team)
		|| (e->template has<Rocket>() && e->template get<Rocket>()->team() != team)
		|| (e->template has<Sensor>() && !e->template has<EnergyPickup>() && e->template get<Sensor>()->team != team)
		|| (e->template has<Decoy>() && e->template get<Decoy>()->team() != team);
}

void Grenade::update_server(const Update& u)
{
	Transform* t = get<Transform>();
	if (!t->parent.ref())
	{
		Vec3 pos = t->absolute_pos();
		Vec3 next_pos;
		{
			Vec3 half_accel = Physics::btWorld->getGravity() * u.time.delta * 0.5f;
			velocity += half_accel;
			next_pos = pos + velocity * u.time.delta;
			velocity += half_accel;
		}
		if (next_pos.y < Game::level.min_y)
		{
			World::remove_deferred(entity());
			return;
		}

		if (!btVector3(next_pos - pos).fuzzyZero())
		{
			Vec3 v = velocity;
			if (v.length_squared() > 0.0f)
				v.normalize();
			btCollisionWorld::ClosestRayResultCallback ray_callback(pos, next_pos + v * GRENADE_RADIUS);
			Physics::raycast(&ray_callback, ~Team::containment_field_mask(team()) & ~CollisionAwkIgnore & ~CollisionTarget);
			if (ray_callback.hasHit())
			{
				Entity* e = &Entity::list[ray_callback.m_collisionObject->getUserIndex()];
				if (grenade_trigger_filter(e, team()))
					explode();
				else
				{
					if (active)
					{
						// attach
						velocity = Vec3::zero;
						t->parent = e->get<Transform>();
						next_pos = ray_callback.m_hitPointWorld + ray_callback.m_hitNormalWorld * GRENADE_RADIUS * 1.1f;
						t->absolute_rot(Quat::look(ray_callback.m_hitNormalWorld));
					}
					else
					{
						// bounce
						velocity = velocity.reflect(ray_callback.m_hitNormalWorld) * 0.5f;
					}
					active = true;
				}
			}
		}
		t->absolute_pos(next_pos);
	}

	if (active && timer > GRENADE_DELAY)
		explode();
}

void Grenade::explode()
{
	vi_assert(Game::level.local);
	Vec3 me = get<Transform>()->absolute_pos();
	ParticleEffect::spawn(ParticleEffect::Type::Grenade, me, Quat::look(Vec3(0, 1, 0)));

	for (auto i = Health::list.iterator(); !i.is_last(); i.next())
	{
		Vec3 to_item = i.item()->get<Transform>()->absolute_pos() - me;
		r32 distance = to_item.length();
		to_item /= distance;
		if (i.item()->has<Awk>())
		{
			if (distance < GRENADE_RANGE * 0.66f)
				i.item()->damage(entity(), 1);
		}
		else if (distance < GRENADE_RANGE && !i.item()->has<EnergyPickup>())
			i.item()->damage(entity(), distance < GRENADE_RANGE * 0.5f ? 3 : (distance < GRENADE_RANGE * 0.75f ? 2 : 1));
	}

	World::remove_deferred(entity());
}

void Grenade::set_owner(PlayerManager* m)
{
	// not synced over network
	owner = m;
	get<View>()->team = m ? s8(m->team.ref()->team()) : AI::TeamNone;
	if (!m)
		active = false;
}

AI::Team Grenade::team() const
{
	return owner.ref() ? owner.ref()->team.ref()->team() : AI::TeamNone;
}

r32 Grenade::particle_accumulator;
void Grenade::update_client_all(const Update& u)
{
	// normal particles
	const r32 interval = 0.1f;
	particle_accumulator += u.time.delta;
	while (particle_accumulator > interval)
	{
		particle_accumulator -= interval;
		for (auto i = list.iterator(); !i.is_last(); i.next())
		{
			Transform* t = i.item()->get<Transform>();
			if (t->parent.ref())
			{
				View* v = i.item()->get<View>();
				if (v->mesh != Asset::Mesh::grenade_attached)
				{
					v->mesh = Asset::Mesh::grenade_attached;
					i.item()->get<Audio>()->post_event(AK::EVENTS::PLAY_BEEP_GRENADE);
				}
			}
			else
				Particles::tracers.add(t->absolute_pos(), Vec3::zero, 0);
		}
	}

	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->active)
		{
			Vec3 me = i.item()->get<Transform>()->absolute_pos();
			const r32 interval = 3.0f;
			if (s32(Game::time.total / interval) != s32((Game::time.total - u.time.delta) / interval))
				Shockwave::add(me, GRENADE_RANGE, 1.5f, Shockwave::Type::Wave);
			AI::Team my_team = i.item()->team();
			b8 countdown = false;
			for (auto i = Health::list.iterator(); !i.is_last(); i.next())
			{
				if (grenade_trigger_filter(i.item(), my_team)
					&& (i.item()->get<Transform>()->absolute_pos() - me).length_squared() < GRENADE_RANGE * GRENADE_RANGE)
				{
					countdown = true;
					break;
				}
			}
			if (countdown)
			{
				i.item()->timer += u.time.delta;
				r32 interval = LMath::lerpf(vi_min(1.0f, i.item()->timer / GRENADE_DELAY), 0.35f, 0.05f);
				if (s32(i.item()->timer / interval) != s32((i.item()->timer - u.time.delta) / interval))
					i.item()->get<Audio>()->post_event(AK::EVENTS::PLAY_BEEP_GRENADE);
			}
			else
				i.item()->timer = 0.0f;
		}
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
	if (Game::level.local)
		World::remove_deferred(entity());
}

Vec3 Target::velocity() const
{
	if (has<Awk>())
		return get<Awk>()->velocity;
	else if (Game::level.local)
		return get<RigidBody>()->btBody->getInterpolationLinearVelocity();
	else
		return net_velocity;
}

b8 Target::predict_intersection(const Vec3& from, r32 speed, const Net::StateFrame* state_frame, Vec3* intersection) const
{
	Vec3 pos;
	Vec3 v;
	if (state_frame)
	{
		Quat rot;
		Net::transform_absolute(*state_frame, get<Transform>()->id(), &pos, &rot);
		pos += rot * local_offset; // todo possibly: rewind local_offset as well?

		Net::StateFrame state_frame_last;
		Net::state_frame_by_timestamp(&state_frame_last, state_frame->timestamp - NET_TICK_RATE);
		Vec3 pos_last;
		Quat rot_last;
		Net::transform_absolute(state_frame_last, get<Transform>()->id(), &pos_last, &rot_last);
		pos_last += rot_last * local_offset;

		v = (pos - pos_last) / NET_TICK_RATE;
	}
	else
	{
		v = velocity();
		pos = absolute_pos();
	}

	Vec3 to_target = pos - from;
	r32 intersect_time_squared = to_target.dot(to_target) / ((speed * speed) - 2.0f * to_target.dot(v) - v.dot(v));
	if (intersect_time_squared > 0.0f)
	{
		*intersection = pos + v * sqrtf(intersect_time_squared);
		return true;
	}
	else
		return false;
}

r32 Target::radius() const
{
	if (has<MinionCommon>())
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

b8 PlayerTrigger::is_triggered() const
{
	for (s32 i = 0; i < max_trigger; i++)
	{
		if (triggered[i].ref())
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
void Rope::draw(const RenderParams& params)
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

	Loader::shader_permanent(Asset::Shader::flat_instanced);

	RenderSync* sync = params.sync;
	sync->write(RenderOp::Shader);
	sync->write(Asset::Shader::flat_instanced);
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
	sync->write<Vec4>(Vec4(1, 1, 1, 1));

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
				if (last_segment->has<Rope>())
					Net::finalize(last_segment->entity());

				Vec3 spawn_pos = last_segment_pos + (diff / length) * rope_interval * 0.5f;
				Entity* box = World::create<PhysicsEntity>(AssetNull, spawn_pos, rot, RigidBody::Type::CapsuleZ, Vec3(ROPE_RADIUS, ROPE_SEGMENT_LENGTH - ROPE_RADIUS * 2.0f, 0.0f), 0.05f, CollisionAwkIgnore, ~CollisionWalker & ~CollisionAllTeamsContainmentField);
				box->add<Rope>();

				static Quat rotation_a = Quat::look(Vec3(0, 0, 1)) * Quat::euler(0, PI * -0.5f, 0);
				static Quat rotation_b = Quat::look(Vec3(0, 0, -1)) * Quat::euler(PI, PI * -0.5f, 0);

				RigidBody::Constraint constraint = RigidBody::Constraint();
				constraint.type = constraint_type;
				constraint.frame_a = btTransform(rotation_b, Vec3(0, 0, ROPE_SEGMENT_LENGTH * -0.5f));
				constraint.frame_b = btTransform(rotation_a, last_segment_relative_pos),
				constraint.limits = Vec3(PI, PI, 0);
				constraint.a = box->get<RigidBody>(); // this must be constraint A in order for the netcode to pick up on the constraint
				constraint.b = last_segment;
				RigidBody::add_constraint(constraint);

				box->get<RigidBody>()->set_ccd(true);
				box->get<RigidBody>()->set_damping(0.5f, 0.5f);
				last_segment = box->get<RigidBody>();
				last_segment_relative_pos = Vec3(0, 0, ROPE_SEGMENT_LENGTH * 0.5f);
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

void Rope::end(const Vec3& pos, const Vec3& normal, RigidBody* end, r32 slack, b8 add_cap)
{
	Vec3 abs_pos = pos + normal * ROPE_RADIUS;
	RigidBody* start = get<RigidBody>();
	Vec3 start_relative_pos = Vec3(0, 0, ROPE_SEGMENT_LENGTH * 0.5f);
	RigidBody* last = rope_add(start, start_relative_pos, abs_pos, Quat::look(Vec3::normalize(abs_pos - get<Transform>()->to_world(start_relative_pos))), slack, RigidBody::Constraint::Type::ConeTwist);
	if (!last) // we didn't need to add any rope segments; just attach ourselves to the end point
		last = start;

	if (add_cap)
	{
		Entity* base = World::create<Prop>(Asset::Mesh::rope_base);
		base->get<Transform>()->absolute(pos, Quat::look(normal));
		base->get<Transform>()->reparent(end->get<Transform>());
		Net::finalize(base);
	}

	RigidBody::Constraint constraint = RigidBody::Constraint();
	constraint.type = RigidBody::Constraint::Type::PointToPoint;
	constraint.frame_a = btTransform(Quat::identity, start_relative_pos);
	constraint.frame_b = btTransform(Quat::identity, end->get<Transform>()->to_local(abs_pos));
	constraint.a = last;
	constraint.b = end; // this must be constraint A in order for the netcode to pick up on the constraint
	RigidBody::add_constraint(constraint);

	Net::finalize(last->entity());
}

void Rope::spawn(const Vec3& pos, const Vec3& dir, r32 max_distance, r32 slack, b8 attach_end)
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

		if (!attach_end || ray_callback2.hasHit())
		{
			RigidBody* a = Entity::list[ray_callback.m_collisionObject->getUserIndex()].get<RigidBody>();

			Rope* rope = Rope::start(a, ray_callback.m_hitPointWorld, ray_callback.m_hitNormalWorld, Quat::look(ray_callback.m_hitNormalWorld), slack);

			if (rope)
			{
				if (attach_end && ray_callback2.hasHit())
				{
					// attach on both ends
					RigidBody* b = Entity::list[ray_callback2.m_collisionObject->getUserIndex()].get<RigidBody>();
					rope->end(ray_callback2.m_hitPointWorld, ray_callback2.m_hitNormalWorld, b, slack);
				}
				else
				{
					// only attached on one end
					Vec3 start_relative_pos = Vec3(0, 0, ROPE_SEGMENT_LENGTH * 0.5f);
					rope_add(rope->get<RigidBody>(), start_relative_pos, end2, Quat::look(Vec3::normalize(end2 - rope->get<Transform>()->to_world(start_relative_pos))), slack, RigidBody::Constraint::Type::ConeTwist);
				}
			}
		}
	}
}

WaterEntity::WaterEntity(AssetID mesh_id)
{
	create<Transform>();
	create<Water>(mesh_id);
}

PinArray<Shockwave, MAX_ENTITIES> Shockwave::list;

void Shockwave::add(const Vec3& pos, r32 radius, r32 duration, Type t, Transform* parent)
{
	Shockwave* s = list.add();
	new (s) Shockwave();
	s->pos = parent ? parent->to_local(pos) : pos;
	s->max_radius = radius;
	s->duration = duration;
	s->type = t;
	s->parent = parent;
}

void Shockwave::draw_alpha(const RenderParams& params)
{
	// "Light" and "Wave" type shockwaves get rendered in loop.h, not here
	const Mesh* mesh = Loader::mesh_permanent(Asset::Mesh::sphere_highres);
	Loader::shader_permanent(Asset::Shader::fresnel);

	RenderSync* sync = params.sync;
	sync->write(RenderOp::Shader);
	sync->write(Asset::Shader::fresnel);
	sync->write(params.technique);

	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		r32 radius = i.item()->radius();
		Vec3 pos = i.item()->absolute_pos();
		if (i.item()->type != Type::Alpha || !params.camera->visible_sphere(pos, radius))
			continue;

		Mat4 m;
		m.make_transform(pos, Vec3(radius), Quat::identity);
		Mat4 mvp = m * params.view_projection;

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::mvp);
		sync->write(RenderDataType::Mat4);
		sync->write<s32>(1);
		sync->write<Mat4>(mvp);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::diffuse_color);
		sync->write(RenderDataType::Vec4);
		sync->write<s32>(1);
		sync->write<Vec4>(Vec4(1, 1, 1, i.item()->opacity()));

		sync->write(RenderOp::Mesh);
		sync->write(RenderPrimitiveMode::Triangles);
		sync->write(Asset::Mesh::sphere_highres);
	}
}

r32 Shockwave::radius() const
{
	switch (type)
	{
		case Type::Wave:
		{
			return Ease::cubic_out(timer / duration, 0.0f, max_radius);
		}
		case Type::Light:
		{
			r32 blend = Ease::cubic_in<r32>(timer / duration);
			return LMath::lerpf(blend, max_radius * 0.25f, max_radius);
		}
		case Type::Alpha:
		{
			r32 blend = Ease::cubic_in<r32>(timer / duration);
			return LMath::lerpf(blend, 0.0f, max_radius);
		}
		default:
		{
			vi_assert(false);
			return 0.0f;
		}
	}
}

r32 Shockwave::opacity() const
{
	switch (type)
	{
		case Type::Light:
		{
			r32 blend = Ease::cubic_in<r32>(timer / duration);
			return LMath::lerpf(blend, 1.0f, 0.0f);
		}
		case Type::Wave:
		{
			r32 fade_radius = max_radius * (2.0f / 15.0f);
			r32 fade = 1.0f - vi_max(0.0f, ((radius() - (max_radius - fade_radius)) / fade_radius));
			return fade * 0.8f;
		}
		case Type::Alpha:
		{
			r32 blend = Ease::cubic_in<r32>(timer / duration);
			return LMath::lerpf(blend, 0.8f, 0.0f);
		}
		default:
		{
			vi_assert(false);
			return 0.0f;
		}
	}
}

Vec3 Shockwave::absolute_pos() const
{
	Transform* p = parent.ref();
	return p ? p->to_world(pos) : pos;
}

void Shockwave::update(const Update& u)
{
	timer += u.time.delta;
	if (timer > duration)
		list.remove(id());
}

CollectibleEntity::CollectibleEntity(ID save_id, Resource type, s16 amount)
{
	create<Transform>();

	PointLight* light = create<PointLight>();
	light->radius = 6.0f;
	light->offset = Vec3(0, 0, 0.2f);

	Collectible* c = create<Collectible>();
	c->save_id = save_id;
	c->type = type;
	c->amount = amount;
	switch (type)
	{
		case Resource::HackKits:
		case Resource::Energy:
		{
			// simple models
			View* v = create<View>(type == Resource::HackKits ? Asset::Mesh::hack_kit : Asset::Mesh::energy);
			v->shader = Asset::Shader::standard;
			v->color = Vec4(1, 1, 1, MATERIAL_INACCESSIBLE);
			break;
		}
		case Resource::Drones:
		{
			// animated model
			SkinnedModel* model = create<SkinnedModel>();
			model->mesh = Asset::Mesh::awk;
			model->shader = Asset::Shader::armature;
			model->color = Vec4(1, 1, 1, MATERIAL_INACCESSIBLE);
			model->offset.translate(Vec3(0, 0, AWK_RADIUS));

			Animator* anim = create<Animator>();
			anim->armature = Asset::Armature::awk;
			anim->layers[0].behavior = Animator::Behavior::Loop;
			anim->layers[0].animation = Asset::Animation::awk_fly;
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}
}

void Collectible::give_rewards()
{
	s16 a = amount;
	if (a == 0)
	{
		switch (type)
		{
			case Resource::HackKits:
			{
				a = 1;
				break;
			}
			case Resource::Energy:
			{
				a = 1000;
				break;
			}
			case Resource::Drones:
			{
				a = 10;
				break;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}
	}
	if (Game::level.local)
		Overworld::resource_change(type, a);

	Game::save.collectibles.add({ Game::level.id, save_id });

	char msg[512];
	sprintf(msg, _(strings::resource_collected), a, _(Overworld::resource_info[s32(type)].description));
	for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
		i.item()->msg(msg, true);
	collected.fire();
}

Interactable* Interactable::closest(const Vec3& pos)
{
	r32 distance_sq = CONTROL_POINT_RADIUS * 0.35f * CONTROL_POINT_RADIUS * 0.35f;
	Interactable* result = nullptr;
	// find the closest interactable
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		Vec3 i_pos(0.4f, 1.1f, 0);
		Quat i_rot = Quat::identity;
		i.item()->get<Transform>()->to_world(&i_pos, &i_rot);
		Vec3 to_interactable = i_pos - pos;
		r32 d = to_interactable.length_squared();
		if (d < distance_sq)
		{
			distance_sq = d;
			result = i.item();
		}
	}
	return result;
}

Interactable::Interactable(Type t)
	: type(t),
	user_data(),
	interacted()
{
}

void Interactable::awake()
{
	link<&Interactable::animation_callback>(get<Animator>()->trigger(Asset::Animation::interactable_interact, 1.916f));
	link<&Interactable::animation_callback>(get<Animator>()->trigger(Asset::Animation::interactable_interact_disable, 1.916f));
	switch (type)
	{
		case Type::Terminal:
		{
			interacted.link(&TerminalInteractable::interacted);
			break;
		}
		case Type::Tram:
		{
			interacted.link(&TramInteractableEntity::interacted);
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}
}

namespace InteractableNet
{
	b8 send_msg(Interactable* i)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::Interactable);
		{
			Ref<Interactable> ref = i;
			serialize_ref(p, ref);
		}
		Net::msg_finalize(p);
		return true;
	}
}

b8 Interactable::net_msg(Net::StreamRead* p, Net::MessageSource src)
{
	using Stream = Net::StreamRead;
	Ref<Interactable> ref;
	serialize_ref(p, ref);
	Interactable* i = ref.ref();
	if (i && Game::level.mode == Game::Mode::Parkour)
	{
		if (Game::level.local)
		{
			if (src == Net::MessageSource::Remote)
				InteractableNet::send_msg(i); // need to send out this message to everyone
			else if (src == Net::MessageSource::Loopback)
				i->interacted.fire(i);
			else
				vi_assert(false);
		}
		else // client
		{
			if (src == Net::MessageSource::Loopback)
				i->interacted.fire(i);
		}
	}
	return true;
}

void Interactable::interact()
{
	Animator* anim = get<Animator>();
	if (anim->layers[1].animation == AssetNull)
	{
		if (type == Type::Terminal)
		{
			anim->layers[0].animation = Asset::Animation::interactable_disabled;
			anim->layers[1].play(Asset::Animation::interactable_interact_disable);
		}
		else
		{
			anim->layers[0].animation = Asset::Animation::interactable_enabled;
			anim->layers[1].play(Asset::Animation::interactable_interact);
		}
	}
}

void Interactable::interact_no_animation()
{
	InteractableNet::send_msg(this);
}

void Interactable::animation_callback()
{
	InteractableNet::send_msg(this);
}

void TerminalEntity::open()
{
	Animator* animator = Game::level.terminal.ref()->get<Animator>();
	animator->layers[0].play(Asset::Animation::terminal_opened);
	animator->layers[1].play(Asset::Animation::terminal_open);
}

void TerminalEntity::close()
{
	Animator* animator = Game::level.terminal.ref()->get<Animator>();
	animator->layers[0].animation = AssetNull;
	animator->layers[1].play(Asset::Animation::terminal_close);
}

void TerminalEntity::closed()
{
	if (Game::level.local)
		Team::transition_mode(Game::Mode::Pvp);
}

TerminalEntity::TerminalEntity()
{
	Transform* transform = create<Transform>();

	SkinnedModel* model = create<SkinnedModel>();
	model->mesh = Asset::Mesh::terminal;
	model->shader = Asset::Shader::armature;
	model->color = Vec4(1, 1, 1, MATERIAL_INACCESSIBLE);

	Animator* anim = create<Animator>();
	anim->armature = Asset::Armature::terminal;
	anim->layers[0].behavior = Animator::Behavior::Loop;
	anim->layers[0].blend_time = 0.0f;
	anim->layers[0].animation = Game::save.zones[Game::level.id] == ZoneState::Locked ? AssetNull : Asset::Animation::terminal_opened;
	anim->layers[1].blend_time = 0.0f;
	anim->trigger(Asset::Animation::terminal_close, 1.33f).link(&closed);

	RigidBody* body = create<RigidBody>(RigidBody::Type::Mesh, Vec3::zero, 0.0f, CollisionStatic | CollisionInaccessible, ~CollisionStatic & ~CollisionParkour & ~CollisionInaccessible & ~CollisionElectric, Asset::Mesh::terminal_collision);
	body->set_restitution(0.75f);
}

TerminalInteractable::TerminalInteractable()
{
	Transform* transform = create<Transform>();

	SkinnedModel* model = create<SkinnedModel>();
	model->mesh = Asset::Mesh::interactable;
	model->shader = Asset::Shader::armature;
	model->hollow();

	Animator* anim = create<Animator>();
	anim->armature = Asset::Armature::interactable;
	anim->layers[0].behavior = Animator::Behavior::Loop;
	anim->layers[0].animation = Game::save.zones[Game::level.id] == ZoneState::Locked ? Asset::Animation::interactable_enabled : Asset::Animation::interactable_disabled;
	anim->layers[0].blend_time = 0.0f;
	anim->layers[1].blend_time = 0.0f;

	create<Interactable>(Interactable::Type::Terminal);
}

void TerminalInteractable::interacted(Interactable*)
{
	vi_assert(Game::level.mode == Game::Mode::Parkour);

	Animator* animator = Game::level.terminal.ref()->get<Animator>();
	if (animator->layers[1].animation == AssetNull) // make sure nothing's happening already
	{
		ZoneState zone_state = Game::save.zones[Game::level.id];
		if (zone_state == ZoneState::Locked)
		{
			if (Game::level.local)
				Overworld::zone_change(Game::level.id, ZoneState::Hostile);
			for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
				i.item()->msg(_(strings::zone_unlocked), true);
			TerminalEntity::open();
		}
		else if (zone_state == ZoneState::Hostile)
		{
			if (Game::level.local)
			{
				Overworld::resource_change(Resource::HackKits, -1);
				Overworld::resource_change(Resource::Drones, -DEFAULT_RUSH_DRONES);
			}
			TerminalEntity::close();
		}
	}
}

const r32 TRAM_LENGTH = 3.7f * 2.0f;
const r32 TRAM_SPEED_MAX = 10.0f;

TramRunnerEntity::TramRunnerEntity(s8 track, b8 is_front)
{
	create<Transform>();
	View* model = create<View>(Asset::Mesh::tram_runner);
	model->shader = Asset::Shader::standard;
	model->color.w = MATERIAL_INACCESSIBLE;
	RigidBody* body = create<RigidBody>(RigidBody::Type::Mesh, Vec3::zero, 0.0f, CollisionStatic | CollisionInaccessible, ~CollisionStatic & ~CollisionInaccessible & ~CollisionParkour & ~CollisionElectric, Asset::Mesh::tram_runner);
	body->set_restitution(0.75f);
	TramRunner* r = create<TramRunner>();
	r->track = track;
	r->is_front = is_front;

	const Game::TramTrack& t = Game::level.tram_tracks[track];
	r32 offset;
	if (Game::save.zone_last == t.level && !Game::save.zone_current_restore)
	{
		offset = t.points[t.points.length - 1].offset - TRAM_LENGTH;
		r->velocity = -TRAM_SPEED_MAX;
		r->state = TramRunner::State::Arriving;
	}
	else
		offset = 0.0f;

	if (is_front)
	{
		offset += TRAM_LENGTH;
		r->target_offset += TRAM_LENGTH;
	}
	r->set(offset);
}

void TramRunner::set(r32 x)
{
	const Game::TramTrack& t = Game::level.tram_tracks[track];
	while (true)
	{
		const Game::TramTrack::Point current = t.points[offset_index];
		if (offset_index > 0 && x < current.offset)
			offset_index--;
		else if (offset_index < t.points.length - 2 && t.points[offset_index + 1].offset < x)
			offset_index++;
		else
			break;
	}

	const Game::TramTrack::Point current = t.points[offset_index];
	const Game::TramTrack::Point next = t.points[offset_index + 1];
	r32 blend = (x - current.offset) / (next.offset - current.offset);
	Transform* transform = get<Transform>();
	transform->pos = Vec3::lerp(blend, current.pos, next.pos);
	transform->rot = Quat::look(Vec3::normalize(next.pos - current.pos));
	offset = x;
}

void TramRunner::go(s8 track, r32 x, State s)
{
	vi_assert(Game::level.local);
	const Game::TramTrack& t = Game::level.tram_tracks[track];
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->track == track)
		{
			i.item()->state = s;
			i.item()->target_offset = x * (t.points[t.points.length - 1].offset - TRAM_LENGTH);
			if (i.item()->is_front)
				i.item()->target_offset += TRAM_LENGTH;
		}
	}
}

namespace TramNet
{
	enum Message
	{
		Entered,
		Exited,
		DoorsOpen,
		count,
	};

	b8 send(Tram*, Message);
};

void TramRunner::update_server(const Update& u)
{
	const r32 ACCEL_TIME = 5.0f;
	const r32 ACCEL_MAX = TRAM_SPEED_MAX / ACCEL_TIME;
	const r32 ACCEL_DISTANCE = TRAM_SPEED_MAX * ACCEL_TIME - 0.5f * ACCEL_MAX * (ACCEL_TIME * ACCEL_TIME);

	{
		const Game::TramTrack& t = Game::level.tram_tracks[track];
		if (is_front
			&& state == State::Departing
			&& Game::scheduled_load_level == AssetNull
			&& offset > t.points[t.points.length - 1].offset) // we hit our goal, we are the front runner, we're a local game, and we're exiting the level
			Game::schedule_load_level(t.level, Game::Mode::Parkour);
	}

	r32 error = target_offset - offset;
	r32 distance = fabsf(error);
	r32 dv_half = ACCEL_MAX * u.time.delta * 0.5f;
	if (state == State::Departing || distance > ACCEL_DISTANCE) // accelerating to max speed
	{
		get<RigidBody>()->activate_linked();
		if (error > 0.0f)
			velocity = vi_min(TRAM_SPEED_MAX, velocity + dv_half);
		else
			velocity = vi_max(-TRAM_SPEED_MAX, velocity - dv_half);
		set(offset + velocity * u.time.delta);
		if (error > 0.0f)
			velocity = vi_min(TRAM_SPEED_MAX, velocity + dv_half);
		else
			velocity = vi_max(-TRAM_SPEED_MAX, velocity - dv_half);
	}
	else if (distance == 0.0f) // stopped
	{
		if (state == State::Arriving)
		{
			if (is_front)
				TramNet::send(Tram::by_track(track), TramNet::Message::DoorsOpen);
			state = State::Idle;
		}

		get<RigidBody>()->btBody->setActivationState(ISLAND_SLEEPING);
	}
	else // decelerating
	{
		if (velocity > 0.0f)
		{
			velocity = vi_max(0.01f, velocity - dv_half);
			set(vi_min(target_offset, offset + velocity * u.time.delta));
		}
		else
		{
			velocity = vi_min(-0.01f, velocity + dv_half);
			set(vi_max(target_offset, offset + velocity * u.time.delta));
		}
		if (velocity > 0.0f)
			velocity = vi_max(0.01f, velocity - dv_half);
		else
			velocity = vi_min(-0.01f, velocity + dv_half);
	}
}

void TramRunner::update_client(const Update& u)
{
	if (get<RigidBody>()->btBody->isActive() && mersenne::randf_co() < u.time.delta / 3.0f)
	{
		b8 left = mersenne::randf_co() < 0.5f;
		Vec3 pos = get<Transform>()->to_world(Vec3(left ? -0.35f : 0.35f, 0.45f, 0));

		Quat rot = get<Transform>()->absolute_rot() * Quat::euler(0, (left ? PI * -0.5f : PI * 0.5f), 0);
		for (s32 i = 0; i < 15; i++)
		{
			Particles::sparks.add
			(
				pos,
				rot * Vec3(mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo()) * 10.0f,
				Vec4(1, 1, 1, 1)
			);
		}

		Shockwave::add(pos + Vec3(0, -0.2f, 0), 3.0f, 0.25f, Shockwave::Type::Light, get<Transform>());
	}
}

TramEntity::TramEntity(TramRunner* runner_a, TramRunner* runner_b)
{
	const r32 width = TRAM_LENGTH * 0.5f;
	const r32 height = 2.54f;
	const r32 rope_length = 4.0f;

	Transform* transform = create<Transform>();

	{
		Vec3 pos_a = runner_a->get<Transform>()->pos;
		Vec3 pos_b = runner_b->get<Transform>()->pos;
		transform->pos = (pos_a + pos_b) * 0.5f;
		transform->pos.y += (fabsf(pos_b.y - pos_a.y) * 0.5f) - height - rope_length;
		transform->rot = Quat::look(Vec3::normalize(pos_b - pos_a));
	}

	const Mesh* mesh = Loader::mesh(Asset::Mesh::tram_mesh);
	RigidBody* body = create<RigidBody>(RigidBody::Type::Box, (mesh->bounds_max - mesh->bounds_min) * 0.5f, 5.0f, CollisionAwkIgnore, ~CollisionWalker & ~CollisionInaccessible & ~CollisionParkour & ~CollisionStatic & ~CollisionElectric);
	body->set_restitution(0.75f);
	body->set_damping(0.5f, 0.5f);
	body->rebuild();

	create<PlayerTrigger>()->radius = 1.3f; // trigger for entering

	Tram* tram = create<Tram>();
	tram->runner_a = runner_a->get<TramRunner>();
	tram->runner_b = runner_b->get<TramRunner>();
	if (tram->runner_b.ref()->state == TramRunner::State::Arriving)
		body->btBody->setLinearVelocity(transform->rot * Vec3(0, 0, -TRAM_SPEED_MAX));

	View* view = create<View>();
	view->mesh = Asset::Mesh::tram_mesh;
	view->shader = Asset::Shader::standard;
	view->color.w = MATERIAL_INACCESSIBLE;

	{
		Entity* child = World::alloc<StaticGeom>(Asset::Mesh::tram_collision, Vec3::zero, Quat::identity, CollisionInaccessible, ~CollisionAwkIgnore & ~CollisionInaccessible & ~CollisionParkour & ~CollisionElectric);
		child->get<Transform>()->parent = transform;
		child->get<View>()->mesh = Asset::Mesh::tram_mesh_1;
		child->get<View>()->shader = Asset::Shader::flat;
		child->get<View>()->alpha();
		World::awake(child);
		Net::finalize(child);
	}

	{
		Entity* doors = World::alloc<Empty>();
		doors->get<Transform>()->parent = transform;
		doors->get<Transform>()->rot = Quat::identity;

		SkinnedModel* model = doors->create<SkinnedModel>();
		model->mesh = Asset::Mesh::tram_doors;
		model->shader = Asset::Shader::armature;
		model->color = Vec4(1, 1, 1, MATERIAL_INACCESSIBLE);

		Animator* anim = doors->create<Animator>();
		anim->armature = Asset::Armature::tram_doors;
		anim->layers[0].behavior = Animator::Behavior::Loop;
		anim->layers[0].blend_time = 0.0f;
		anim->layers[1].blend_time = 0.0f;

		RigidBody* body = doors->create<RigidBody>(RigidBody::Type::Mesh, Vec3::zero, 0.0f, CollisionStatic | CollisionInaccessible, ~CollisionStatic & ~CollisionAwkIgnore & ~CollisionInaccessible & ~CollisionParkour & ~CollisionElectric, Asset::Mesh::tram_collision_door);
		body->set_restitution(0.75f);
		
		doors->create<PlayerTrigger>()->radius = 8.0f; // trigger for exiting

		World::awake(doors);
		Net::finalize(doors);

		tram->doors = doors;
	}

	{
		runner_a->get<RigidBody>()->rebuild();
		Quat rot_a = runner_a->get<Transform>()->rot;
		Rope* rope1 = Rope::start(runner_a->get<RigidBody>(), runner_a->get<Transform>()->pos + rot_a * Vec3(0, -0.37f, 0), rot_a * Vec3(0, -1, 0), Quat::look(rot_a * Vec3(0, -1, 0)));
		if (rope1)
			rope1->end(transform->to_world(Vec3(0, height, -width)), transform->rot * (Quat::euler(0, 0, PI * (30.0f / 180.0f)) * Vec3(0, 1, 0)), body, 0.0f, true);
	}
	{
		runner_b->get<RigidBody>()->rebuild();
		Quat rot_b = runner_b->get<Transform>()->rot;
		Rope* rope2 = Rope::start(runner_b->get<RigidBody>(), runner_b->get<Transform>()->pos + rot_b * Vec3(0, -0.37f, 0), rot_b * Vec3(0, -1, 0), Quat::look(rot_b * Vec3(0, -1, 0)));
		if (rope2)
			rope2->end(transform->to_world(Vec3(0, height, width)), rot_b * (Quat::euler(0, 0, PI * (-30.0f / 180.0f)) * Vec3(0, 1, 0)), body, 0.0f, true);
	}
}

void Tram::awake()
{
	link_arg<Entity*, &Tram::player_entered>(get<PlayerTrigger>()->entered);
	link_arg<Entity*, &Tram::player_exited>(doors.ref()->get<PlayerTrigger>()->exited);
}

namespace TramNet
{
	b8 send(Tram* t, Message m)
	{
		using Stream = Net::StreamWrite;
		Stream* p = Net::msg_new(Net::MessageType::Tram);

		{
			Ref<Tram> ref = t;
			serialize_ref(p, ref);
		}

		serialize_enum(p, Message, m);

		Net::msg_finalize(p);

		return true;
	}
}

b8 Tram::net_msg(Net::StreamRead* p, Net::MessageSource)
{
	using Stream = Net::StreamRead;

	Ref<Tram> ref;
	serialize_ref(p, ref);

	TramNet::Message m;
	serialize_enum(p, TramNet::Message, m);

	if (Game::level.mode == Game::Mode::Parkour && ref.ref())
	{
		switch (m)
		{
			case TramNet::Message::Entered:
			{
				if (ref.ref()->departing
					&& ref.ref()->doors_open())
				{
					ref.ref()->doors_open(false);
					if (Game::level.local)
						TramRunner::go(ref.ref()->track(), 1.0f, TramRunner::State::Departing);
				}
				break;
			}
			case TramNet::Message::Exited:
			{
				if (ref.ref()->doors_open())
				{
					ref.ref()->doors_open(false);
					ref.ref()->departing = false;
				}
				break;
			}
			case TramNet::Message::DoorsOpen:
			{
				ref.ref()->doors_open(true);
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

void Tram::player_entered(Entity* e)
{
	if (departing
		&& doors_open()
		&& e->has<Parkour>()
		&& e->get<PlayerControlHuman>()->local())
	{
		if (Overworld::zone_under_attack() == Game::level.tram_tracks[track()].level) // can't go there if it's under attack
			e->get<PlayerControlHuman>()->player.ref()->msg(_(strings::error_zone_under_attack), false);
		else
			TramNet::send(this, TramNet::Message::Entered);
	}
}

void Tram::player_exited(Entity* e)
{
	if (!departing
		&& doors_open()
		&& e->has<Parkour>()
		&& e->get<PlayerControlHuman>()->local())
	{
		TramNet::send(this, TramNet::Message::Exited);
	}
}

Tram* Tram::by_track(s8 track)
{
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->runner_b.ref()->track == track)
			return i.item();
	}
	return nullptr;
}

b8 Tram::player_inside(Entity* player)
{
	Ref<RigidBody> support = player->get<Walker>()->support;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (support.equals(i.item()->get<RigidBody>()))
			return true;
	}
	return false;
}

s8 Tram::track() const
{
	return runner_b.ref()->track;
}

b8 Tram::doors_open() const
{
	return doors.ref()->get<RigidBody>()->collision_filter == 0;
}

void Tram::doors_open(b8 open)
{
	Animator* anim = doors.ref()->get<Animator>();
	RigidBody* body = doors.ref()->get<RigidBody>();
	if (open)
	{
		body->set_collision_masks(CollisionStatic | CollisionInaccessible, 0); // disable collision
		anim->layers[0].play(Asset::Animation::tram_doors_opened);
		anim->layers[1].play(Asset::Animation::tram_doors_open);
	}
	else
	{
		body->set_collision_masks(CollisionStatic | CollisionInaccessible, ~CollisionStatic & ~CollisionAwkIgnore & ~CollisionParkour & ~CollisionInaccessible & ~CollisionElectric); // enable collision
		anim->layers[0].animation = AssetNull;
		anim->layers[1].play(Asset::Animation::tram_doors_close);
	}
}

void TramInteractableEntity::interacted(Interactable* i)
{
	s8 track = s8(i->user_data);
	Tram* tram = Tram::by_track(track);
	if (tram->doors_open())
	{
		tram->departing = false;
		tram->doors_open(false);
	}
	else
	{
		AssetID target_level = Game::level.tram_tracks[track].level;
		if (Game::save.zones[Game::level.id] != ZoneState::Locked
			|| Game::save.zones[target_level] != ZoneState::Locked)
		{
			if (Game::level.local && Game::save.zones[target_level] != ZoneState::Friendly)
				Overworld::resource_change(Resource::HackKits, -1);
			tram->departing = true;
			tram->doors_open(true);
		}
		else
		{
			for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
				i.item()->msg(_(strings::error_locked_zone), false);
		}
	}
}

TramInteractableEntity::TramInteractableEntity(const Vec3& absolute_pos, const Quat& absolute_rot, s8 track)
{
	create<Transform>();

	SkinnedModel* model = create<SkinnedModel>();
	model->mesh = Asset::Mesh::interactable;
	model->shader = Asset::Shader::armature;
	model->hollow();

	Animator* anim = create<Animator>();
	anim->armature = Asset::Armature::interactable;
	anim->layers[0].behavior = Animator::Behavior::Loop;
	anim->layers[0].animation = Asset::Animation::interactable_enabled;
	anim->layers[0].blend_time = 0.0f;
	anim->layers[1].blend_time = 0.0f;

	Interactable* i = create<Interactable>(Interactable::Type::Tram);
	i->user_data = track;

	{
		Entity* collision = World::create<StaticGeom>(Asset::Mesh::interactable_collision, absolute_pos, absolute_rot, CollisionInaccessible, ~CollisionParkour & ~CollisionInaccessible & ~CollisionElectric);
		collision->get<View>()->color.w = MATERIAL_INACCESSIBLE;
		Net::finalize(collision);
	}
}

Array<Ascensions::Entry> Ascensions::entries;
r32 Ascensions::timer;
r32 Ascensions::particle_accumulator;

void Ascensions::update(const Update& u)
{
	const r32 total_time = 20.0f; // total duration of an individual ascension animation
	if (Game::level.mode != Game::Mode::Special)
	{
		timer -= Game::real_time.delta;
		if (timer < 0.0f)
		{
			timer = 40.0f + mersenne::randf_co() * 200.0f;

			Entry* e = entries.add();
			e->timer = total_time;
			e->username = Usernames::all[mersenne::rand_u32() % Usernames::count];
		}
	}

	for (s32 i = 0; i < entries.length; i++)
	{
		Entry* e = &entries[i];
		r32 old_timer = e->timer;
		e->timer -= u.time.delta;
		if (e->timer < 0.0f)
		{
			entries.remove(i);
			i--;
		}
		else
		{
			// only show notifications in parkour mode
			if (Game::level.mode == Game::Mode::Parkour
				&& old_timer >= total_time * 0.85f && e->timer < total_time * 0.85f)
			{
				char msg[512];
				sprintf(msg, _(strings::player_ascended), e->username);
				PlayerHuman::log_add(msg, AI::TeamNone);
			}
		}
	}

	// particles
	const r32 interval = 0.5f;
	particle_accumulator += u.time.delta;
	while (particle_accumulator > interval)
	{
		particle_accumulator -= interval;
		for (s32 i = 0; i < entries.length; i++)
		{
			const Entry& e = entries[i];
			r32 blend = 1.0f - (e.timer / total_time);
			Particles::tracers_skybox.add
			(
				Quat::euler(Ease::circ_out<r32>(blend) * PI * 0.45f, Game::level.rotation, 0) * Vec3(Game::level.skybox.far_plane * 0.9f, 0, 0), // position
				(Game::level.skybox.far_plane / 100.0f) * LMath::lerpf(blend, 1.0f, 0.5f) // size scale
			);
		}
	}
}

void Ascensions::clear()
{
	timer = 40.0f + mersenne::randf_co() * 200.0f;
	entries.length = 0;
}

}
