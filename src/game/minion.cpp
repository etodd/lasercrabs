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

#define MINION_VIEW_RANGE 30.0f

#define WALK_SPEED 3.0f

#define HEALTH 5

namespace VI
{

Minion::Minion(const Vec3& pos, const Quat& quat, AI::Team team, PlayerManager* manager)
{
	Transform* transform = create<Transform>();
	transform->pos = pos;

	create<Teleportee>();

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

	create<MinionCommon>()->owner = manager;

	create<AIAgent>()->team = team;

	create<Target>();

	create<MinionAI>();

	create<PlayerTrigger>()->radius = 0.0f;

	if (manager && manager->minion_containment_fields())
		get<MinionCommon>()->create_containment_field();
}

void MinionCommon::awake()
{
	link_arg<const TargetEvent&, &MinionCommon::hit_by>(get<Target>()->target_hit);
	link_arg<Entity*, &MinionCommon::killed>(get<Health>()->killed);

	Animator* animator = get<Animator>();
	animator->layers[1].loop = false;
	link<&MinionCommon::footstep>(animator->trigger(Asset::Animation::character_walk, 0.3375f));
	link<&MinionCommon::footstep>(animator->trigger(Asset::Animation::character_walk, 0.75f));
	link_arg<r32, &MinionCommon::landed>(get<Walker>()->land);
	link_arg<Entity*, &MinionCommon::player_exited>(get<PlayerTrigger>()->exited);
}

MinionCommon::~MinionCommon()
{
	if (containment_field.ref())
		World::remove_deferred(containment_field.ref());
}

void MinionCommon::create_containment_field()
{
	get<PlayerTrigger>()->radius = AWK_MAX_DISTANCE;

	Entity* f = World::alloc<Empty>();
	f->get<Transform>()->absolute_pos(get<Transform>()->absolute_pos());

	View* view = f->add<View>();
	AI::Team team = get<AIAgent>()->team;
	view->team = (u8)team;
	view->mesh = Asset::Mesh::containment_field;
	view->shader = Asset::Shader::flat;
	view->alpha();
	view->color.w = 0.2f;

	const Mesh* mesh = Loader::mesh(view->mesh);

	CollisionGroup team_mask;
	switch (team)
	{
		case AI::Team::A:
		{
			team_mask = CollisionTeamAContainmentField;
			break;
		}
		case AI::Team::B:
		{
			team_mask = CollisionTeamBContainmentField;
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}
	f->add<RigidBody>(RigidBody::Type::Mesh, Vec3::zero, 0.0f, team_mask, CollisionContainmentField, view->mesh);

	containment_field = f;
}

void MinionCommon::player_exited(Entity* player)
{
	if (player->get<AIAgent>()->team != get<AIAgent>()->team)
	{
		Vec3 pos;
		Quat rot;
		player->get<Transform>()->absolute(&pos, &rot);

		player->get<Health>()->damage(entity(), 1);

		for (s32 i = 0; i < 50; i++)
		{
			Particles::sparks.add
			(
				pos,
				rot * Vec3(mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo()) * 10.0f,
				Vec4(1, 1, 1, 1)
			);
		}
	}
}

void MinionCommon::landed(r32 speed)
{
	if (speed < -10.0f)
		get<Health>()->damage(nullptr, HEALTH);
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

void MinionCommon::update(const Update& u)
{
	// update head position
	{
		get<Target>()->local_offset = Vec3(0.1f, 0, 0);
		Quat rot = Quat::identity;
		get<Animator>()->to_local(Asset::Bone::character_head, &get<Target>()->local_offset, &rot);
	}

	if (containment_field.ref())
		containment_field.ref()->get<Transform>()->absolute_pos(get<Transform>()->absolute_pos());

	get<SkinnedModel>()->offset.make_transform(
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

	// No rotation
	new_skin->offset.make_transform(
		Vec3(0, -1.1f, 0),
		Vec3(1.0f, 1.0f, 1.0f),
		Quat::identity
	);

	new_skin->color = old_skin->color;
	new_skin->team = old_skin->team;

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
		if (killer->has<PlayerCommon>())
		{
			Team* team = &Team::list[(s32)get<AIAgent>()->team];
			if (team->team() != killer->get<AIAgent>()->team)
				team->track(killer->get<PlayerCommon>()->manager.ref(), owner.ref());
		}

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

Entity* closest_target_to(const Vec3& pos, AI::Team team)
{
	Entity* closest = nullptr;

	r32 closest_distance = FLT_MAX;
	for (auto i = Sensor::list.iterator(); !i.is_last(); i.next())
	{
		Sensor* sensor = i.item();
		if (sensor->team != team && !sensor->has<MinionCommon>())
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

	for (auto i = MinionCommon::list.iterator(); !i.is_last(); i.next())
	{
		MinionCommon* minion = i.item();
		if (minion->get<AIAgent>()->team != team)
		{
			Vec3 teleporter_pos = minion->get<Transform>()->absolute_pos();

			r32 total_distance = (teleporter_pos - pos).length();
			if (total_distance < closest_distance)
			{
				closest = minion->entity();
				closest_distance = total_distance;
			}
		}
	}

	return closest;
}

void MinionAI::awake()
{
	get<Walker>()->max_speed = get<Walker>()->speed;
	new_goal();
}

b8 MinionAI::can_see(Entity* target) const
{
	if (target->has<AIAgent>() && get<AIAgent>()->stealth)
		return false;

	Vec3 pos = get<MinionCommon>()->head_pos();
	Vec3 target_pos = target->get<Transform>()->absolute_pos();
	Vec3 diff_flattened = target_pos - pos;
	diff_flattened.y = 0.0f;
	if (diff_flattened.length_squared() < MINION_VIEW_RANGE * MINION_VIEW_RANGE)
	{
		btCollisionWorld::ClosestRayResultCallback ray_callback(pos, target_pos);
		Physics::raycast(&ray_callback, btBroadphaseProxy::StaticFilter | CollisionInaccessible);
		if (!ray_callback.hasHit())
			return true;
	}
	return false;
}

#define PATH_RECALC_TIME 1.0f

void MinionAI::teleport_if_necessary(const Vec3& target)
{
	// find a teleporter to use
	Teleporter* closest_teleporter = Teleporter::closest(target, get<AIAgent>()->team);
	Vec3 closest_teleporter_pos;
	Quat closest_teleporter_rot;
	r32 closest_distance;
	if (closest_teleporter)
	{
		closest_teleporter->get<Transform>()->absolute(&closest_teleporter_pos, &closest_teleporter_rot);
		closest_distance = (closest_teleporter_pos - target).length_squared();

		Vec3 pos = get<Transform>()->absolute_pos();
		if (closest_teleporter && (closest_distance < (pos - target).length_squared())) // use the teleporter
		{
			get<Teleportee>()->target = closest_teleporter;
			get<Teleportee>()->go();
		}
	}
}

void MinionAI::new_goal()
{
	Vec3 pos = get<Transform>()->absolute_pos();
	goal.entity = closest_target_to(pos, get<AIAgent>()->team);
	auto path_callback = ObjectLinkEntryArg<MinionAI, const AI::Result&, &MinionAI::set_path>(id());
	if (goal.entity.ref())
	{
		goal.type = Goal::Type::Target;
		if (!can_see(goal.entity.ref()))
		{
			Vec3 target = goal.entity.ref()->get<Transform>()->absolute_pos();
			teleport_if_necessary(target);
			path_request = PathRequest::Target;
			path_timer = PATH_RECALC_TIME;
			AI::pathfind(pos, target, path_callback);
		}
	}
	else
	{
		path_request = PathRequest::Random;
		goal.type = Goal::Type::Position;
		AI::random_path(pos, path_callback);
	}
}

void MinionAI::find_goal_near(const Vec3& target)
{
	Entity* entity = closest_target_to(target, get<AIAgent>()->team);
	Vec3 end;
	if (entity)
		end = entity->get<Transform>()->absolute_pos();
	else
		end = target;

	teleport_if_necessary(end);

	auto path_callback = ObjectLinkEntryArg<MinionAI, const AI::Result&, &MinionAI::set_path>(id());
	Vec3 pos = get<Transform>()->absolute_pos();
	if (entity && (end - target).length_squared() < AWK_MAX_DISTANCE)
	{
		goal.entity = entity;
		goal.type = Goal::Type::Target;
		path_request = PathRequest::Target;
		path_timer = PATH_RECALC_TIME;
		AI::pathfind(pos, end, path_callback);
	}
	else
	{
		path_request = PathRequest::Position;
		goal.type = Goal::Type::Position;
		AI::pathfind(pos, end, path_callback);
	}
}

void MinionAI::update(const Update& u)
{
	Vec3 pos = get<Transform>()->absolute_pos();

	if (path_request == PathRequest::None)
	{
		b8 need_new_goal = false;
		b8 enable_recalc = true;
		switch (goal.type)
		{
			case Goal::Type::Position:
			{
				if (path.length == 0 || (path[path.length - 1] - pos).length_squared() < 3.0f * 3.0f)
					need_new_goal = true;
				break;
			}
			case Goal::Type::Target:
			{
				attack_timer = vi_max(0.0f, attack_timer - u.time.delta);
				if (goal.entity.ref())
				{
					// we're going after the sensor
					if (can_see(goal.entity.ref()))
					{
						// turn to and attack the sensor
						Vec3 target_pos = goal.entity.ref()->get<Transform>()->absolute_pos();
						turn_to(target_pos);
						enable_recalc = false;
						path.length = 0;

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
					need_new_goal = true;
				break;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}

		if (need_new_goal)
			new_goal();
		else if (enable_recalc)
		{
			path_timer = vi_max(0.0f, path_timer - u.time.delta);
			if (path_timer == 0.0f)
			{
				// recalc path
				path_timer = PATH_RECALC_TIME;
				path_request = PathRequest::Repath;
				AI::pathfind(pos, goal.type == Goal::Type::Position ? goal.pos : goal.entity.ref()->get<Transform>()->absolute_pos(), ObjectLinkEntryArg<MinionAI, const AI::Result&, &MinionAI::set_path>(id()));
			}
		}
	}

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
	Vec3 pos = get<Transform>()->absolute_pos();
	path = result.path;
	if (path_request != PathRequest::Repath)
	{
		if (path.length > 0)
			goal.pos = path[path.length - 1];
		else
			goal.pos = pos;
	}
	path_request = PathRequest::None;
	path_index = 0;
}

void MinionAI::turn_to(const Vec3& target)
{
	Vec3 forward = Vec3::normalize(target - get<Transform>()->absolute_pos());
	get<Walker>()->target_rotation = atan2(forward.x, forward.z);
}


}