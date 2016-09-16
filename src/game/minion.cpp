#include "minion.h"
#include "data/animator.h"
#include "render/skinned_model.h"
#include "walker.h"
#include "asset/armature.h"
#include "asset/animation.h"
#include "asset/shader.h"
#include "asset/mesh.h"
#include "asset/Wwise_IDs.h"
#include "audio.h"
#include "player.h"
#include "mersenne/mersenne-twister.h"
#include "game.h"
#include "render/views.h"
#include "awk.h"
#include "BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "data/ragdoll.h"
#include "entities.h"
#include "render/particles.h"

#define WALK_SPEED 2.0f
#define ROTATION_SPEED 4.0f
#define MINION_HEARING_RANGE 7.0f
#define HEALTH 3

namespace VI
{

Minion::Minion(const Vec3& pos, const Quat& quat, AI::Team team, PlayerManager* manager)
{
	Transform* transform = create<Transform>();
	transform->pos = pos;

	Animator* animator = create<Animator>();
	SkinnedModel* model = create<SkinnedModel>();

	animator->armature = Asset::Armature::character;
	animator->layers[0].play(Asset::Animation::character_idle);

	model->shader = Asset::Shader::armature;
	model->mesh = Asset::Mesh::character;
	model->team = (u8)team;
	model->color.w = MATERIAL_NO_OVERRIDE;

	create<Audio>();

	Health* health = create<Health>(HEALTH, HEALTH);
	
	Vec3 forward = quat * Vec3(0, 0, 1);

	Walker* walker = create<Walker>(atan2f(forward.x, forward.z));
	walker->max_speed = WALK_SPEED;
	walker->rotation_speed = ROTATION_SPEED;

	create<MinionCommon>()->owner = manager;

	create<AIAgent>()->team = team;

	create<Target>();

	create<MinionAI>();
}

void MinionCommon::awake()
{
	link_arg<const TargetEvent&, &MinionCommon::hit_by>(get<Target>()->target_hit);
	link_arg<Entity*, &MinionCommon::killed>(get<Health>()->killed);

	Animator* animator = get<Animator>();
	animator->layers[1].loop = false;
	link<&MinionCommon::footstep>(animator->trigger(Asset::Animation::character_walk, 0.3375f));
	link<&MinionCommon::footstep>(animator->trigger(Asset::Animation::character_walk, 0.75f));
}

MinionCommon* MinionCommon::closest(AI::TeamMask mask, const Vec3& pos, r32* distance)
{
	MinionCommon* closest = nullptr;
	r32 closest_distance = FLT_MAX;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->get<AIAgent>()->team, mask))
		{
			r32 d = (pos - i.item()->get<Transform>()->absolute_pos()).length_squared();
			if (d < closest_distance)
			{
				closest = i.item();
				closest_distance = d;
			}
		}
	}
	if (distance)
		*distance = sqrtf(closest_distance);
	return closest;
}

s32 MinionCommon::count(AI::TeamMask mask)
{
	s32 result = 0;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->get<AIAgent>()->team, mask))
			result++;
	}
	return result;
}

void MinionCommon::footstep()
{
	Audio::post_global_event(AK::EVENTS::PLAY_FOOTSTEP, get<Walker>()->base_pos());
}

Vec3 MinionCommon::head_pos()
{
	return get<Target>()->absolute_pos();
}

b8 MinionCommon::headshot_test(const Vec3& ray_start, const Vec3& ray_end)
{
	return LMath::ray_sphere_intersect(ray_start, ray_end, head_pos(), MINION_HEAD_RADIUS);
}

#define TELEPORT_TIME 0.75f
void MinionCommon::update(const Update& u)
{
	attack_timer = vi_max(0.0f, attack_timer - u.time.delta);

	// update head position
	{
		get<Target>()->local_offset = Vec3(0.1f, 0, 0);
		Quat rot = Quat::identity;
		get<Animator>()->to_local(Asset::Bone::character_head, &get<Target>()->local_offset, &rot);
	}

	get<SkinnedModel>()->offset.make_transform
	(
		Vec3(0, get<Walker>()->capsule_height() * -0.5f - get<Walker>()->support_height, 0),
		Vec3(1.0f, 1.0f, 1.0f),
		Quat::euler(0, get<Walker>()->rotation + PI * 0.5f, 0)
	);

	Animator::Layer* layer = &get<Animator>()->layers[0];

	// update animation
	AssetID anim;
	if (get<Walker>()->support.ref() && get<Walker>()->dir.length_squared() > 0.0f)
	{
		r32 net_speed = vi_max(get<Walker>()->net_speed, WALK_SPEED * 0.5f);
		anim = Asset::Animation::character_walk;
		layer->speed = net_speed / get<Walker>()->speed;
	}
	else
	{
		anim = Asset::Animation::character_idle;
		layer->speed = 1.0f;
	}

	layer->play(anim);
}

void MinionCommon::hit_by(const TargetEvent& e)
{
	get<Health>()->damage(e.hit_by, get<Health>()->hp_max);
}

void MinionCommon::killed(Entity* killer)
{
	get<Audio>()->post_event(AK::EVENTS::STOP);

	Entity* ragdoll = World::create<Empty>();
	ragdoll->get<Transform>()->absolute_pos(get<Transform>()->absolute_pos());

	// Apply the SkinnedModel::offset rotation to the ragdoll transform to make everything work
	ragdoll->get<Transform>()->absolute_rot(Quat::euler(0, get<Walker>()->rotation + PI * 0.5f, 0));

	SkinnedModel* new_skin = ragdoll->add<SkinnedModel>();
	SkinnedModel* old_skin = get<SkinnedModel>();
	new_skin->mesh = old_skin->mesh;
	new_skin->shader = old_skin->shader;
	new_skin->texture = old_skin->texture;
	new_skin->color = old_skin->color;
	new_skin->team = old_skin->team;
	new_skin->mask = old_skin->mask;

	// No rotation
	new_skin->offset.make_transform(
		Vec3(0, -1.1f, 0),
		Vec3(1.0f, 1.0f, 1.0f),
		Quat::identity
	);

	Animator* new_anim = ragdoll->add<Animator>();
	Animator* old_anim = get<Animator>();
	new_anim->armature = old_anim->armature;
	new_anim->bones.resize(old_anim->bones.length);
	for (s32 i = 0; i < old_anim->bones.length; i++)
		new_anim->bones[i] = old_anim->bones[i];

	Audio::post_global_event(AK::EVENTS::PLAY_HEADSHOT, head_pos());
	World::remove_deferred(entity());

	Ragdoll* r = ragdoll->add<Ragdoll>();
	btRigidBody* head = r->get_body(Asset::Bone::character_head)->btBody;

	if (killer)
	{
		if (killer->has<Awk>())
			head->applyImpulse(killer->get<Awk>()->velocity * 0.1f, Vec3::zero);
		else
		{
			Vec3 killer_to_head = head->getWorldTransform().getOrigin() - killer->get<Transform>()->absolute_pos();
			killer_to_head.normalize();
			head->applyImpulse(killer_to_head * 10.0f, Vec3::zero);
		}
	}
}

// Minion AI

Entity* closest_target(MinionAI* me, AI::Team team, const Vec3& direction)
{
	// if the target is in the wrong direction, add a cost to it
	r32 direction_cost = direction.length_squared() > 0.0f ? 100.0f : 0.0f;

	Vec3 pos = me->get<Transform>()->absolute_pos();
	Entity* closest = nullptr;

	r32 closest_distance = FLT_MAX;

	for (auto i = ContainmentField::list.iterator(); !i.is_last(); i.next())
	{
		ContainmentField* field = i.item();
		if (field->team != team)
		{
			if (me->can_see(field->entity()))
				return field->entity();
			Vec3 to_field = field->get<Transform>()->absolute_pos() - pos;
			r32 total_distance = to_field.length_squared() + (to_field.dot(direction) < 0.0f ? direction_cost : 0.0f);
			if (total_distance < closest_distance)
			{
				closest = field->entity();
				closest_distance = total_distance;
			}
		}
	}

	for (auto i = Sensor::list.iterator(); !i.is_last(); i.next())
	{
		Sensor* sensor = i.item();
		if (sensor->team != team)
		{
			if (me->can_see(sensor->entity()))
				return sensor->entity();
			Vec3 to_sensor = sensor->get<Transform>()->absolute_pos() - pos;
			r32 total_distance = to_sensor.length_squared() + (to_sensor.dot(direction) < 0.0f ? direction_cost : 0.0f);
			if (total_distance < closest_distance)
			{
				closest = sensor->entity();
				closest_distance = total_distance;
			}
		}
	}

	for (auto i = MinionCommon::list.iterator(); !i.is_last(); i.next())
	{
		MinionCommon* minion = i.item();
		if (minion->get<AIAgent>()->team != team)
		{
			if (me->can_see(minion->entity()))
				return minion->entity();
			Vec3 to_minion = minion->get<Transform>()->absolute_pos() - pos;
			r32 total_distance = to_minion.length_squared() + (to_minion.dot(direction) < 0.0f ? direction_cost : 0.0f);
			if (total_distance < closest_distance)
			{
				closest = minion->entity();
				closest_distance = total_distance;
			}
		}
	}

	for (auto i = Rocket::list.iterator(); !i.is_last(); i.next())
	{
		Rocket* rocket = i.item();
		if (rocket->team != team && rocket->get<Transform>()->parent.ref())
		{
			if (me->can_see(rocket->entity()))
				return rocket->entity();
			Vec3 to_rocket = rocket->get<Transform>()->absolute_pos() - pos;
			r32 total_distance = to_rocket.length_squared() + (to_rocket.dot(direction) < 0.0f ? direction_cost : 0.0f);
			if (total_distance < closest_distance)
			{
				closest = rocket->entity();
				closest_distance = total_distance;
			}
		}
	}

	for (auto i = Teleporter::list.iterator(); !i.is_last(); i.next())
	{
		Teleporter* teleporter = i.item();
		if (teleporter->team != team && !teleporter->has<ControlPoint>())
		{
			if (me->can_see(teleporter->entity()))
				return teleporter->entity();
			Vec3 to_teleporter = teleporter->get<Transform>()->absolute_pos() - pos;
			r32 total_distance = to_teleporter.length_squared() + (to_teleporter.dot(direction) < 0.0f ? direction_cost : 0.0f);
			if (total_distance < closest_distance)
			{
				closest = teleporter->entity();
				closest_distance = total_distance;
			}
		}
	}

	return closest;
}

Entity* visible_target(MinionAI* me, AI::Team team)
{
	for (auto i = Awk::list.iterator(); !i.is_last(); i.next())
	{
		Awk* awk = i.item();
		if (awk->get<AIAgent>()->team != team)
		{
			if (me->can_see(awk->entity(), true))
				return awk->entity();
		}
	}

	for (auto i = MinionCommon::list.iterator(); !i.is_last(); i.next())
	{
		MinionCommon* minion = i.item();
		if (minion->get<AIAgent>()->team != team)
		{
			if (me->can_see(minion->entity()))
				return minion->entity();
		}
	}

	for (auto i = ContainmentField::list.iterator(); !i.is_last(); i.next())
	{
		ContainmentField* field = i.item();
		if (field->team != team)
		{
			if (me->can_see(field->entity()))
				return field->entity();
		}
	}

	for (auto i = Sensor::list.iterator(); !i.is_last(); i.next())
	{
		Sensor* sensor = i.item();
		if (sensor->team != team)
		{
			if (me->can_see(sensor->entity()))
				return sensor->entity();
		}
	}

	for (auto i = Rocket::list.iterator(); !i.is_last(); i.next())
	{
		Rocket* rocket = i.item();
		if (rocket->team != team && rocket->get<Transform>()->parent.ref())
		{
			if (me->can_see(rocket->entity()))
				return rocket->entity();
		}
	}

	for (auto i = Teleporter::list.iterator(); !i.is_last(); i.next())
	{
		Teleporter* teleporter = i.item();
		if (teleporter->team != team && !teleporter->has<ControlPoint>())
		{
			if (me->can_see(teleporter->entity()))
				return teleporter->entity();
		}
	}

	return nullptr;
}

void MinionAI::awake()
{
	get<Walker>()->max_speed = get<Walker>()->speed;
	new_goal(get<Walker>()->forward());
}

b8 MinionAI::can_see(Entity* target, b8 limit_vision_cone) const
{
	if (target->has<AIAgent>() && target->get<AIAgent>()->stealth)
		return false;

	Vec3 pos = get<MinionCommon>()->head_pos();
	Vec3 target_pos = target->get<Transform>()->absolute_pos();
	Vec3 diff = target_pos - pos;
	r32 distance_squared = diff.length_squared();

	// if we're targeting an awk that is flying or just flew recently,
	// then don't limit detection to the minion's vision cone
	// this essentially means the minion can hear the awk flying around
	if (limit_vision_cone
		&& target->has<Awk>())
	{
		if (distance_squared < MINION_HEARING_RANGE * MINION_HEARING_RANGE && Game::time.total - target->get<Awk>()->attach_time < 1.0f) // we can hear the awk
			limit_vision_cone = false;
		else
		{
			PlayerManager* manager = target->get<PlayerCommon>()->manager.ref();
			if (Team::list[(s32)get<AIAgent>()->team].player_tracks[manager->id()].tracking)
				limit_vision_cone = false;
		}
	}

	if (distance_squared < SENSOR_RANGE * SENSOR_RANGE)
	{
		diff.normalize();
		if (!limit_vision_cone || diff.dot(get<Walker>()->forward()) > 0.707f)
		{
			btCollisionWorld::ClosestRayResultCallback ray_callback(pos, target_pos);
			Physics::raycast(&ray_callback, (btBroadphaseProxy::StaticFilter | CollisionInaccessible | CollisionAllTeamsContainmentField) & ~Team::containment_field_mask(get<AIAgent>()->team));
			if (!ray_callback.hasHit())
				return true;
		}
	}
	return false;
}

#define PATH_RECALC_TIME 1.0f

void MinionAI::new_goal(const Vec3& direction)
{
	Vec3 pos = get<Transform>()->absolute_pos();
	goal.entity = closest_target(this, get<AIAgent>()->team, direction);
	auto path_callback = ObjectLinkEntryArg<MinionAI, const AI::Result&, &MinionAI::set_path>(id());
	if (goal.entity.ref())
	{
		goal.type = Goal::Type::Target;
		if (!can_see(goal.entity.ref()))
		{
			path_request = PathRequest::Target;
			goal.pos = goal.entity.ref()->get<Transform>()->absolute_pos();
			AI::pathfind(pos, goal.pos, path_callback);
		}
	}
	else
	{
		goal.type = Goal::Type::Position;
		if (direction.length_squared() > 0.0f)
		{
			path_request = PathRequest::Position;
			goal.pos = pos + direction * AWK_MAX_DISTANCE;
			AI::pathfind(pos, goal.pos, path_callback);
		}
		else
		{
			path_request = PathRequest::Random;
			AI::random_path(pos, path_callback);
		}
	}
	target_timer = 0.0f;
	path_timer = PATH_RECALC_TIME;
}

Vec3 goal_pos(const MinionAI::Goal& g)
{
	if (g.type == MinionAI::Goal::Type::Position || !g.entity.ref())
		return g.pos;
	else
		return g.entity.ref()->get<Transform>()->absolute_pos();
}

Teleporter* teleporter_candidate(const MinionAI* minion, const MinionAI::Goal& g)
{
	// must have the upgrade in order for minions to teleport
	PlayerManager* owner = minion->get<MinionCommon>()->owner.ref();
	if (!owner || !owner->has_upgrade(Upgrade::Teleporter))
		return nullptr;

	Vec3 target = goal_pos(g);
	r32 distance;
	Teleporter* teleporter = Teleporter::closest(1 << minion->get<AIAgent>()->team, target, &distance);
	if (teleporter && distance < (target - minion->get<Transform>()->absolute_pos()).length() - 5.0f)
		return teleporter;
	return nullptr;
}

r32 MinionAI::particle_accumulator = 0.0f;
void MinionAI::update_all(const Update& u)
{
	particle_accumulator -= u.time.delta;

	s32 count = 0;
	while (particle_accumulator < 0.0f)
	{
		const r32 TELEPORT_PARTICLE_INTERVAL = 0.02f;
		particle_accumulator += TELEPORT_PARTICLE_INTERVAL;
		count++;
	}

	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->teleport_timer > 0.0f)
		{
			Vec3 pos = i.item()->get<Walker>()->base_pos();
			for (s32 j = 0; j < count; j++)
			{
				const r32 TELEPORT_PARTICLE_RADIUS = 0.5f;
				const Vec3 particle_velocity(0, 10.0f, 0);
				Particles::sparks.add
				(
					pos + Vec3((mersenne::randf_oo() * 2.0f - 1.0f) * TELEPORT_PARTICLE_RADIUS, -0.3f, (mersenne::randf_oo() * 2.0f - 1.0f) * TELEPORT_PARTICLE_RADIUS),
					particle_velocity,
					Vec4(1, 1, 1, 1)
				);
			}
		}
		i.item()->update(u);
	}
}

// use this to set the intial value of teleport_timer such that
// minions don't all teleport at the same time
r32 MinionAI::teleport_time()
{
	r32 result = 0;
	for (auto i = list.iterator(); !i.is_last(); i.next())
		result = vi_max(result, i.item()->teleport_timer - TELEPORT_TIME);
	return result + TELEPORT_TIME * 2.0f;
}

void MinionAI::update(const Update& u)
{
	target_timer += u.time.delta;

	Vec3 pos = get<Transform>()->absolute_pos();

	if (path_request == PathRequest::None)
	{
		b8 recalc = false;
		path_timer = vi_max(0.0f, path_timer - u.time.delta);
		if (path_timer == 0.0f)
		{
			path_timer = PATH_RECALC_TIME;
			recalc = true;
		}

		if (recalc)
		{
			Entity* target_candidate = visible_target(this, get<AIAgent>()->team);
			if (target_candidate && target_candidate != goal.entity.ref())
			{
				// look, a shiny!
				path.length = 0;
				goal.type = Goal::Type::Target;
				goal.entity = target_candidate;
				target_timer = 0;
			}
		}

		if (teleport_timer > 0.0f)
		{
			r32 old_timer = teleport_timer;
			teleport_timer = vi_max(0.0f, teleport_timer - u.time.delta);
			if (teleport_timer < TELEPORT_TIME && old_timer >= TELEPORT_TIME)
			{
				path_timer = 0.0f;
				Teleporter* teleporter = teleporter_candidate(this, goal);
				if (teleporter)
				{
					teleport(entity(), teleporter);
					path.length = 0;
				}
				else
					teleport_timer = 0.0f;
			}
		}
		else
		{
			switch (goal.type)
			{
				case Goal::Type::Position:
				{
					if (path.length == 0 || (path[path.length - 1] - pos).length_squared() < 3.0f * 3.0f)
						new_goal();
					else
					{
						if (recalc)
						{
							// recalc path
							if (teleporter_candidate(this, goal))
								teleport_timer = teleport_time();
							else
							{
								path_request = PathRequest::Repath;
								AI::pathfind(pos, goal.pos, ObjectLinkEntryArg<MinionAI, const AI::Result&, &MinionAI::set_path>(id()));
							}
						}
					}
					break;
				}
				case Goal::Type::Target:
				{
					Entity* g = goal.entity.ref();
					if (g)
					{
						// we're going after the target
						if (can_see(g))
						{
							// turn to and attack the target
							Vec3 head_pos = get<MinionCommon>()->head_pos();
							Vec3 aim_pos;
							if (!g->has<Target>() || !g->get<Target>()->predict_intersection(head_pos, PROJECTILE_SPEED, &aim_pos))
								aim_pos = g->get<Transform>()->absolute_pos();
							turn_to(aim_pos);
							path.length = 0;

							Vec3 to_target = aim_pos - head_pos;
							to_target.y = 0.0f;
							if (get<MinionCommon>()->attack_timer == 0.0f // make sure our cooldown is done
								&& Vec3::normalize(to_target).dot(get<Walker>()->forward()) > 0.98f // make sure we're looking at the target
								&& target_timer > MINION_ATTACK_TIME * 0.5f // give some reaction time
								&& !Team::game_over)
							{
								PlayerManager* owner = get<MinionCommon>()->owner.ref();
								World::create<ProjectileEntity>(owner ? owner->entity.ref() : nullptr, head_pos, aim_pos - head_pos);
								get<MinionCommon>()->attack_timer = MINION_ATTACK_TIME;
							}
						}
						else
						{
							if (goal.entity.ref()->has<Awk>()) // if we can't see the Awk anymore, let them go
								new_goal();
							else
							{
								if (recalc)
								{
									// recalc path
									if (teleporter_candidate(this, goal))
										teleport_timer = teleport_time();
									else
									{
										path_request = PathRequest::Target;
										AI::pathfind(pos, g->get<Transform>()->absolute_pos(), ObjectLinkEntryArg<MinionAI, const AI::Result&, &MinionAI::set_path>(id()));
									}
								}
							}
						}
					}
					else
						new_goal();
					break;
				}
				default:
				{
					vi_assert(false);
					break;
				}
			}
		}
	}

	// path following

	if (path_index < path.length)
	{
		Vec3 flat_pos = pos;
		flat_pos.y = 0.0f;
		Vec3 t = path[path_index];
		t.y = 0.0f;
		Vec3 ray = t - flat_pos;
		while (ray.length_squared() < 0.05f * 0.05f)
		{
			path_index++;
			if (path_index == path.length)
				break;
			t = path[path_index];
			t.y = 0.0f;
			ray = t - flat_pos;
		}

		r32 ray_length = ray.length();
		if (ray_length > 0.1f)
		{
			ray /= ray_length;
			get<Walker>()->dir = Vec2(ray.x, ray.z);
		}
	}
	else
		get<Walker>()->dir = Vec2::zero;
}

void MinionAI::set_path(const AI::Result& result)
{
	path = result.path;
	if (path_request != PathRequest::Repath)
	{
		if (path.length > 0)
			goal.pos = path[path.length - 1];
		else
			goal.pos = get<Transform>()->absolute_pos();
	}
	path_request = PathRequest::None;
	path_index = 0;
}

void MinionAI::turn_to(const Vec3& target)
{
	Vec3 forward = Vec3::normalize(target - get<Transform>()->absolute_pos());
	get<Walker>()->target_rotation = atan2f(forward.x, forward.z);
}


}