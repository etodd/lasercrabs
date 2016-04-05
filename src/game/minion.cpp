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

#define MINION_VIEW_RANGE 15.0f

#define WALK_SPEED 1.0f

#define HEALTH 5

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
	return LMath::ray_sphere_intersect(ray_start, ray_end, head_pos(), MINION_HEAD_RADIUS);
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

// Minion AI

void MinionAI::awake()
{
	get<Walker>()->max_speed = get<Walker>()->speed;
	new_goal();
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
		Physics::raycast(&ray_callback, btBroadphaseProxy::StaticFilter | CollisionInaccessible);
		if (!ray_callback.hasHit())
			return true;
	}
	return false;
}

#define PATH_RECALC_TIME 1.0f

void MinionAI::new_goal()
{
	Vec3 pos = get<Transform>()->absolute_pos();
	goal.entity = closest_enemy_sensor(pos, get<AIAgent>()->team);
	auto path_callback = ObjectLinkEntryArg<MinionAI, const AI::Path&, &MinionAI::set_path>(id());
	path_timer = PATH_RECALC_TIME;
	path_request_active = true;
	if (goal.entity.ref())
		AI::pathfind(pos, goal.entity.ref()->get<Transform>()->absolute_pos(), path_callback);
	else
		AI::random_path(pos, path_callback);
}

void MinionAI::update(const Update& u)
{
	Vec3 pos = get<Transform>()->absolute_pos();

	b8 goal_reached = false;
	if (goal.entity.ref())
	{
		if (can_see(goal.entity.ref()))
		{
			Vec3 target_pos = goal.entity.ref()->get<Transform>()->absolute_pos();
			turn_to(target_pos);

			Vec3 head_pos = get<MinionCommon>()->head_pos();

			Vec3 to_target = target_pos - head_pos;
			to_target.y = 0.0f;
			if (attack_timer == 0.0f && Vec3::normalize(to_target).dot(get<Walker>()->forward()) > 0.9f)
			{
				World::create<ProjectileEntity>(entity(), head_pos, 1, target_pos - head_pos);
				attack_timer = 1.0f;
			}
		}
	}
	else
		goal_reached = (goal.pos - pos).length_squared() < 3.0f * 3.0f;

	attack_timer = vi_max(0.0f, attack_timer - u.time.delta);

	if (goal_reached && !path_request_active)
		new_goal();

	if (!path_request_active)
		path_timer = vi_max(0.0f, path_timer - u.time.delta);

	if (!goal_reached && path_timer == 0.0f)
	{
		// recalc path
		path_timer = PATH_RECALC_TIME;
		path_request_active = true;
		AI::pathfind(pos, goal.entity.ref() ? goal.entity.ref()->get<Transform>()->absolute_pos() : goal.pos, ObjectLinkEntryArg<MinionAI, const AI::Path&, &MinionAI::set_path>(id()));
	}

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

void MinionAI::set_path(const AI::Path& p)
{
	path = p;
	if (p.length > 0)
		goal.pos = p[p.length - 1];
	Vec3 pos = get<Transform>()->absolute_pos();
	printf("%f %f %f %f %f %f\n", goal.pos.x, goal.pos.y, goal.pos.z, pos.x, pos.y, pos.z);
	path_request_active = false;
	path_index = 0;
}

void MinionAI::turn_to(const Vec3& target)
{
	Vec3 forward = Vec3::normalize(target - get<Transform>()->absolute_pos());
	get<Walker>()->target_rotation = atan2(forward.x, forward.z);
}


}