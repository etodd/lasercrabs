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
#include "net.h"
#include "team.h"
#include "parkour.h"

#define WALK_SPEED 2.5f
#define ROTATION_SPEED 4.0f
#define MINION_HEARING_RANGE 7.0f
#define MINION_VISION_RANGE 20.0f
#define HEALTH 3
#define PATH_RECALC_TIME 1.0f
#define TARGET_SCAN_TIME 0.5f

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
	model->team = (s8)team;
	model->color.w = MATERIAL_NO_OVERRIDE;

	create<Audio>();

	create<Health>(HEALTH, HEALTH);
	
	Vec3 forward = quat * Vec3(0, 0, 1);

	Walker* walker = create<Walker>(atan2f(forward.x, forward.z));
	walker->max_speed = walker->speed = WALK_SPEED;
	walker->rotation_speed = ROTATION_SPEED;

	create<MinionCommon>()->owner = manager;

	create<AIAgent>()->team = team;

	create<Target>();

	create<MinionAI>()->patrol_point = pos;
}

void MinionCommon::awake()
{
	link_arg<const TargetEvent&, &MinionCommon::hit_by>(get<Target>()->target_hit);
	link_arg<Entity*, &MinionCommon::killed>(get<Health>()->killed);

	Animator* animator = get<Animator>();
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
void MinionCommon::update_server(const Update& u)
{
	attack_timer = vi_max(0.0f, attack_timer - u.time.delta);

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

void MinionCommon::update_client(const Update& u)
{
	get<SkinnedModel>()->offset.make_transform
	(
		Vec3(0, get<Walker>()->capsule_height() * -0.5f - get<Walker>()->support_height, 0),
		Vec3(1.0f, 1.0f, 1.0f),
		Quat::euler(0, get<Walker>()->rotation + PI * 0.5f, 0)
	);

	// update head position
	{
		get<Target>()->local_offset = Vec3(0.1f, 0, 0);
		Quat rot = Quat::identity;
		get<Animator>()->to_local(Asset::Bone::character_head, &get<Target>()->local_offset, &rot);
	}
}

void MinionCommon::hit_by(const TargetEvent& e)
{
	get<Health>()->damage(e.hit_by, get<Health>()->hp_max);
}

void MinionCommon::killed(Entity* killer)
{
	get<Audio>()->post_event(AK::EVENTS::STOP);
	Audio::post_global_event(AK::EVENTS::PLAY_HEADSHOT, head_pos());

	if (Game::level.local)
	{
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

		World::remove_deferred(entity());

		Ragdoll* r = ragdoll->add<Ragdoll>();

		if (killer)
		{
			if (killer->has<Awk>())
				r->apply_impulse(Ragdoll::Impulse::Head, killer->get<Awk>()->velocity * 0.1f);
			else
			{
				Vec3 killer_to_us = get<Transform>()->absolute_pos() - killer->get<Transform>()->absolute_pos();
				killer_to_us.normalize();
				r->apply_impulse(killer->has<Parkour>() ? Ragdoll::Impulse::Feet : Ragdoll::Impulse::Head, killer_to_us * 10.0f);
			}
		}

		Net::finalize(ragdoll);
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
			Vec3 item_pos = field->get<Transform>()->absolute_pos();
			if ((item_pos - me->patrol_point).length_squared() > MINION_VISION_RANGE * MINION_VISION_RANGE)
				continue;
			if (me->can_see(field->entity()))
				return field->entity();
			Vec3 to_field = item_pos - pos;
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
		if (sensor->team != team && !sensor->has<EnergyPickup>())
		{
			Vec3 item_pos = sensor->get<Transform>()->absolute_pos();
			if ((item_pos - me->patrol_point).length_squared() > MINION_VISION_RANGE * MINION_VISION_RANGE)
				continue;
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
			Vec3 item_pos = minion->get<Transform>()->absolute_pos();
			if ((item_pos - me->patrol_point).length_squared() > MINION_VISION_RANGE * MINION_VISION_RANGE)
				continue;
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
		if (rocket->get<Transform>()->parent.ref() && rocket->team() != team)
		{
			Vec3 item_pos = rocket->get<Transform>()->absolute_pos();
			if ((item_pos - me->patrol_point).length_squared() > MINION_VISION_RANGE * MINION_VISION_RANGE)
				continue;
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

	for (auto i = Grenade::list.iterator(); !i.is_last(); i.next())
	{
		Grenade* grenade = i.item();
		if (grenade->team() != team)
		{
			Vec3 item_pos = grenade->get<Transform>()->absolute_pos();
			if ((item_pos - me->patrol_point).length_squared() > MINION_VISION_RANGE * MINION_VISION_RANGE)
				continue;
			if (me->can_see(grenade->entity()))
				return grenade->entity();
			Vec3 to_grenade = grenade->get<Transform>()->absolute_pos() - pos;
			r32 total_distance = to_grenade.length_squared() + (to_grenade.dot(direction) < 0.0f ? direction_cost : 0.0f);
			if (total_distance < closest_distance)
			{
				closest = grenade->entity();
				closest_distance = total_distance;
			}
		}
	}

	return closest;
}

Entity* visible_target(MinionAI* me, AI::Team team)
{
	for (auto i = Decoy::list.iterator(); !i.is_last(); i.next())
	{
		Decoy* awk = i.item();
		if (awk->get<AIAgent>()->team != team)
		{
			if (me->can_see(awk->entity(), true))
				return awk->entity();
		}
	}

	for (auto i = PlayerCommon::list.iterator(); !i.is_last(); i.next())
	{
		PlayerCommon* player = i.item();
		if (player->get<AIAgent>()->team != team)
		{
			if (me->can_see(player->entity(), true))
				return player->entity();
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

	for (auto i = Grenade::list.iterator(); !i.is_last(); i.next())
	{
		Grenade* grenade = i.item();
		if (grenade->team() != team)
		{
			if (me->can_see(grenade->entity()))
				return grenade->entity();
		}
	}

	for (auto i = Sensor::list.iterator(); !i.is_last(); i.next())
	{
		Sensor* sensor = i.item();
		if (sensor->team != team && !sensor->has<EnergyPickup>())
		{
			if (me->can_see(sensor->entity()))
				return sensor->entity();
		}
	}

	for (auto i = Rocket::list.iterator(); !i.is_last(); i.next())
	{
		Rocket* rocket = i.item();
		if (rocket->get<Transform>()->parent.ref() && rocket->team() != team)
		{
			if (me->can_see(rocket->entity()))
				return rocket->entity();
		}
	}

	return nullptr;
}

void MinionAI::awake()
{
	path_request = PathRequest::PointQuery;
	auto callback = ObjectLinkEntryArg<MinionAI, const Vec3&, &MinionAI::set_patrol_point>(id());
	AI::closest_walk_point(get<Transform>()->absolute_pos(), callback);
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

	// if we're targeting a decoy, always go for the decoy
	if (target->has<Decoy>())
		limit_vision_cone = false;

	if (distance_squared < MINION_VISION_RANGE * MINION_VISION_RANGE)
	{
		diff.normalize();
		if (!limit_vision_cone || diff.dot(get<Walker>()->forward()) > 0.707f)
		{
			btCollisionWorld::ClosestRayResultCallback ray_callback(pos, target_pos);
			Physics::raycast(&ray_callback, (CollisionStatic | CollisionInaccessible | CollisionAllTeamsContainmentField) & ~Team::containment_field_mask(get<AIAgent>()->team));
			if (!ray_callback.hasHit())
				return true;
		}
	}
	return false;
}

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
		path_request = PathRequest::Random;
		AI::random_path(pos, patrol_point, MINION_VISION_RANGE, path_callback);
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

void MinionAI::update(const Update& u)
{
	target_timer += u.time.delta;

	Vec3 pos = get<Transform>()->absolute_pos();

	if (path_request == PathRequest::None)
	{
		target_scan_timer = vi_max(0.0f, target_scan_timer - u.time.delta);
		if (target_scan_timer == 0.0f)
		{
			target_scan_timer = TARGET_SCAN_TIME;

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

		b8 recalc = false;
		path_timer = vi_max(0.0f, path_timer - u.time.delta);
		if (path_timer == 0.0f)
		{
			path_timer = PATH_RECALC_TIME;
			recalc = true;
		}

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
						path_request = PathRequest::Repath;
						AI::pathfind(pos, goal.pos, ObjectLinkEntryArg<MinionAI, const AI::Result&, &MinionAI::set_path>(id()));
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
						if (!g->has<Target>() || !g->get<Target>()->predict_intersection(head_pos, PROJECTILE_SPEED, nullptr, &aim_pos))
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
							Net::finalize(World::create<ProjectileEntity>(get<MinionCommon>()->owner.ref(), head_pos, aim_pos - head_pos));
							get<MinionCommon>()->attack_timer = MINION_ATTACK_TIME;
						}
					}
					else
					{
						if (recalc)
						{
							// recalc path
							path_request = PathRequest::Target;
							Vec3 goal_pos = g->get<Transform>()->absolute_pos();
							if ((goal_pos - patrol_point).length_squared() < MINION_VISION_RANGE * MINION_VISION_RANGE) // still in range; follow them
								AI::pathfind(pos, goal_pos, ObjectLinkEntryArg<MinionAI, const AI::Result&, &MinionAI::set_path>(id()));
							else // out of range
								new_goal();
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

void MinionAI::set_patrol_point(const Vec3& p)
{
	patrol_point = p;
	new_goal(get<Walker>()->forward());
}

void MinionAI::set_path(const AI::Result& result)
{
	for (s32 i = 0; i < result.path.length; i++)
	{
		if ((result.path[i] - patrol_point).length_squared() > MINION_VISION_RANGE * MINION_VISION_RANGE) // entire path must be in range
		{
			new_goal();
			return;
		}
	}

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