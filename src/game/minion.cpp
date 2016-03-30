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

#define WALK_SPEED 1.5f

#define HEALTH 50

namespace VI
{

Minion::Minion(const Vec3& pos, const Quat& quat, AI::Team team, PlayerManager* manager)
{
	Transform* transform = create<Transform>();
	transform->pos = pos;

	Animator* animator = create<Animator>();
	SkinnedModel* model = create<SkinnedModel>();

	animator->armature = Asset::Armature::character_mesh;
	animator->layers[0].animation = Asset::Animation::character_idle;

	model->shader = Asset::Shader::armature;
	model->mesh = Asset::Mesh::character_mesh;
	model->color = Team::colors[(s32)team];
	model->color.w = MATERIAL_NO_OVERRIDE;

	create<Audio>();

	Health* health = create<Health>(HEALTH, HEALTH);
	
	Vec3 forward = quat * Vec3(0, 0, 1);

	Walker* walker = create<Walker>(atan2f(forward.x, forward.z));
	walker->max_speed = WALK_SPEED;

	create<MinionCommon>();

	create<AIAgent>()->team = team;

	PointLight* light = create<PointLight>();
	light->color = Team::colors[(s32)team].xyz();
	light->type = PointLight::Type::Override;
	light->team_mask = 1 << (s32)team;
	light->radius = SENSOR_RANGE;

	create<Sensor>(team, manager);
	create<PlayerTrigger>()->radius = SENSOR_RANGE;

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
	Audio::post_global_event(AK::EVENTS::PLAY_FOOTSTEP, get<Walker>()->base_pos());
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
	Vec3 head = head_pos();

	Vec3 ray = ray_end - ray_start;
	Vec3 head_to_ray_start = ray_start - head;

	r32 a = ray.length_squared();
	r32 b = 2.0f * ray.dot(head_to_ray_start);
	r32 c = head_to_ray_start.length_squared() - (HEAD_RADIUS * HEAD_RADIUS);

	r32 delta = (b * b) - 4.0f * a * c;

	return delta >= 0.0f;
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
		r32 net_speed = vi_max(get<Walker>()->net_speed, WALK_SPEED * 0.5f);
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

Entity* closest_enemy_sensor(const Vec3& pos, AI::Team team)
{
	Entity* closest = nullptr;

	r32 closest_distance = FLT_MAX;
	for (auto i = Sensor::list.iterator(); !i.is_last(); i.next())
	{
		Sensor* sensor = i.item();
		if (sensor->team != team && !sensor->has<MinionAI>())
		{
			Vec3 sensor_pos = sensor->get<Transform>()->absolute_pos();

			r32 total_distance = (sensor_pos - pos).length();
			if (total_distance < closest_distance)
			{
				closest = sensor->entity();
				closest_distance = total_distance;
			}
		}
	}

	return closest;
}

void MinionPath::run()
{
	minion->path_valid = false;
	auto path_callback = ObjectLinkEntryArg<MinionAI, const AI::Path&, &MinionAI::set_path>(minion->id());

	Vec3 pos = minion->get<Transform>()->absolute_pos();

	// find new target
	Entity* target = closest_enemy_sensor(pos, minion->get<AIAgent>()->team);

	if (target)
	{
		if (minion->can_see(target))
			done(true);
		else
		{
			goal = target->get<Transform>()->absolute_pos();
			random_path = false;
			AI::pathfind(pos, goal, path_callback);
			active(true);
		}
	}
	else
	{
		random_path = true;
		AI::random_path(pos, path_callback);
		active(true);
	}
}

void MinionPath::update(const Update& u)
{
	if (minion->path_valid)
	{
		path_timer += u.time.delta;
		if (minion->path_index >= minion->path.length)
			done(true);
		else if (!random_path && path_timer > 1.0f)
		{
			// recalc
			minion->path_valid = false;
			AI::pathfind(minion->get<Transform>()->absolute_pos(), goal, ObjectLinkEntryArg<MinionAI, const AI::Path&, &MinionAI::set_path>(minion->id()));
		}
	}
}

void MinionPath::abort()
{
	minion->path.length = 0;
	MinionBehavior<MinionPath>::abort();
}

void MinionAttack::run()
{
	Entity* sensor = closest_enemy_sensor(minion->get<Transform>()->absolute_pos(), minion->get<AIAgent>()->team);
	if (sensor && minion->can_see(sensor))
	{
		target = sensor;
		minion->turn_to(sensor->get<Transform>()->absolute_pos());
		active(true);
	}
	else
	{
		target = nullptr;
		done(false);
	}
}

void MinionAttack::update(const Update& u)
{
	if (target.ref())
	{
		Vec3 head_pos = minion->get<MinionCommon>()->head_pos();
		Vec3 target_pos = minion->get<Transform>()->absolute_pos();
		minion->turn_to(target_pos);
		Vec3 to_target = target_pos - head_pos;
		to_target.y = 0.0f;
		if (Vec3::normalize(to_target).dot(minion->get<Walker>()->forward()) > 0.9f)
		{
			RaycastCallbackExcept ray_callback(head_pos, target_pos, minion->entity());
			Physics::raycast(&ray_callback);
			if (!ray_callback.hasHit() || ray_callback.m_collisionObject->getUserIndex() == target.id)
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
		MinionPath::update_active(u);
		MinionAttack::update_active(u);
	}
}

// Minion AI

MinionAI::MinionAI()
{
	behavior = Repeat::alloc
	(
		Succeed::alloc
		(
			Sequence::alloc
			(
				MinionPath::alloc(),
				MinionAttack::alloc(),
				Delay::alloc(3.0f)
			)
		)
	);
	behavior->set_context(this);
}

void MinionAI::awake()
{
	get<Walker>()->max_speed = get<Walker>()->speed;
	behavior->run();
}

MinionAI::~MinionAI()
{
	behavior->~Behavior();
}

b8 MinionAI::can_see(Entity* target) const
{
	if (target->has<AIAgent>() && get<AIAgent>()->stealth)
		return false;

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

void MinionAI::update(const Update& u)
{
	Vec3 pos;
	Quat rot;
	get<Transform>()->absolute(&pos, &rot);
	Vec3 forward = rot * Vec3(0, 0, 1);
	
	if (path_index < path.length)
	{
		Vec3 flat_pos = pos;
		flat_pos.y = 0.0f;
		Vec3 t = path[path_index];
		t.y = 0.0f;
		Vec3 ray = t - flat_pos;
		while (ray.length() < 0.1f)
		{
			path_index++;
			if (path_index == path.length)
				break;
			t = path[path_index];
			t.y = 0.0f;
			ray = t - flat_pos;
		}

		// Check if we're still on the path
		if (Game::time.total - last_path_recalc > 0.25f)
		{
			if (path_index > 0)
			{
				if (path_index < path.length)
				{
					Vec3 last = path[path_index - 1];
					last.y = 0.0f;
					Vec3 next = path[path_index];
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
	path.length = 0;

	Vec3 pos = get<Walker>()->base_pos();

	AI::pathfind(pos, target, ObjectLinkEntryArg<MinionAI, const AI::Path&, &MinionAI::set_path>(id()));
}

void MinionAI::set_path(const AI::Path& p)
{
	path = p;
	path_valid = true;
}

void MinionAI::recalc_path(const Update& u)
{
	last_path_recalc = Game::time.total;
	go(path[path.length - 1]);
}

void MinionAI::turn_to(const Vec3& target)
{
	Vec3 forward = Vec3::normalize(target - get<Transform>()->absolute_pos());
	get<Walker>()->target_rotation = atan2(forward.x, forward.z);
}


}
