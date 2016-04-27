#include "entities.h"
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

namespace VI
{


AwkEntity::AwkEntity(AI::Team team)
{
	create<Audio>();
	Transform* transform = create<Transform>();
	create<Awk>();
	create<AIAgent>()->team = team;

	Health* health = create<Health>(1, AWK_HEALTH);

	SkinnedModel* model = create<SkinnedModel>();
	model->mesh = Asset::Mesh::awk;
	model->shader = Asset::Shader::armature;
	model->team = (u8)team;

	Animator* anim = create<Animator>();
	anim->armature = Asset::Armature::awk;

	create<Target>();

	Vec3 abs_pos;
	Quat abs_quat;
	get<Transform>()->absolute(&abs_pos, &abs_quat);
	create<RigidBody>(RigidBody::Type::Sphere, Vec3(AWK_RADIUS), 0.0f, CollisionAwk | CollisionTarget, CollisionDefault & ~CollisionTarget & ~CollisionAwkIgnore);
}

Health::Health(u16 start_value, u16 hp_max)
	: hp(start_value), hp_max(hp_max), added(), damaged(), killed()
{
}

void Health::damage(Entity* e, u16 damage)
{
	if (hp > 0 && damage > 0)
	{
		if (damage > hp)
			hp = 0;
		else
			hp -= damage;
		damaged.fire({ e, damage });
		if (hp == 0)
			killed.fire(e);
	}
}

void Health::add(u16 amount)
{
	u16 old_hp = hp;
	hp = vi_min((u16)(hp + amount), hp_max);
	if (hp > old_hp)
		added.fire();
}

b8 Health::is_full() const
{
	return hp >= hp_max;
}

HealthPickupEntity::HealthPickupEntity()
{
	create<Transform>();
	View* model = create<View>();
	model->color = Vec4(0.6f, 0.6f, 0.6f, MATERIAL_NO_OVERRIDE);
	model->mesh = Asset::Mesh::target;
	model->shader = Asset::Shader::standard;

	PointLight* light = create<PointLight>();
	light->radius = 8.0f;

	Target* target = create<Target>();

	HealthPickup* pickup = create<HealthPickup>();

	model->offset.scale(Vec3(HEALTH_PICKUP_RADIUS - 0.2f));

	RigidBody* body = create<RigidBody>(RigidBody::Type::Sphere, Vec3(HEALTH_PICKUP_RADIUS), 0.1f, CollisionAwkIgnore | CollisionTarget, ~CollisionAwk & ~CollisionShield);
	body->set_damping(0.5f, 0.5f);
}

void HealthPickup::awake()
{
	link_arg<const TargetEvent&, &HealthPickup::hit>(get<Target>()->target_hit);
	reset();
}

void HealthPickup::reset()
{
	owner = nullptr;
	get<View>()->team = (u8)AI::Team::None;
	get<PointLight>()->team = (u8)AI::Team::None;
}

void HealthPickup::hit(const TargetEvent& e)
{
	if (e.hit_by->has<AIAgent>()
		&& e.hit_by->has<Health>()
		&& (!owner.ref() || e.hit_by != owner.ref()->entity()))
	{
		Health* health = e.hit_by->get<Health>();

		if (health->hp < health->hp_max)
		{
			if (owner.ref())
				owner.ref()->get<Health>()->damage(e.hit_by, 1);

			health->add(1);
			owner = health;

			AI::Team team = e.hit_by->get<AIAgent>()->team;
			get<PointLight>()->team = (u8)team;
			get<View>()->team = (u8)team;
		}
	}
}

SensorEntity::SensorEntity(PlayerManager* owner, const Vec3& abs_pos, const Quat& abs_rot)
{
	Transform* transform = create<Transform>();
	transform->pos = abs_pos;
	transform->rot = abs_rot;

	AI::Team team = owner->team.ref()->team();

	View* model = create<View>();
	model->mesh = Asset::Mesh::sphere;
	model->team = (u8)team;
	model->shader = Asset::Shader::standard;
	model->offset.scale(Vec3(SENSOR_RADIUS));

	create<Health>(5, 5);

	PointLight* light = create<PointLight>();
	light->type = PointLight::Type::Override;
	light->team = (u8)team;
	light->radius = SENSOR_RANGE;

	create<Sensor>(team, owner);

	create<Target>();

	RigidBody* body = create<RigidBody>(RigidBody::Type::Sphere, Vec3(SENSOR_RADIUS), 1.0f, CollisionAwkIgnore | CollisionTarget, btBroadphaseProxy::AllFilter);
	body->set_damping(0.5f, 0.5f);

	create<PlayerTrigger>()->radius = SENSOR_RANGE;
}

Sensor::Sensor(AI::Team t, PlayerManager* m)
	: team(t),
	player_manager(m)
{
}

void Sensor::awake()
{
	link_arg<Entity*, &Sensor::killed_by>(get<Health>()->killed);
	if (has<Target>())
		link_arg<const TargetEvent&, &Sensor::hit_by>(get<Target>()->target_hit);
}

void Sensor::hit_by(const TargetEvent& e)
{
	get<Health>()->damage(e.hit_by, get<Health>()->hp_max);
}

void Sensor::killed_by(Entity* e)
{
	if (e)
	{
		AI::Team enemy_team = e->get<AIAgent>()->team;
		if (enemy_team != team)
		{
			for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
			{
				if (i.item()->team.ref()->team() == enemy_team)
					i.item()->add_credits(CREDITS_SENSOR_DESTROY);
			}
		}
	}
	World::remove_deferred(entity());
}

#define sensor_shockwave_interval 3.0f
void Sensor::update_all(const Update& u)
{
	r32 time = u.time.total;
	r32 last_time = time - u.time.delta;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		r32 offset = i.index * sensor_shockwave_interval * 0.3f;
		if ((s32)((time + offset) / sensor_shockwave_interval) != (s32)((last_time + offset) / sensor_shockwave_interval))
			World::create<ShockwaveEntity>(10.0f)->get<Transform>()->absolute_pos(i.item()->get<Transform>()->absolute_pos());
	}
}

ControlPointEntity::ControlPointEntity()
{
	create<Transform>();
	create<ControlPoint>();

	View* model = create<View>();
	model->mesh = Asset::Mesh::control_point;
	model->color = Vec4(1.0f, 1.0f, 1.0f, MATERIAL_NO_OVERRIDE);
	model->shader = Asset::Shader::standard;
}

ControlPoint::ControlPoint()
	: team(AI::Team::None)
{
}

#define CONTROL_POINT_INTERVAL 15.0f
r32 ControlPoint::timer = CONTROL_POINT_INTERVAL;
void ControlPoint::update_all(const Update& u)
{
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		Vec3 control_point_pos;
		Quat control_point_rot;
		i.item()->get<Transform>()->absolute(&control_point_pos, &control_point_rot);

		AI::Team control_point_team = AI::Team::None;
		for (auto sensor = Sensor::list.iterator(); !sensor.is_last(); sensor.next())
		{
			if (sensor.item()->get<PlayerTrigger>()->contains(control_point_pos))
			{
				Vec3 to_sensor = sensor.item()->get<Transform>()->absolute_pos() - control_point_pos;
				if (to_sensor.dot(control_point_rot * Vec3(0, 0, 1)) > 0.0f) // make sure the control point is facing the sensor
				{
					AI::Team sensor_team = sensor.item()->team;
					if (control_point_team == AI::Team::None)
						control_point_team = sensor_team;
					else if (control_point_team != sensor_team)
					{
						control_point_team = AI::Team::None; // control point is contested
						break;
					}
				}
			}
		}

		i.item()->team = control_point_team;
		i.item()->get<View>()->team = (u8)control_point_team;
	}

	timer -= u.time.delta;
	if (timer < 0.0f)
	{
		// give points to teams based on how many control points they own
		s32 reward_buffer[(s32)AI::Team::count] = {};

		for (auto i = list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->team != AI::Team::None)
				reward_buffer[(s32)i.item()->team] += CREDITS_CONTROL_POINT;
		}

		// add credits to players
		for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
		{
			s32 reward = reward_buffer[(s32)i.item()->team.ref()->team()];
			i.item()->add_credits(reward);
		}

		timer = CONTROL_POINT_INTERVAL;
	}

}
#define TELEPORTER_RADIUS 0.5f
TeleporterEntity::TeleporterEntity(const Vec3& pos, const Quat& rot, AI::Team team)
{
	create<Transform>()->absolute(pos, rot);
	create<Teleporter>()->team = team;

	create<Health>(5, 5);

	View* model = create<View>();
	model->mesh = Asset::Mesh::teleporter;
	model->team = (u8)team;
	model->shader = Asset::Shader::standard;

	create<RigidBody>(RigidBody::Type::Sphere, Vec3(TELEPORTER_RADIUS), 0.0f, CollisionAwkIgnore, btBroadphaseProxy::AllFilter);
}

Teleporter::Teleporter()
{
	obstacle_id = AI::obstacle_add(get<Transform>()->absolute_pos(), TELEPORTER_RADIUS, TELEPORTER_RADIUS);
}

Teleporter::~Teleporter()
{
	AI::obstacle_remove(obstacle_id);
}

PlayerSpawn::PlayerSpawn(AI::Team team)
{
	create<Transform>();

	View* view = create<View>();
	view->mesh = Asset::Mesh::spawn;
	view->shader = Asset::Shader::standard;
	view->team = (u8)team;

	create<PlayerTrigger>()->radius = PLAYER_SPAWN_RADIUS;

	PointLight* light = create<PointLight>();
	light->team = (u8)team;
	light->offset.y = 2.0f;
	light->radius = 12.0f;
}

#define PROJECTILE_SPEED 100.0f
#define PROJECTILE_LENGTH 2.0f
#define PROJECTILE_THICKNESS 0.04f
#define PROJECTILE_MAX_LIFETIME 3.0f
ProjectileEntity::ProjectileEntity(Entity* owner, const Vec3& pos, u16 damage, const Vec3& velocity)
{
	Vec3 dir = Vec3::normalize(velocity);
	Transform* transform = create<Transform>();
	transform->absolute_pos(pos);
	transform->absolute_rot(Quat::look(dir));

	PointLight* light = create<PointLight>();
	light->radius = 10.0f;
	light->color = Vec3(1, 1, 1);

	create<Audio>();

	create<Projectile>(owner, damage, dir * PROJECTILE_SPEED);
}

Projectile::Projectile(Entity* entity, u16 damage, const Vec3& velocity)
	: owner(entity), damage(damage), velocity(velocity), lifetime()
{
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
	Physics::raycast(&ray_callback, -1);
	if (ray_callback.hasHit())
	{
		Entity* hit_object = &Entity::list[ray_callback.m_collisionObject->getUserIndex()];
		if (hit_object != owner.ref())
		{
			Vec3 basis;
			if (hit_object->has<Health>())
			{
				hit_object->get<Health>()->damage(owner.ref(), damage);
				basis = Vec3::normalize(velocity);
			}
			else
				basis = ray_callback.m_hitNormalWorld;

			for (s32 i = 0; i < 50; i++)
			{
				Particles::sparks.add
				(
					ray_callback.m_hitPointWorld,
					Quat::look(basis) * Vec3(mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo()) * 10.0f,
					Vec4(1, 1, 1, 1)
				);
			}
			World::remove(entity());
			return;
		}
	}
	else
		get<Transform>()->absolute_pos(next_pos);
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

b8 PlayerTrigger::contains(const Vec3& p) const
{
	return (p - get<Transform>()->absolute_pos()).length_squared() < radius * radius;
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

	Mesh* mesh_data = Loader::mesh_instanced(Asset::Mesh::tri_tube);
	Vec3 radius = (Vec4(mesh_data->bounds_radius, mesh_data->bounds_radius, mesh_data->bounds_radius, 0)).xyz();
	r32 f_radius = vi_max(radius.x, vi_max(radius.y, radius.z));

	// ropes
	{
		static Mat4 scale = Mat4::make_scale(Vec3(rope_radius, rope_radius, rope_segment_length * 0.5f));

		for (auto i = Rope::list.iterator(); !i.is_last(); i.next())
		{
			Mat4 m;
			i.item()->get<Transform>()->mat(&m);

			if (params.camera->visible_sphere(m.translation(), rope_segment_length * f_radius))
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

	Loader::shader(Asset::Shader::standard_instanced);

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
	sync->write<Vec4>(Vec4(1, 1, 1, MATERIAL_UNLIT));

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
			r32 rope_interval = rope_segment_length / (1.0f + slack);
			Vec3 scale = Vec3(rope_radius, rope_radius, rope_segment_length * 0.5f);

			if (length > rope_interval * 0.5f)
			{
				Vec3 spawn_pos = last_segment_pos + (diff / length) * rope_interval * 0.5f;
				Entity* box = World::create<PhysicsEntity>(AssetNull, spawn_pos, rot, RigidBody::Type::CapsuleZ, Vec3(rope_radius, rope_segment_length - rope_radius * 2.0f, 0.0f), 0.05f, CollisionAwkIgnore, CollisionInaccessibleMask);
				Rope* r = box->add<Rope>();
				if (last_segment->has<Rope>())
					last_segment->get<Rope>()->next = box->get<RigidBody>();
				r->prev = last_segment;

				static Quat rotation_a = Quat::look(Vec3(0, 0, 1)) * Quat::euler(0, PI * -0.5f, 0);
				static Quat rotation_b = Quat::look(Vec3(0, 0, -1)) * Quat::euler(PI, PI * -0.5f, 0);

				RigidBody::Constraint constraint;
				constraint.type = constraint_type;
				constraint.frame_a = btTransform(rotation_a, last_segment_relative_pos),
				constraint.frame_b = btTransform(rotation_b, Vec3(0, 0, rope_segment_length * -0.5f));
				constraint.limits = Vec3(PI, PI, 0);
				constraint.a = last_segment;
				constraint.b = box->get<RigidBody>();
				RigidBody::add_constraint(constraint);

				box->get<RigidBody>()->set_damping(0.5f, 0.5f);
				last_segment = box->get<RigidBody>();
				last_segment_relative_pos = Vec3(0, 0, rope_segment_length * 0.5f);
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
	// add the first rope segment
	Vec3 p = abs_pos + abs_normal * rope_radius;
	Transform* start_trans = start->get<Transform>();
	RigidBody* rope = rope_add(start, start_trans->to_local(p), p + abs_rot * Vec3(0, 0, rope_segment_length), abs_rot, slack, RigidBody::Constraint::Type::PointToPoint);
	vi_assert(rope); // should never happen
	return rope->get<Rope>();
}

void Rope::end(const Vec3& pos, const Vec3& normal, RigidBody* end, r32 slack)
{
	Vec3 abs_pos = pos + normal * rope_radius;
	RigidBody* start = get<RigidBody>();
	Vec3 start_relative_pos = Vec3(0, 0, rope_segment_length * 0.5f);
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
	ray_callback.m_flags = btTriangleRaycastCallback::EFlags::kF_FilterBackfaces | btTriangleRaycastCallback::EFlags::kF_KeepUnflippedNormal;
	ray_callback.m_collisionFilterGroup = ray_callback.m_collisionFilterMask = btBroadphaseProxy::AllFilter;

	Physics::btWorld->rayTest(start_pos, end, ray_callback);
	if (ray_callback.hasHit())
	{
		Vec3 end2 = start_pos + dir_normalized * -max_distance;

		btCollisionWorld::ClosestRayResultCallback ray_callback2(start_pos, end2);
		ray_callback2.m_flags = btTriangleRaycastCallback::EFlags::kF_FilterBackfaces | btTriangleRaycastCallback::EFlags::kF_KeepUnflippedNormal;
		ray_callback2.m_collisionFilterGroup = ray_callback2.m_collisionFilterMask = btBroadphaseProxy::AllFilter;
		Physics::btWorld->rayTest(start_pos, end2, ray_callback2);

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

TileEntity::TileEntity(const Vec3& pos, const Quat& rot, Transform* parent, const Vec3& offset, r32 anim_time)
{
	Transform* transform = create<Transform>();

	transform->absolute(pos + offset, rot * Quat::euler(PI * 0.5f, PI * 0.5f, fmod((Game::time.total + (anim_time * 2.0f)) * 5.0f, PI * 2.0f)));

	transform->reparent(parent);

	Vec3 relative_target_pos = pos;
	Quat relative_target_rot = rot;
	if (parent)
		parent->to_local(&relative_target_pos, &relative_target_rot);

	create<Tile>(relative_target_pos, relative_target_rot, anim_time);
}

Array<Mat4> Tile::instances;

Tile::Tile(const Vec3& pos, const Quat& rot, r32 anim_time)
	: relative_target_pos(pos),
	relative_target_rot(rot),
	timer(),
	anim_time(anim_time)
{
}

void Tile::awake()
{
	relative_start_pos = get<Transform>()->pos;
	relative_start_rot = get<Transform>()->rot;
}

#define TILE_LIFE_TIME 6.0f
#define TILE_ANIM_OUT_TIME 0.3f
void Tile::update(const Update& u)
{
	timer += u.time.delta;
	if (timer > TILE_LIFE_TIME)
		World::remove(entity());
	else
	{
		r32 blend = vi_min(timer / anim_time, 1.0f);

		Vec3 blend_pos = Vec3::lerp(blend, relative_start_pos, relative_target_pos) + Vec3(sinf(blend * PI) * 0.25f);
		Quat blend_rot = Quat::slerp(blend, relative_start_rot, relative_target_rot);

		get<Transform>()->pos = blend_pos;
		get<Transform>()->rot = blend_rot;
	}
}

r32 Tile::scale() const
{
	r32 blend;
	if (timer < TILE_LIFE_TIME - TILE_ANIM_OUT_TIME)
		blend = vi_min(timer / anim_time, 1.0f);
	else
		blend = Ease::quad_in(((timer - (TILE_LIFE_TIME - TILE_ANIM_OUT_TIME)) / TILE_ANIM_OUT_TIME), 1.0f, 0.0f);
	return blend * TILE_SIZE;
}

void Tile::draw_alpha(const RenderParams& params)
{
	instances.length = 0;

	Mesh* mesh_data = Loader::mesh_instanced(Asset::Mesh::plane);
	Vec3 radius = (Vec4(mesh_data->bounds_radius, mesh_data->bounds_radius, mesh_data->bounds_radius, 0)).xyz();
	r32 f_radius = vi_max(radius.x, vi_max(radius.y, radius.z));

	{
		for (auto i = Tile::list.iterator(); !i.is_last(); i.next())
		{
			Tile* tile = i.item();
			const r32 size = tile->scale();
			if (params.camera->visible_sphere(tile->get<Transform>()->absolute_pos(), size * f_radius))
			{
				Mat4* m = instances.add();
				tile->get<Transform>()->mat(m);
				m->scale(Vec3(size));
			}
		}
	}

	if (instances.length == 0)
		return;

	Loader::shader(Asset::Shader::standard_instanced);

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
	sync->write<Vec4>(Vec4(1, 1, 0.25f, 0.5f));

	sync->write(RenderOp::Instances);
	sync->write(Asset::Mesh::plane);
	sync->write(instances.length);
	sync->write<Mat4>(instances.data, instances.length);
}

MoverEntity::MoverEntity(const b8 reversed, const b8 trans, const b8 rot)
{
	create<Mover>(reversed, trans, rot);
}

Mover::Mover(const b8 reversed, const b8 trans, const b8 rot)
	: reversed(reversed), x(), translation(trans), rotation(rot), target(reversed ? 1.0f : 0.0f), speed(), object(), start_pos(), start_rot(), end_pos(), end_rot(), last_moving(), ease()
{
}

void Mover::update(const Update& u)
{
	if (object.ref())
	{
		r32 actual_target = reversed ? 1.0f - target : target;
		b8 moving;
		if (x == actual_target)
		{
			RigidBody* body = object.ref()->get<RigidBody>();
			if (body)
				body->btBody->setActivationState(ISLAND_SLEEPING);

			moving = false;
		}
		else
		{
			if (x < actual_target)
				x = vi_min(actual_target, x + speed * u.time.delta);
			else
				x = vi_max(actual_target, x - speed * u.time.delta);
			refresh();

			moving = true;
		}

		if (!object.ref()->has<Audio>())
			object.ref()->entity()->add<Audio>();
		if (moving && !last_moving)
			object.ref()->get<Audio>()->post_event(AK::EVENTS::MOVER_LOOP);
		else if (!moving && last_moving)
			object.ref()->get<Audio>()->post_event(AK::EVENTS::MOVER_STOP);
		last_moving = moving;
	}
	else
		World::remove(entity());
}

void Mover::setup(Transform* obj, Transform* end, const r32 _speed)
{
	object = obj;
	obj->absolute(&start_pos, &start_rot);
	end->absolute(&end_pos, &end_rot);
	if (translation)
		speed = _speed / (end_pos - start_pos).length();
	else
		speed = _speed / Quat::angle(start_rot, end_rot);
	refresh();
}

void Mover::refresh()
{
	if (object.ref())
	{
		r32 eased = Ease::ease(ease, x, 0.0f, 1.0f);
		if (translation && rotation)
			object.ref()->absolute(Vec3::lerp(eased, start_pos, end_pos), Quat::slerp(eased, start_rot, end_rot));
		else
		{
			if (rotation)
				object.ref()->absolute_rot(Quat::slerp(eased, start_rot, end_rot));
			if (translation)
				object.ref()->absolute_pos(Vec3::lerp(eased, start_pos, end_pos));
		}
		RigidBody* body = object.ref()->get<RigidBody>();
		if (body)
			body->btBody->activate(true);
	}
	else
		World::remove(entity());
}

ShockwaveEntity::ShockwaveEntity(r32 max_radius)
{
	create<Transform>();

	PointLight* light = create<PointLight>();
	light->radius = 0.0f;
	light->type = PointLight::Type::Shockwave;

	create<Shockwave>(max_radius);
}

Shockwave::Shockwave(r32 max_radius)
	: timer(), max_radius(max_radius)
{
}

#define shockwave_duration 2.0f
#define shockwave_duration_multiplier (1.0f / shockwave_duration)

r32 Shockwave::radius() const
{
	return get<PointLight>()->radius;
}

r32 Shockwave::duration() const
{
	return max_radius * (2.0f / 15.0f);
}

void Shockwave::update(const Update& u)
{
	PointLight* light = get<PointLight>();
	timer += u.time.delta;
	r32 d = duration();
	if (timer > d)
		World::remove(entity());
	else
	{
		r32 fade_radius = max_radius * (2.0f / 15.0f);
		light->radius = Ease::cubic_out(timer * (1.0f / d), 0.0f, max_radius);
		r32 fade = 1.0f - vi_max(0.0f, ((light->radius - (max_radius - fade_radius)) / fade_radius));
		light->color = Vec3(fade);
	}
}


}
