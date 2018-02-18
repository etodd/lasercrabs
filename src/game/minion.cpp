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
#include "drone.h"
#include "BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "data/ragdoll.h"
#include "entities.h"
#include "render/particles.h"
#include "net.h"
#include "team.h"
#include "parkour.h"
#include "data/components.h"
#include "common.h"

#define WALK_SPEED 3.0f
#define PATH_RECALC_TIME 2.1f
#define TARGET_SCAN_TIME 0.5f

namespace VI
{

MinionEntity::MinionEntity(const Vec3& pos, const Quat& quat, AI::Team team, PlayerManager* manager)
{
	Transform* transform = create<Transform>();
	transform->pos = pos;

	Animator* animator = create<Animator>();
	SkinnedModel* model = create<SkinnedModel>();

	animator->armature = Asset::Armature::character;
	animator->layers[0].play(Asset::Animation::character_idle);

	model->shader = Asset::Shader::armature;
	model->mesh = Asset::Mesh::character;
	model->team = s8(team);
	model->color.w = MATERIAL_NO_OVERRIDE;

	create<Audio>();

	create<Health>(MINION_HEALTH, MINION_HEALTH);
	
	Vec3 forward = quat * Vec3(0, 0, 1);

	Walker* walker = create<Walker>(atan2f(forward.x, forward.z));
	walker->max_speed = walker->speed = WALK_SPEED;

	Minion* m = create<Minion>();
	m->owner = manager;

	create<AIAgent>()->team = team;

	create<Target>();

}

void Minion::awake()
{
	link_arg<const TargetEvent&, &Minion::hit_by>(get<Target>()->target_hit);
	link_arg<Entity*, &Minion::killed>(get<Health>()->killed);
	target_timer = 100000.0f; // force target recalculation

	Animator* animator = get<Animator>();
	link<&Minion::footstep>(animator->trigger(Asset::Animation::character_walk, 0.0f));
	link<&Minion::footstep>(animator->trigger(Asset::Animation::character_walk, 0.5f));
	link<&Minion::melee_started>(animator->trigger(Asset::Animation::character_melee, 0.0f));
	link<&Minion::melee_hand_closed>(animator->trigger(Asset::Animation::character_melee, 0.333f));
	link<&Minion::melee_hand_open>(animator->trigger(Asset::Animation::character_melee, 1.875f));
	link<&Minion::melee_thrust>(animator->trigger(Asset::Animation::character_melee, 0.75f));
	link<&Minion::melee_damage>(animator->trigger(Asset::Animation::character_melee, 0.875f));
	link<&Minion::fired>(animator->trigger(Asset::Animation::character_fire, 0.0f));

	get<Walker>()->awake();
	get<Audio>()->offset(get<Walker>()->base_pos() - get<Transform>()->absolute_pos());
}

Minion::~Minion()
{
	if (charging)
	{
		get<Audio>()->stop(AK::EVENTS::STOP_MINION_CHARGE);
		charging = false;
	}
	if (obstacle_id != u32(-1))
	{
		AI::obstacle_remove(obstacle_id);
		obstacle_id = u32(-1);
	}
}

Minion* Minion::closest(AI::TeamMask mask, const Vec3& pos, r32* distance)
{
	Minion* closest = nullptr;
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

s32 Minion::count(AI::TeamMask mask)
{
	s32 result = 0;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->get<AIAgent>()->team, mask))
			result++;
	}
	return result;
}

void Minion::footstep()
{
	get<Audio>()->post(AK::EVENTS::PLAY_MINION_FOOTSTEP);
}

void Minion::melee_started()
{
	get<Audio>()->post(AK::EVENTS::PLAY_MINION_MELEE_PULLBACK);
}

void Minion::melee_hand_closed()
{
	get<Audio>()->post(AK::EVENTS::PLAY_MINION_MELEE_HANDCLOSE);
}

void Minion::melee_hand_open()
{
	get<Audio>()->post(AK::EVENTS::PLAY_MINION_MELEE_HANDOPEN);
}

void Minion::melee_thrust()
{
	get<Audio>()->post(AK::EVENTS::PLAY_MINION_MELEE_THRUST);
}

void Minion::fired()
{
	get<Audio>()->post(AK::EVENTS::PLAY_MINION_WEAPON_FIRE);
	EffectLight::add(hand_pos(), DRONE_RADIUS * 1.5f, 0.1f, EffectLight::Type::MuzzleFlash);
}

void Minion::melee_damage()
{
	AI::Team my_team = get<AIAgent>()->team;
	Vec3 damage_pos = hand_pos();

	b8 did_damage = false;
	for (auto i = Health::list.iterator(); !i.is_last(); i.next())
	{
		Entity* e = i.item()->entity();
		AI::Team team;
		AI::entity_info(e, my_team, &team);
		if (team != my_team)
		{
			Vec3 to_target = e->get<Transform>()->absolute_pos() - damage_pos;
			r32 distance = to_target.length();
			if (distance < MINION_MELEE_RANGE)
			{
				if (e->has<Walker>())
				{
					Vec3 v = Vec3::normalize(to_target);
					v.y = 0.6f;
					e->get<RigidBody>()->btBody->setLinearVelocity(v * 7.0f);
				}

				if (e->has<Parkour>())
				{
					Parkour* parkour = e->get<Parkour>();
					parkour->last_support = e->get<Walker>()->support = nullptr;
					parkour->last_support_time = Game::time.total;
					parkour->wall_run_state = ParkourWallRunState::None;

					if (Game::level.local)
						e->get<Health>()->damage(entity(), 1);

					did_damage = true; // did damage
				}
				else
				{
					if (Game::level.local && e->get<Health>()->can_take_damage(entity()))
						e->get<Health>()->damage(entity(), 1);

					did_damage = true; // did damage
				}
			}
		}
	}

	if (Game::level.local)
	{
		Vec3 forward = get<Walker>()->forward();
		Glass::shatter_all(damage_pos + forward * (WALKER_MINION_RADIUS * -0.5f), damage_pos + forward * (WALKER_MINION_RADIUS * 0.25f));
	}

	// spark effects
	if (did_damage)
	{
		get<Audio>()->post(AK::EVENTS::PLAY_MINION_MELEE_IMPACT);
		Quat rot = Quat::euler(0, get<Walker>()->rotation, 0);
		for (s32 i = 0; i < 50; i++)
		{
			Particles::sparks.add
			(
				damage_pos,
				rot * Vec3(mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo()) * 10.0f,
				Vec4(1, 1, 1, 1)
			);
		}
	}
}

Vec3 Minion::head_pos() const
{
	return get<Target>()->absolute_pos();
}

Vec3 Minion::hand_pos() const
{
	Vec3 p(0, 0, 0);
	get<Animator>()->to_world(Asset::Bone::character_hand_R, &p);
	return p;
}

void minion_model_offset(Mat4* m, r32 rotation, r32 capsule_height)
{
	m->make_transform
	(
		Vec3(0, capsule_height * -0.5f - WALKER_SUPPORT_HEIGHT, 0),
		Vec3(1.0f, 1.0f, 1.0f),
		Quat::euler(0, rotation + PI * 0.5f, 0)
	);
}

r32 minion_angle_to(const Minion* m, const Vec3& target)
{
	Vec3 forward = Vec3::normalize(target - m->get<Transform>()->absolute_pos());
	return atan2f(forward.x, forward.z);;
}

Vec3 Minion::aim_pos(r32 rotation) const
{
	Mat4 mat;
	get<Transform>()->mat(&mat);

	Mat4 offset;
	minion_model_offset(&offset, rotation, get<Walker>()->capsule_height());

	mat = offset * mat;
	return (mat * Vec4(-0.188f, 1.600f, -0.516f, 1.0f)).xyz();
}

b8 Minion::headshot_test(const Vec3& ray_start, const Vec3& ray_end)
{
	return LMath::ray_sphere_intersect(ray_start, ray_end, head_pos(), MINION_HEAD_RADIUS);
}

r32 entity_cost(const Minion* me, const Vec3& pos, AI::Team team, const Entity* target)
{
	Vec3 target_pos = target->get<Transform>()->absolute_pos();

	if (!target->has<ForceField>())
	{
		ForceField* target_force_field = ForceField::inside(~(1 << team), target_pos);
		if (target_force_field && (target_force_field->flags & ForceField::FlagInvincible))
			return FLT_MAX; // can't do it
	}

	if (target == me->carrying.ref())
		return FLT_MAX; // can't do it

	Vec3 to_target = target_pos - pos;
	r32 total_distance;

	const AI::PathZone* path_zone = AI::PathZone::get(pos, target);
	if (path_zone) // must pass through choke point before going to target
		total_distance = (path_zone->choke_point - pos).length() + (target_pos - path_zone->choke_point - pos).length();
	else
		total_distance = to_target.length();

	return total_distance;
}

Entity* closest_target(Minion* me, AI::Team team)
{
	Entity* best = nullptr;
	r32 best_cost = FLT_MAX;
	Vec3 pos = me->get<Transform>()->absolute_pos();

	for (auto i = Battery::list.iterator(); !i.is_last(); i.next())
	{
		Battery* item = i.item();
		if (item->team != team && item->team != AI::TeamNone)
		{
			Vec3 item_pos = item->get<Transform>()->absolute_pos();
			if (me->can_see(item->entity()))
				return item->entity();
			r32 cost = entity_cost(me, pos, team, item->entity());
			if (cost < best_cost)
			{
				best = item->entity();
				best_cost = cost;
			}
		}
	}

	if (best)
		return best;

	for (auto i = ForceField::list.iterator(); !i.is_last(); i.next())
	{
		ForceField* item = i.item();
		if (item->team != team && !(item->flags & ForceField::FlagInvincible))
		{
			Vec3 item_pos = item->get<Transform>()->absolute_pos();
			if (me->can_see(item->entity()))
				return item->entity();
			r32 cost = entity_cost(me, pos, team, item->entity());
			if (cost < best_cost)
			{
				best = item->entity();
				best_cost = cost;
			}
		}
	}

	for (auto i = Turret::list.iterator(); !i.is_last(); i.next())
	{
		Turret* item = i.item();
		if (item->team != team)
		{
			if (me->can_see(item->entity(), false, false))
				return item->entity();
			r32 cost = entity_cost(me, pos, team, item->entity());
			if (cost < best_cost)
			{
				best = item->entity();
				best_cost = cost;
			}
		}
	}

	for (auto i = MinionSpawner::list.iterator(); !i.is_last(); i.next())
	{
		MinionSpawner* item = i.item();
		if (item->team != team)
		{
			Vec3 item_pos = item->get<Transform>()->absolute_pos();
			if (me->can_see(item->entity()))
				return item->entity();
			r32 cost = entity_cost(me, pos, team, item->entity());
			if (cost < best_cost)
			{
				best = item->entity();
				best_cost = cost;
			}
		}
	}

	for (auto i = Minion::list.iterator(); !i.is_last(); i.next())
	{
		Minion* item = i.item();
		if (item->get<AIAgent>()->team != team)
		{
			Vec3 item_pos = item->get<Transform>()->absolute_pos();
			if (me->can_see(item->entity()))
				return item->entity();
			r32 cost = entity_cost(me, pos, team, item->entity());
			if (cost < best_cost)
			{
				best = item->entity();
				best_cost = cost;
			}
		}
	}

	for (auto i = Grenade::list.iterator(); !i.is_last(); i.next())
	{
		Grenade* item = i.item();
		if (item->team != team)
		{
			Vec3 item_pos = item->get<Transform>()->absolute_pos();
			if (me->can_see(item->entity()))
				return item->entity();
			r32 cost = entity_cost(me, pos, team, item->entity());
			if (cost < best_cost)
			{
				best = item->entity();
				best_cost = cost;
			}
		}
	}

	for (auto i = Rectifier::list.iterator(); !i.is_last(); i.next())
	{
		Rectifier* item = i.item();
		if (item->team != team && !item->has<Battery>())
		{
			Vec3 item_pos = item->get<Transform>()->absolute_pos();
			if (me->can_see(item->entity()))
				return item->entity();
			r32 cost = entity_cost(me, pos, team, item->entity());
			if (cost < best_cost)
			{
				best = item->entity();
				best_cost = cost;
			}
		}
	}

	for (auto i = PlayerCommon::list.iterator(); !i.is_last(); i.next())
	{
		PlayerCommon* item = i.item();
		if (item->get<AIAgent>()->team != team)
		{
			Vec3 item_pos = item->get<Transform>()->absolute_pos();
			if (me->can_see(item->entity()))
				return item->entity();
			r32 cost = entity_cost(me, pos, team, item->entity());
			if (cost < best_cost)
			{
				best = item->entity();
				best_cost = cost;
			}
		}
	}

	return best;
}

ForceField* force_field_between_me_and_target(Minion* me)
{
	if (Entity* goal = me->goal.entity.ref())
	{
		AI::Team team = me->get<AIAgent>()->team;
		// check if there is a force field between us and our goal
		Vec3 pos = me->get<Walker>()->base_pos();
		if (goal->has<ForceField>())
		{
			if (ForceField* field = ForceField::inside(~(1 << team), pos))
			{
				if (!(field->flags & ForceField::FlagInvincible))
					return field;
			}
		}
		else
		{
			Vec3 target_pos = goal->get<Target>()->absolute_pos();
			if (ForceField::hash(team, pos) != ForceField::hash(team, target_pos))
			{
				if (ForceField* field = ForceField::inside(~(1 << team), pos))
				{
					if (!(field->flags & ForceField::FlagInvincible))
						return field;
				}
				else if (ForceField* field = ForceField::inside(~(1 << team), target_pos))
				{
					if (!(field->flags & ForceField::FlagInvincible)
						&& me->can_see(field->entity()))
						return field;
				}
			}
		}
	}
	return nullptr;
}

Entity* visible_target(Minion* me, AI::Team team)
{
	for (auto i = PlayerCommon::list.iterator(); !i.is_last(); i.next())
	{
		PlayerCommon* player = i.item();
		if (player->get<AIAgent>()->team != team)
		{
			if (me->can_see(player->entity(), true))
				return player->entity();
		}
	}

	if (ForceField* field = force_field_between_me_and_target(me))
		return field->entity();

	for (auto i = Turret::list.iterator(); !i.is_last(); i.next())
	{
		Turret* turret = i.item();
		if (turret->team != team && me->can_see(turret->entity()))
			return turret->entity();
	}

	for (auto i = Minion::list.iterator(); !i.is_last(); i.next())
	{
		Minion* minion = i.item();
		if (minion->get<AIAgent>()->team != team && me->can_see(minion->entity()))
			return minion->entity();
	}

	for (auto i = MinionSpawner::list.iterator(); !i.is_last(); i.next())
	{
		MinionSpawner* item = i.item();
		if (item->team != team && me->can_see(item->entity()))
			return item->entity();
	}

	for (auto i = Grenade::list.iterator(); !i.is_last(); i.next())
	{
		Grenade* grenade = i.item();
		if (grenade->team != team && me->can_see(grenade->entity()))
			return grenade->entity();
	}

	for (auto i = ForceField::list.iterator(); !i.is_last(); i.next())
	{
		ForceField* field = i.item();
		if (field->team != team
			&& !(field->flags & ForceField::FlagInvincible)
			&& me->can_see(field->entity()))
			return field->entity();
	}

	for (auto i = Battery::list.iterator(); !i.is_last(); i.next())
	{
		Battery* battery = i.item();
		if (battery->team != team && battery->team != AI::TeamNone && me->can_see(battery->entity()))
			return battery->entity();
	}

	for (auto i = Rectifier::list.iterator(); !i.is_last(); i.next())
	{
		Rectifier* rectifier = i.item();
		if (rectifier->team != team && !rectifier->has<Battery>() && me->can_see(rectifier->entity()))
			return rectifier->entity();
	}

	return nullptr;
}

// the position we need to path toward in order to hit the goal
Vec3 Minion::goal_path_position(const Goal& g, const Vec3& minion_pos)
{
	if (g.type == Goal::Type::Target)
	{
		Entity* e = g.entity.ref();
		vi_assert(e);
		if (e->has<MinionTarget>())
		{
			MinionTarget* t = e->get<MinionTarget>();
			if (t->ingress_points.length > 0)
			{
				r32 closest_distance_sq = FLT_MAX;
				Vec3 closest_point;
				for (s32 i = 0; i < t->ingress_points.length; i++)
				{
					const Vec3& pos = t->ingress_points[i];
					r32 distance_sq = (pos - minion_pos).length_squared();
					if (distance_sq < closest_distance_sq)
					{
						closest_distance_sq = distance_sq;
						closest_point = pos;
					}
				}
				return closest_point;
			}
			else if (t->has<Turret>())
				return e->get<Transform>()->absolute_pos() + Vec3(0, -1.5f, 0);
			else if (t->has<Battery>())
				return e->get<Battery>()->spawn_point.ref()->get<Transform>()->absolute_pos() + Vec3(0, DRONE_RADIUS, 0);
			else
				return e->get<Transform>()->absolute_pos();
		}
	}
	return g.pos; // last known position of the target
}

void Minion::update_server(const Update& u)
{
	// AI
	target_timer += u.time.delta;

	b8 can_attack = false;
	if (attack_timer > 0.0f)
	{
		attack_timer = vi_max(0.0f, attack_timer - u.time.delta);
		can_attack = attack_timer == 0.0f;
	}

	Vec3 pos = get<Walker>()->base_pos();

	if (path_request == PathRequest::None)
	{
		target_scan_timer = vi_max(0.0f, target_scan_timer - u.time.delta);
		if (target_scan_timer == 0.0f)
		{
			target_scan_timer = TARGET_SCAN_TIME;

			Entity* target_candidate = visible_target(this, get<AIAgent>()->team);
			if (target_candidate != goal.entity.ref()
				&& (target_candidate || path_index >= path.length))
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
				if (path.length == 0 || (path[path.length - 1] - pos).length_squared() < 1.5f * 1.5f)
				{
					if (target_timer > 2.0f)
						new_goal();
				}
				else
				{
					if (recalc)
					{
						// recalc path
						path_request = PathRequest::Repath;
						AI::pathfind(get<AIAgent>()->team, pos, goal_path_position(goal, pos), ObjectLinkEntryArg<Minion, const AI::Result&, &Minion::set_path>(id()));
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
						AI::Team target_team;
						AI::entity_info(g, get<AIAgent>()->team, &target_team);
						if (target_team == AI::TeamNone || target_team == get<AIAgent>()->team) // it's friendly, find a new goal
							new_goal();
						else
						{
							// turn to and attack the target
							Vec3 hand_pos = aim_pos(get<Walker>()->rotation);
							Vec3 aim_pos;
							if (!g->get<Target>()->predict_intersection(hand_pos, BOLT_SPEED_MINION, nullptr, &aim_pos))
								aim_pos = g->get<Target>()->absolute_pos();
							turn_to(aim_pos);

							Animator::Layer* anim_layer = &get<Animator>()->layers[0];

							if (target_timer > MINION_ATTACK_TIME * 0.25f // give some reaction time
								&& anim_layer->animation != Asset::Animation::character_melee
								&& Team::match_state == Team::MatchState::Active)
							{
								if ((aim_pos - hand_pos).length_squared() < MINION_MELEE_RANGE * MINION_MELEE_RANGE)
								{
									path.length = 0;
									anim_layer->speed = 1.0f;
									anim_layer->behavior = Animator::Behavior::Default;
									anim_layer->play(Asset::Animation::character_melee);
									attack_timer = 0.0f;
								}
								else if (!has_grenade())
								{
									if (can_attack
										&& fabsf(LMath::angle_to(get<Walker>()->target_rotation, get<Walker>()->rotation)) < PI * 0.05f) // make sure we're looking at the target
										fire(aim_pos);
									else if (attack_timer == 0.0f)
										attack_timer = MINION_ATTACK_TIME;
								}
							}
						}
					}
					else
					{
						if (recalc)
						{
							Vec3 goal_pos = goal_path_position(goal, pos);
							if (ForceField::hash(get<AIAgent>()->team, pos, ForceField::HashMode::OnlyInvincible) == ForceField::hash(get<AIAgent>()->team, goal_pos, ForceField::HashMode::OnlyInvincible))
							{
								// recalc path
								path_request = PathRequest::Target;
								AI::pathfind(get<AIAgent>()->team, pos, goal_pos, ObjectLinkEntryArg<Minion, const AI::Result&, &Minion::set_path>(id()));
							}
							else // won't be able to reach the goal; find a new one
								new_goal();
						}
					}
				}
				else
					new_goal();
				break;
			}
			default:
				vi_assert(false);
				break;
		}
	}

	// path following

	{
		const Animator::Layer& layer = get<Animator>()->layers[0];
		if (path_index < path.length
			&& layer.animation != Asset::Animation::character_fire
			&& layer.animation != Asset::Animation::character_aim
			&& layer.animation != Asset::Animation::character_melee)
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

	// dynamic obstacle
	{
		Vec3 pos = get<Walker>()->base_pos();
		if (obstacle_id != u32(-1) && (!goal.entity.ref() || get<Walker>()->dir.length_squared() > 0.0f || (obstacle_pos - pos).length_squared() > WALKER_MINION_RADIUS))
		{
			// moving
			AI::obstacle_remove(obstacle_id);
			obstacle_id = u32(-1);
			get<RigidBody>()->set_collision_masks(CollisionMinionMoving, get<RigidBody>()->collision_filter);
		}
		else if (obstacle_id == u32(-1) && get<Walker>()->dir.length_squared() == 0.0f && get<Walker>()->support.ref())
		{
			// standing still
			obstacle_pos = pos;
			obstacle_id = AI::obstacle_add(pos, WALKER_MINION_RADIUS, get<Walker>()->capsule_height() + WALKER_SUPPORT_HEIGHT);
			get<RigidBody>()->set_collision_masks(CollisionWalker, get<RigidBody>()->collision_filter);
		}
	}

	// update animation
	{
		Animator::Layer* layer = &get<Animator>()->layers[0];

		if (layer->animation != Asset::Animation::character_fire
			&& layer->animation != Asset::Animation::character_melee)
		{
			layer->behavior = Animator::Behavior::Loop;
			if (attack_timer > 0.0f)
			{
				layer->speed = 1.0f;
				layer->play(Asset::Animation::character_aim);
			}
			else if (get<Walker>()->support.ref() && get<Walker>()->dir.length_squared() > 0.0f)
			{
				r32 net_speed = vi_max(get<Walker>()->net_speed, WALK_SPEED * 0.5f);
				layer->speed = (net_speed / get<Walker>()->speed) * 1.2f;
				layer->play(Asset::Animation::character_walk);
			}
			else
			{
				layer->speed = 1.0f;
				layer->play(Asset::Animation::character_idle);
			}
		}
	}
}

void Minion::fire(const Vec3& target)
{
	vi_assert(Game::level.local);
	Vec3 hand = aim_pos(get<Walker>()->rotation);
	Net::finalize(World::create<BoltEntity>(get<AIAgent>()->team, owner.ref(), entity(), Bolt::Type::Minion, hand, target - hand));

	Animator::Layer* layer = &get<Animator>()->layers[0];
	layer->speed = 1.0f;
	layer->behavior = Animator::Behavior::Default;
	layer->play(Asset::Animation::character_fire);
}

r32 Minion::particle_accumulator;
void Minion::update_all(const Update& u)
{
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		Minion* m = i.item();

		if (Game::level.local)
			m->update_server(u);

		minion_model_offset(&m->get<SkinnedModel>()->offset, m->get<Walker>()->rotation, m->get<Walker>()->capsule_height());

		// update head position
		{
			m->get<Target>()->local_offset = Vec3(0.1f, 0, 0);
			m->get<Animator>()->to_local(Asset::Bone::character_head, &m->get<Target>()->local_offset);
		}
	}

#if !SERVER
	const r32 interval = 0.02f;
	particle_accumulator += u.time.delta;
	while (particle_accumulator > interval)
	{
		particle_accumulator -= interval;
		for (auto i = list.iterator(); !i.is_last(); i.next())
		{
			const Animator::Layer& layer = i.item()->get<Animator>()->layers[0];
			b8 charging_now = layer.animation == Asset::Animation::character_aim;
			if (charging_now)
			{
				Vec3 pos = i.item()->hand_pos();

				// spawn particle effect
				Vec3 offset = Quat::euler(0.0f, mersenne::randf_co() * PI * 2.0f, (mersenne::randf_co() - 0.5f) * PI) * Vec3(0, 0, 1.0f);
				Particles::fast_tracers.add
				(
					pos + offset,
					offset * -3.5f,
					0
				);
			}

			if (i.item()->charging != charging_now)
			{
				if (charging_now)
					i.item()->get<Audio>()->post(AK::EVENTS::PLAY_MINION_CHARGE);
				else
					i.item()->get<Audio>()->stop(AK::EVENTS::STOP_MINION_CHARGE);
				i.item()->charging = charging_now;
			}
		}
	}
#endif
}

void Minion::hit_by(const TargetEvent& e)
{
	get<Health>()->damage(e.hit_by, get<Health>()->hp_max);
}

void Minion::killed(Entity* killer)
{
	PlayerManager::entity_killed_by(entity(), killer);
	get<Audio>()->stop_all();
	get<Audio>()->post_unattached(killer && killer->has<Drone>() ? AK::EVENTS::PLAY_MINION_HEADSHOT : AK::EVENTS::PLAY_MINION_DIE, head_pos() - get<Transform>()->absolute_pos());

	if (Game::level.local)
	{
		World::remove_deferred(entity());
		Ragdoll::add(entity(), killer);
	}
}

b8 minion_vision_check(AI::Team team, const Vec3& start, const Vec3& end, Entity* target, b8 check_force_fields)
{
	btCollisionWorld::ClosestRayResultCallback ray_callback(start, end);
	Physics::raycast(&ray_callback, (CollisionStatic | CollisionInaccessible | CollisionElectric | CollisionParkour | (check_force_fields ? CollisionAllTeamsForceField : 0)) & ~Team::force_field_mask(team));
	if (ray_callback.hasHit())
	{
		Entity* hit = &Entity::list[ray_callback.m_collisionObject->getUserIndex()];
		if (hit->has<ForceFieldCollision>())
			hit = hit->get<ForceFieldCollision>()->field.ref()->entity();
		return target == hit;
	}
	else
		return true;
}

b8 Minion::can_see(Entity* target, b8 limit_vision_cone, b8 check_force_fields) const
{
	if ((target->has<AIAgent>() && target->get<AIAgent>()->stealth == 1.0f)
		|| (target->has<Drone>() && target->get<Drone>()->state() != Drone::State::Crawl)
		|| target == carrying.ref())
		return false;

	Vec3 target_pos = target->get<Target>()->absolute_pos();

	Vec3 hand = aim_pos(minion_angle_to(this, target_pos));

	{
		ForceField::HashMode hash_mode = check_force_fields ? ForceField::HashMode::All : ForceField::HashMode::OnlyInvincible;
		if (!target->has<ForceField>() && ForceField::hash(get<AIAgent>()->team, hand, hash_mode) != ForceField::hash(get<AIAgent>()->team, target_pos, hash_mode))
			return false;
	}

	Vec3 head = head_pos();

	Vec3 diff = target_pos - head;
	r32 distance = diff.length() + (target->has<ForceField>() ? -FORCE_FIELD_RADIUS : 0);

	if (limit_vision_cone && target->has<Parkour>() && distance < MINION_HEARING_RANGE)
		limit_vision_cone = false;

	return distance < MINION_VISION_RANGE
		&& (!limit_vision_cone || Vec3::normalize(diff).dot(get<Walker>()->forward()) > 0.707f)
		&& (!target->has<Parkour>() || fabsf(diff.y) < MINION_HEARING_RANGE)
		&& minion_vision_check(get<AIAgent>()->team, head, target_pos, target, check_force_fields)
		&& minion_vision_check(get<AIAgent>()->team, hand, target_pos, target, check_force_fields)
		&& (!has_grenade() || (aim_pos(get<Walker>()->rotation) - target_pos).length_squared() < MINION_MELEE_RANGE * MINION_MELEE_RANGE); // can only melee if holding a grenade
}

void Minion::new_goal()
{
	Entity* target = closest_target(this, get<AIAgent>()->team);
	if (target != goal.entity.ref() || (!target && !goal.entity.ref()))
	{
		goal.entity = target;
		auto path_callback = ObjectLinkEntryArg<Minion, const AI::Result&, &Minion::set_path>(id());
		if (target)
		{
			goal.type = Goal::Type::Target;
			if (!can_see(goal.entity.ref()))
			{
				path_request = PathRequest::Target;
				goal.pos = goal.entity.ref()->get<Transform>()->absolute_pos();
				Vec3 pos = get<Walker>()->base_pos();
				AI::pathfind(get<AIAgent>()->team, pos, goal_path_position(goal, pos), path_callback);
			}
		}
		else
		{
			goal.type = Goal::Type::Position;
			path_request = PathRequest::Random;
			Vec3 pos = get<Walker>()->base_pos();
			AI::random_path(pos, pos, get<AIAgent>()->team, MINION_VISION_RANGE, path_callback);
		}
		target_timer = 0.0f;
		path_timer = PATH_RECALC_TIME;
	}
}

Vec3 goal_pos(const Minion::Goal& g)
{
	if (g.type == Minion::Goal::Type::Position || !g.entity.ref())
		return g.pos;
	else
		return g.entity.ref()->get<Transform>()->absolute_pos();
}

void Minion::set_path(const AI::Result& result)
{
	get<Minion>()->attack_timer = 0.0f; // we're no longer attacking

	path_request = PathRequest::None;
	path = result.path;
	path_index = 0;
	if (path.length > 1)
	{
		// sometimes the system returns a few extra path points at the beginning, which actually puts us farther from the goal
		// if we're close enough to the second path point, then skip that first one.
		for (s32 i = 1; i < vi_min(s32(path.length), 3); i++)
		{
			if ((path[i] - get<Walker>()->base_pos()).length_squared() < 0.3f * 0.3f)
				path_index = i;
		}
	}
	else if (path.length == 0 && goal.type == Goal::Type::Target)
		goal.entity = nullptr; // can't path there

	if (path_request != PathRequest::Repath)
	{
		if (path.length > 0)
			goal.pos = path[path.length - 1];
		else
			goal.pos = get<Transform>()->absolute_pos();
	}
}

void Minion::turn_to(const Vec3& target)
{
	get<Walker>()->target_rotation = minion_angle_to(this, target);
}

b8 Minion::has_grenade() const
{
	Entity* c = carrying.ref();
	return c && c->has<Grenade>();
}


}
