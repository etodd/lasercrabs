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

#define HEAD_RADIUS 0.25f

#define WALK_SPEED 2.0f

#define HEALTH 50
#define VULNERABLE_HEALTH 26

namespace VI
{

Minion::Minion(const Vec3& pos, const Quat& quat, AI::Team team)
{
	Transform* transform = create<Transform>();
	transform->pos = pos;

	Animator* animator = create<Animator>();
	SkinnedModel* model = create<SkinnedModel>();

	animator->armature = Asset::Armature::character_mesh;
	animator->layers[0].animation = Asset::Animation::character_idle;

	model->shader = Asset::Shader::armature;
	model->mesh = Asset::Mesh::character_mesh;
	model->color = AI::colors[(s32)team];
	model->color.w = 1.0f / 255.0f; // special G-buffer index prevents override lights from overriding this color

	create<Audio>();

	Health* health = create<Health>(HEALTH);
	
	Vec3 forward = quat * Vec3(0, 0, 1);

	Walker* walker = create<Walker>(atan2f(forward.x, forward.z));
	walker->max_speed = WALK_SPEED;

	create<MinionCommon>();

	create<AIAgent>()->team = team;

	PointLight* light = create<PointLight>();
	light->color = AI::colors[(s32)team].xyz();
	light->type = PointLight::Type::Override;
	light->radius = MINION_VIEW_RANGE;
	light->mask = ~(1 << (s32)team); // don't display to fellow teammates

	create<MinionAI>();
}

void MinionCommon::awake()
{
	link_arg<Entity*, &MinionCommon::killed>(get<Health>()->killed);

	Animator* animator = get<Animator>();
	animator->layers[1].loop = false;
	link<&MinionCommon::footstep>(animator->trigger(Asset::Animation::character_walk, 0.3375f));
	link<&MinionCommon::footstep>(animator->trigger(Asset::Animation::character_walk, 0.75f));
	link<&MinionCommon::footstep>(animator->trigger(Asset::Animation::character_run, 0.216f));
	link<&MinionCommon::footstep>(animator->trigger(Asset::Animation::character_run, 0.476f));
}

void MinionCommon::footstep()
{
	Vec3 base_pos = get<Walker>()->base_pos();

	Audio::post_global_event(AK::EVENTS::PLAY_FOOTSTEP, base_pos);

	ShockwaveEntity* shockwave = World::create<ShockwaveEntity>(entity(), 3.0f);
	shockwave->get<Transform>()->pos = base_pos;
	shockwave->get<Transform>()->reparent(get<Transform>()->parent.ref());
}

Vec3 MinionCommon::head_pos()
{
	Vec3 pos = Vec3(0.1f, 0, 0);
	Quat rot = Quat::identity;
	get<Animator>()->to_world(Asset::Bone::character_head, &pos, &rot);
	return pos;
}

b8 MinionCommon::headshot_test(const Vec3& ray_start, const Vec3& ray_end)
{
	if (get<Health>()->hp <= VULNERABLE_HEALTH)
	{
		Vec3 head = head_pos();

		Vec3 ray = ray_end - ray_start;
		Vec3 head_to_ray_start = ray_start - head;

		r32 a = ray.length_squared();
		r32 b = 2.0f * ray.dot(head_to_ray_start);
		r32 c = head_to_ray_start.length_squared() - (HEAD_RADIUS * HEAD_RADIUS);

		r32 delta = (b * b) - 4.0f * a * c;

		return delta >= 0.0f;
	}
	else
		return false;
}

void MinionCommon::update(const Update& u)
{
	get<SkinnedModel>()->offset.make_transform(
		Vec3(0, -1.1f, 0),
		Vec3(1.0f, 1.0f, 1.0f),
		Quat::euler(0, get<Walker>()->rotation + PI * 0.5f, 0)
	);

	Animator::Layer* layer = &get<Animator>()->layers[0];

	// update animation
	AssetID anim;
	if (get<Walker>()->support.ref() && get<Walker>()->dir.length_squared() > 0.0f)
	{
		r32 net_speed = fmax(get<Walker>()->net_speed, WALK_SPEED * 0.5f);
		anim = Asset::Animation::character_walk;
		layer->speed = net_speed / get<Walker>()->speed;
	}
	else
	{
		anim = Asset::Animation::character_idle;
		layer->speed = 1.0f;
	}

	layer->animation = anim;
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

	// No rotation
	new_skin->offset.make_transform(
		Vec3(0, -1.1f, 0),
		Vec3(1.0f, 1.0f, 1.0f),
		Quat::identity
	);

	new_skin->color = old_skin->color;

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

// Minion behaviors

r32 minion_priority(Entity* e)
{
	if (e->has<MinionAI>())
		return 2;
	else
		return 1;
}

void MinionCheckTarget::run()
{
	AI::Team team = minion->get<AIAgent>()->team;
	Vec3 pos = minion->get<Transform>()->absolute_pos();

	// if we are targeting an enemy
	// make sure we still want to do that
	if (minion->target.ref() && !minion->target.ref()->has<TurretControl>() && minion->can_see(minion->target.ref()))
	{
		done(true);
		return;
	}

	// find new target
	Entity* new_target = nullptr;

	r32 target_priority = 0;
	for (auto i = AIAgent::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team != team && minion->can_see(i.item()->entity()))
		{
			r32 candidate_priority = minion_priority(i.item()->entity());
			if (candidate_priority > target_priority)
			{
				new_target = i.item()->entity();
				target_priority = candidate_priority;
			}
		}
	}

	// go to next turret
	if (!new_target)
	{
		float closest_distance = FLT_MAX;
		for (auto i = TurretControl::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->get<AIAgent>()->team != team)
			{
				Vec3 turret_pos = i.item()->get<Transform>()->absolute_pos();

				float total_distance = (turret_pos - pos).length();
				for (auto j = PlayerManager::list.iterator(); !j.is_last(); j.next())
				{
					if (j.item()->team.ref()->team() != team)
					{
						total_distance += (j.item()->team.ref()->player_spawn.ref()->absolute_pos() - turret_pos).length();
						break;
					}
				}

				if (total_distance < closest_distance)
				{
					new_target = i.item()->entity();
					closest_distance = total_distance;
				}
			}
		}

		// go to player spawn if necessary
		for (auto j = PlayerManager::list.iterator(); !j.is_last(); j.next())
		{
			if (j.item()->team.ref()->team() != team)
			{
				Transform* player_spawn = j.item()->team.ref()->player_spawn.ref();
				if (player_spawn)
				{
					float distance = (player_spawn->absolute_pos() - pos).length();
					if (distance < closest_distance * 0.5f)
						new_target = player_spawn->entity();
					break;
				}
			}
		}
	}

	if (minion->target.ref() == new_target)
		done(true);
	else
	{
		minion->target = new_target;
		done(false);
	}
}

void MinionGoToTarget::run()
{
	path_timer = 0.0f;
	if (minion->target.ref())
	{
		Vec3 target_pos = minion->target.ref()->get<Transform>()->absolute_pos();
		if (target_in_range())
		{
			Vec3 forward = target_pos - minion->get<Transform>()->absolute_pos();
			forward.normalize();
			minion->get<Walker>()->target_rotation = atan2(forward.x, forward.z);
			done(true);
		}
		else
		{
			minion->go(target_pos);
			active(true);
		}
	}
	else
		done(false);
}

void MinionGoToTarget::done(b8 success)
{
	minion->path_point_count = 0;
	MinionBehavior<MinionGoToTarget>::done(success);
}

b8 MinionGoToTarget::target_in_range() const
{
	Vec3 pos = minion->get<Transform>()->absolute_pos();
	Vec3 target_pos = minion->target.ref()->get<Transform>()->absolute_pos();
	if ((pos - target_pos).length_squared() < MINION_VIEW_RANGE *  MINION_VIEW_RANGE)
	{
		RaycastCallbackExcept ray_callback(pos, target_pos, minion->entity());
		Physics::raycast(&ray_callback);
		if (!ray_callback.hasHit() || ray_callback.m_collisionObject->getUserIndex() == minion->target.ref()->id())
			return true;
	}
	return false;
}

void MinionGoToTarget::update(const Update& u)
{
	path_timer += u.time.delta;
	if (!minion->target.ref())
		done(false);
	else if (target_in_range())
		done(true);
	else if (path_timer > 0.5f)
		run();
}

void MinionAttack::run()
{
	Entity* target = minion->target.ref();
	if (target)
	{
		Vec3 target_pos = target->get<Transform>()->absolute_pos();
		minion->turn_to(target_pos);
		active(true);
	}
	else
		done(false);
}

void MinionAttack::update(const Update& u)
{
	Entity* target = minion->target.ref();
	if (target)
	{
		Vec3 head_pos = minion->get<MinionCommon>()->head_pos();
		Vec3 target_pos = target->get<Transform>()->absolute_pos();
		minion->turn_to(target_pos);
		Vec3 to_target = target_pos - head_pos;
		to_target.y = 0.0f;
		if (Vec3::normalize(to_target).dot(minion->get<Walker>()->forward()) > 0.9f)
		{
			RaycastCallbackExcept ray_callback(head_pos, target_pos, minion->entity());
			Physics::raycast(&ray_callback);
			if (!ray_callback.hasHit() || ray_callback.m_collisionObject->getUserIndex() == minion->target.ref()->id())
			{
				World::create<ProjectileEntity>(minion->entity(), head_pos, 5, target_pos - head_pos);
				done(true);
			}
			else
				done(false);
		}
	}
	else
		done(false);
}

namespace MinionBehaviors
{
	void update_active(const Update& u)
	{
		MinionGoToTarget::update_active(u);
		MinionAttack::update_active(u);
	}
}

// Minion AI

MinionAI::MinionAI()
{
	behavior = Repeat::alloc
	(
		Sequence::alloc
		(
			Succeed::alloc
			(
				Parallel::alloc
				(
					Repeat::alloc
					(
						Sequence::alloc
						(
							MinionCheckTarget::alloc(),
							Delay::alloc(1.0f)
						)
					),
					Repeat::alloc
					(
						Sequence::alloc
						(
							MinionGoToTarget::alloc(),
							MinionAttack::alloc(),
							Delay::alloc(1.0f)
						)
					)
				)
			),
			Delay::alloc(0.0f)
		)
	);
	behavior->set_context(this);
}

void MinionAI::awake()
{
	get<Walker>()->max_speed = get<Walker>()->speed;
	behavior->run();
	link_arg<Entity*, &MinionAI::damaged>(get<Health>()->damaged);
	damaged(nullptr); // play vulnerable animation if we spawn at low health
}

MinionAI::~MinionAI()
{
	behavior->~Behavior();
}

b8 MinionAI::can_see(Entity* target) const
{
	Vec3 pos = get<Transform>()->absolute_pos();
	Vec3 target_pos = target->get<Transform>()->absolute_pos();
	if ((target_pos - pos).length_squared() < MINION_VIEW_RANGE * MINION_VIEW_RANGE)
	{
		btCollisionWorld::ClosestRayResultCallback ray_callback(pos, target_pos);
		Physics::raycast(&ray_callback);
		if (!ray_callback.hasHit() || ray_callback.m_collisionObject->getUserIndex() == target->id())
			return true;
	}
	return false;
}

void MinionAI::damaged(Entity* enemy)
{
	target = enemy;
	if (get<Health>()->hp <= VULNERABLE_HEALTH)
	{
		Animator::Layer* layer = &get<Animator>()->layers[2];
		if (layer->animation != Asset::Animation::character_vulnerable)
			layer->play(Asset::Animation::character_vulnerable);
	}
}

void MinionAI::update(const Update& u)
{
	Vec3 pos;
	Quat rot;
	get<Transform>()->absolute(&pos, &rot);
	Vec3 forward = rot * Vec3(0, 0, 1);
	
	if (path_index < path_point_count)
	{
		Vec3 flat_pos = pos;
		flat_pos.y = 0.0f;
		Vec3 t = path_points[path_index];
		t.y = 0.0f;
		Vec3 ray = t - flat_pos;
		while (ray.length() < 0.1f)
		{
			path_index++;
			if (path_index == path_point_count)
				break;
			t = path_points[path_index];
			t.y = 0.0f;
			ray = t - flat_pos;
		}

		// Check if we're still on the path
		if (Game::time.total - last_path_recalc > 0.25f)
		{
			if (path_index > 0)
			{
				if (path_index < path_point_count)
				{
					Vec3 last = path_points[path_index - 1];
					last.y = 0.0f;
					Vec3 next = path_points[path_index];
					next.y = 0.0f;
					Vec3 last_to_next = next - last;
					r32 last_to_next_distance = last_to_next.length();
					Vec3 last_to_next_dir = last_to_next / last_to_next_distance;
					Vec3 last_to_pos = flat_pos - last;
					r32 dot = last_to_next_dir.dot(last_to_pos);
					Vec3 desired_location = last + last_to_next_dir * dot;
					if (dot < -1.0f || dot > last_to_next_distance || (desired_location - flat_pos).length() > 0.5f)
						recalc_path(u); // We're off the path
				}
			}
			else
			{
				if (AI::get_poly(pos, AI::default_search_extents) != path_polys[0])
					recalc_path(u);
			}
		}

		ray.normalize();
		get<Walker>()->dir = Vec2(ray.x, ray.z);
	}
	else
		get<Walker>()->dir = Vec2::zero;
}

void MinionAI::go(const Vec3& target)
{
	path_index = 0;
	path_point_count = 0;

	Vec3 pos = get<Walker>()->base_pos();
	dtPolyRef start_poly = AI::get_poly(pos, AI::default_search_extents);
	dtPolyRef end_poly = AI::get_poly(target, AI::default_search_extents);

	if (!start_poly || !end_poly)
		return;

	dtPolyRef path_polys[MAX_POLYS];
	dtPolyRef path_parents[MAX_POLYS];
	u8 path_straight_flags[MAX_POLYS];
	dtPolyRef path_straight_polys[MAX_POLYS];
	s32 path_poly_count;

	AI::nav_mesh_query->findPath(start_poly, end_poly, (r32*)&pos, (r32*)&target, &AI::default_query_filter, path_polys, &path_poly_count, MAX_POLYS);
	if (path_poly_count)
	{
		// In case of partial path, make sure the end point is clamped to the last polygon.
		Vec3 epos = target;
		if (path_polys[path_poly_count - 1] != end_poly)
			AI::nav_mesh_query->closestPointOnPoly(path_polys[path_poly_count - 1], (r32*)&target, (r32*)&epos, 0);
		
		s32 point_count;
		AI::nav_mesh_query->findStraightPath((r32*)&pos, (r32*)&target, path_polys, path_poly_count,
									 (r32*)path_points, path_straight_flags,
									 path_straight_polys, &point_count, MAX_POLYS, 0);
		path_point_count = point_count;
	}
}

void MinionAI::recalc_path(const Update& u)
{
	last_path_recalc = Game::time.total;
	go(path_points[path_point_count - 1]);
}

void MinionAI::turn_to(const Vec3& target)
{
	Vec3 forward = Vec3::normalize(target - get<Transform>()->absolute_pos());
	get<Walker>()->target_rotation = atan2(forward.x, forward.z);
}


}
