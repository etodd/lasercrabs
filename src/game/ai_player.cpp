#include "ai_player.h"
#include "mersenne/mersenne-twister.h"
#include "entities.h"
#include "console.h"
#include "drone.h"
#include "minion.h"
#include "bullet/src/BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "minion.h"
#include "noise.h"
#if DEBUG_AI_CONTROL
#include "render/views.h"
#endif
#include "net.h"
#include "team.h"
#include "player.h"
#include "net_serialize.h" // for popcount

namespace VI
{


PinArray<PlayerAI, MAX_PLAYERS> PlayerAI::list;

AI::Config PlayerAI::generate_config(AI::Team team, r32 spawn_time)
{
	AI::Config config;

	config.team = team;
	config.spawn_time = spawn_time;

	config.interval_memory_update = 0.2f;
	config.interval_low_level = 0.25f;
	config.interval_high_level = 0.5f;
	config.inaccuracy_min = PI * 0.001f;
	config.inaccuracy_range = PI * 0.01f;
	config.aim_min_delay = 0.5f;
	config.aim_timeout = 2.0f;
	config.aim_speed = 3.0f;
	config.dodge_chance = 0.1f;

	return config;
}

PlayerAI::PlayerAI(PlayerManager* m, const AI::Config& config)
	: manager(m),
	revision(),
	memory(),
	config(config),
	spawning()
{
	m->can_spawn = true;
	m->spawn.link<PlayerAI, const PlayerSpawnPosition&, &PlayerAI::spawn>(this);
}

void PlayerAI::update(const Update& u)
{
	if (Team::game_over
		&& Game::real_time.total - Team::game_over_real_time > SCORE_SUMMARY_DELAY + SCORE_SUMMARY_ACCEPT_DELAY
		&& mersenne::randf_co() < u.time.delta / 6.0f)
	{
		// randomly disconnect
		World::remove(manager.ref()->entity());
		PlayerAI::list.remove(id());
	}
}

void ai_player_spawn(const Vec3& pos, const Quat& rot, PlayerAI* player)
{
	// HACK: clear links so we can respawn the entity and have room for more links
	player->manager.ref()->control_point_capture_completed.entries.length = 0;
	player->manager.ref()->upgrade_completed.entries.length = 0;

	Entity* e = World::create<DroneEntity>(player->manager.ref()->team.ref()->team());

	e->get<Transform>()->absolute(pos, rot);
	e->get<Drone>()->velocity = rot * Vec3(0, 0, DRONE_FLY_SPEED);

	e->add<PlayerCommon>(player->manager.ref());

	player->manager.ref()->instance = e;

	e->add<PlayerControlAI>(player);
	Net::finalize(e);
}

void PlayerAI::spawn_callback(const AI::DronePathNode& node)
{
	spawning = false;
	ai_player_spawn(node.pos, Quat::look(-node.normal), this);
}

void PlayerAI::spawn(const PlayerSpawnPosition& spawn_pos)
{
	if (!spawning)
	{
		AI::TeamMask team_mask = 1 << manager.ref()->team.ref()->team();
		if (config.spawn_time == 0.0f && Game::session.type == SessionType::Story && Battery::count(team_mask) > 0)
		{
			// player has been here for a while; pick a random spawn point near a pickup we own

			Array<Ref<Battery>> pickups;
			for (auto i = Battery::list.iterator(); !i.is_last(); i.next())
			{
				if (AI::match(i.item()->team, team_mask))
					pickups.add(i.item());
			}

			spawning = true;
			AI::drone_closest_point
			(
				pickups[mersenne::rand() % pickups.length].ref()->get<Transform>()->absolute_pos(),
				manager.ref()->team.ref()->team(),
				ObjectLinkEntryArg<PlayerAI, const AI::DronePathNode&, &PlayerAI::spawn_callback>(id())
			);
		}
		else // normal spawn at spawn point
			ai_player_spawn(spawn_pos.pos, Quat::look(Vec3(0, -1, 0)), this);
	}
}

PlayerControlAI::PlayerControlAI(PlayerAI* p)
	: path_index(),
	player(p),
	path(),
	target(),
	shot_at_target(),
	hit_target(),
	aim_timer(),
	aim_timeout(),
	inaccuracy(),
	current(),
	action_queue_key(),
	action_queue(&action_queue_key),
	random_look(0, 0, 1)
{
#if DEBUG_AI_CONTROL
	camera = Camera::add(0);
#endif
}

void PlayerControlAI::awake()
{
#if DEBUG_AI_CONTROL
	camera->flag(CameraFlagFog, false);
	camera->team = s8(get<AIAgent>()->team);
	camera->mask = 1 << camera->team;
	camera->range = DRONE_MAX_DISTANCE;
#endif
	link<&PlayerControlAI::drone_done_flying_or_dashing>(get<Drone>()->done_flying);
	link<&PlayerControlAI::drone_done_flying_or_dashing>(get<Drone>()->done_dashing);
	link_arg<Entity*, &PlayerControlAI::drone_hit>(get<Drone>()->hit);
	link<&PlayerControlAI::drone_detaching>(get<Drone>()->detaching);
	link<&PlayerControlAI::drone_detaching>(get<Drone>()->dashing);
	link_arg<b8, &PlayerControlAI::control_point_capture_completed>(get<PlayerCommon>()->manager.ref()->control_point_capture_completed);
	link_arg<Upgrade, &PlayerControlAI::upgrade_completed>(get<PlayerCommon>()->manager.ref()->upgrade_completed);
}

b8 PlayerControlAI::in_range(const Vec3& p, r32 range) const
{
	Vec3 to_entity = p - get<Transform>()->absolute_pos();
	r32 distance_squared = to_entity.length_squared();
	return distance_squared < range * range;
}

PlayerControlAI::~PlayerControlAI()
{
#if DEBUG_AI_CONTROL
	camera->remove();
#endif
}

void PlayerControlAI::drone_done_flying_or_dashing()
{
	const AI::Config& config = player.ref()->config;
	inaccuracy = config.inaccuracy_min + (mersenne::randf_cc() * config.inaccuracy_range);
	aim_timer = 0.0f;
	aim_timeout = 0.0f;
	if (current.action.type == AI::RecordedLife::Action::TypeNone)
		action_done(true); // successfully panicked
	else if (path_index < path.length)
		path_index++;
}

void PlayerControlAI::drone_detaching()
{
	shot_at_target = true;
	hit_target = false;
	aim_timer = 0.0f;
	aim_timeout = 0.0f;
}

void PlayerControlAI::drone_hit(Entity* e)
{
	hit_target = true;
}

void PlayerControlAI::control_point_capture_completed(b8 success)
{
	action_done(success);
}

void PlayerControlAI::upgrade_completed(Upgrade upgrade)
{
	action_done(true);
}

void add_memory(Array<PlayerAI::Memory>* memories, Entity* entity, const Vec3& pos)
{
	b8 already_found = false;
	for (s32 j = 0; j < memories->length; j++)
	{
		PlayerAI::Memory* m = &(*memories)[j];
		if (m->entity.ref() == entity)
		{
			m->pos = pos;
			already_found = true;
			break;
		}
	}

	if (!already_found)
	{
		PlayerAI::Memory* m = memories->add();
		m->entity = entity;
		m->pos = pos;
	}
}

enum class MemoryStatus
{
	Update, // add or update existing memory
	Keep, // keep any existing memory, but don't update it
	Forget, // ignore and delete any existing memory
};

enum UpdateMemoryFlags
{
	UpdateMemoryLimitRange = 1,
};

template<typename Component>
void update_component_memory(PlayerControlAI* control, MemoryStatus (*filter)(const PlayerControlAI*, const Entity*), UpdateMemoryFlags flags = UpdateMemoryLimitRange)
{
	Array<PlayerAI::Memory>* memory = &control->player.ref()->memory;
	r32 range = control->get<Drone>()->range() * 1.5f;
	b8 limit_range = flags & UpdateMemoryLimitRange;
	// remove outdated memories
	for (s32 i = 0; i < memory->length; i++)
	{
		PlayerAI::Memory* m = &(*memory)[i];
		if (!limit_range || control->in_range(m->pos, range))
		{
			MemoryStatus status = MemoryStatus::Keep;
			Entity* entity = m->entity.ref();
			if (!entity || ((!limit_range || control->in_range(entity->get<Transform>()->absolute_pos(), range)) && filter(control, entity) == MemoryStatus::Forget))
			{
				memory->remove(i);
				i--;
			}
		}
	}

	// add or update memories
	for (auto i = Component::list.iterator(); !i.is_last(); i.next())
	{
		Vec3 pos = i.item()->template get<Transform>()->absolute_pos();
		if ((!limit_range || control->in_range(pos, range)) && filter(control, i.item()->entity()) == MemoryStatus::Update)
			add_memory(memory, i.item()->entity(), pos);
	}
}

Vec2 PlayerControlAI::aim(const Update& u, const Vec3& to_target)
{
	PlayerCommon* common = get<PlayerCommon>();
	Vec3 wall_normal = common->attach_quat * Vec3(0, 0, 1);

	const AI::Config& config = player.ref()->config;
	r32 target_angle_horizontal;
	{
		target_angle_horizontal = LMath::closest_angle(atan2f(to_target.x, to_target.z), common->angle_horizontal);

		{
			// make sure we don't try to turn through the wall
			r32 half_angle = (common->angle_horizontal + target_angle_horizontal) * 0.5f;
			if ((Quat::euler(0, half_angle, common->angle_vertical) * Vec3(0, 0, 1)).dot(wall_normal) < -0.5f)
				target_angle_horizontal = common->angle_horizontal - (target_angle_horizontal - common->angle_horizontal);
		}

		common->angle_horizontal = target_angle_horizontal > common->angle_horizontal
			? vi_min(target_angle_horizontal, common->angle_horizontal + vi_max(0.2f, target_angle_horizontal - common->angle_horizontal) * config.aim_speed * u.time.delta)
			: vi_max(target_angle_horizontal, common->angle_horizontal + vi_min(-0.2f, target_angle_horizontal - common->angle_horizontal) * config.aim_speed * u.time.delta);
		common->angle_horizontal = LMath::angle_range(common->angle_horizontal);
	}

	r32 target_angle_vertical;
	{
		target_angle_vertical = LMath::closest_angle(atan2f(-to_target.y, Vec2(to_target.x, to_target.z).length()), common->angle_vertical);

		{
			// make sure we don't try to turn through the wall
			r32 half_angle = (common->angle_vertical + target_angle_vertical) * 0.5f;
			if (half_angle < -PI * 0.5f
				|| half_angle > PI * 0.5f
				|| (Quat::euler(0, common->angle_horizontal, half_angle) * Vec3(0, 0, 1)).dot(wall_normal) < -0.5f)
			{
				target_angle_vertical = common->angle_vertical - (target_angle_vertical - common->angle_vertical);
			}
		}

		common->angle_vertical = target_angle_vertical > common->angle_vertical
			? vi_min(target_angle_vertical, common->angle_vertical + vi_max(0.2f, target_angle_vertical - common->angle_vertical) * config.aim_speed * u.time.delta)
			: vi_max(target_angle_vertical, common->angle_vertical + vi_min(-0.2f, target_angle_vertical - common->angle_vertical) * config.aim_speed * u.time.delta);
		common->angle_vertical = LMath::angle_range(common->angle_vertical);
	}

	common->angle_vertical = LMath::clampf(common->angle_vertical, -DRONE_VERTICAL_ANGLE_LIMIT, DRONE_VERTICAL_ANGLE_LIMIT);
	common->clamp_rotation(wall_normal, 0.5f);

	return Vec2(target_angle_horizontal, target_angle_vertical);
}

void PlayerControlAI::aim_and_shoot_target(const Update& u, const Vec3& target, Target* target_entity)
{
	PlayerCommon* common = get<PlayerCommon>();

	b8 can_move = common->movement_enabled();

	b8 only_crawling_dashing = false;

	{
		// crawling

		Vec3 pos = get<Transform>()->absolute_pos();
		Vec3 diff = target - pos;
		r32 distance_to_target = diff.length();

		Vec3 to_target = diff / distance_to_target;

		if (get<Drone>()->direction_is_toward_attached_wall(to_target)
			|| (distance_to_target < DRONE_DASH_DISTANCE && fabsf(to_target.dot(get<Transform>()->absolute_rot() * Vec3(0, 0, 1))) < 0.1f))
			only_crawling_dashing = true;

		// if we're shooting for a normal target (health or something), don't crawl
		// except if we're shooting at an enemy Drone and we're on the same surface as them, then crawl
		if (can_move)
		{
			Vec3 to_target_crawl = Vec3::normalize(target - pos);

			if (only_crawling_dashing)
			{
				// we're only going to be crawling and dashing there
				// crawl toward it, but if it's a target we're trying to shoot/dash through, don't get too close
				if (distance_to_target > DRONE_RADIUS * 2.0f)
					get<Drone>()->crawl(to_target_crawl, u);
			}
			else
			{
				// eventually we will shoot there

				// try to crawl toward the target
				Vec3 old_pos = get<Transform>()->pos;
				Quat old_rot = get<Transform>()->rot;
				Vec3 old_lerped_pos = get<Drone>()->lerped_pos;
				Quat old_lerped_rot = get<Drone>()->lerped_rotation;
				Transform* old_parent = get<Transform>()->parent.ref();
				get<Drone>()->crawl(to_target_crawl, u);

				Vec3 new_pos = get<Transform>()->absolute_pos();

				// make sure we can still go where we need to go
				if (!get<Drone>()->can_hit(target_entity))
				{
					// revert the crawling we just did
					get<Transform>()->pos = old_pos;
					get<Transform>()->rot = old_rot;
					get<Transform>()->parent = old_parent;
					get<Drone>()->lerped_pos = old_lerped_pos;
					get<Drone>()->lerped_rotation = old_lerped_rot;
					get<Drone>()->update_offset();
				}
			}
		}
	}

	{
		// shooting / dashing

		const AI::Config& config = player.ref()->config;

		b8 can_shoot = false;

		aim_timer += u.time.delta;
		if (can_move && get<Drone>()->cooldown_can_shoot())
		{
			aim_timeout += u.time.delta;
			if (aim_timer > config.aim_min_delay)
				can_shoot = true;
		}

		Vec3 pos = get<Transform>()->absolute_pos();
		Vec3 to_target = target - pos;
		r32 distance_to_target = to_target.length();
		to_target /= distance_to_target;
		Vec3 wall_normal = common->attach_quat * Vec3(0, 0, 1);

		Vec2 target_angles = aim(u, to_target);

		if (can_shoot)
		{
			// cooldown is done; we can shoot.
			// check if we're done aiming
			b8 lined_up = fabsf(LMath::angle_to(common->angle_horizontal, target_angles.x)) < inaccuracy
				&& fabsf(LMath::angle_to(common->angle_vertical, target_angles.y)) < inaccuracy;

			Vec3 look_dir = common->look_dir();
			if (only_crawling_dashing)
			{
				if (lined_up || distance_to_target < DRONE_SHIELD_RADIUS)
					get<Drone>()->dash_start(look_dir);
			}
			else
			{
				if (lined_up && get<Drone>()->can_shoot(look_dir))
				{
					shot_at_target = true;

					// reset timer for rapid-fire bolter shots
					// if we are actually moving, drone_detaching() will overwrite this to 0
					aim_timer = config.aim_min_delay - (0.2f + mersenne::randf_co() * 0.1f);

					get<Drone>()->go(look_dir);
				}
			}
		}
	}
}

// if tolerance is greater than 0, we need to land within that distance of the given target point
// returns true as long as it's possible for us to eventually hit the goal
b8 PlayerControlAI::go(const Update& u, const AI::DronePathNode& node_prev, const AI::DronePathNode& node, r32 tolerance)
{
	PlayerCommon* common = get<PlayerCommon>();

	b8 can_move = common->movement_enabled();

	b8 only_crawling_dashing = false;

	Vec3 position_before_crawling = get<Transform>()->absolute_pos();

	{
		// crawling

		Vec3 pos = position_before_crawling;
		Vec3 diff = node.pos - pos;
		r32 distance_to_target = diff.length();
		if (distance_to_target < DRONE_RADIUS * 1.2f)
		{
			// and we're already there
			drone_done_flying_or_dashing();
			return true;
		}

		Vec3 to_target = diff / distance_to_target;

		if (get<Drone>()->current_ability == Ability::None && (node.crawl || get<Drone>()->direction_is_toward_attached_wall(to_target)))
			only_crawling_dashing = true;

		// crawling
		// if we're shooting for a normal target (health or something), don't crawl
		// except if we're shooting at an enemy Drone and we're on the same surface as them, then crawl
		if (can_move)
		{
			Vec3 wall_normal = common->attach_quat * Vec3(0, 0, 1);
			Vec3 to_target_convex = (node.pos + node.normal * DRONE_RADIUS) - pos;
			Vec3 to_target_crawl;
			if (wall_normal.dot(to_target_convex) > 0.0f && node.normal.dot(wall_normal) < 0.9f)
			{
				// concave corner
				to_target_crawl = Vec3::normalize((node.pos + node.normal * -DRONE_RADIUS) - pos);
			}
			else
			{
				// coplanar or convex corner
				to_target_crawl = Vec3::normalize(to_target_convex);
			}

			if (only_crawling_dashing)
			{
				// we're only going to be crawling and dashing there
				// crawl toward it, but if it's a target we're trying to shoot/dash through, don't get too close
				get<Drone>()->crawl(to_target_crawl, u);
			}
			else
			{
				// eventually we will shoot there
				b8 could_go_before_crawling = false;
				Vec3 hit;
				if (get<Drone>()->can_shoot(to_target, &hit))
				{
					// we can go generally toward the target
					// now make sure we're actually going to land at the right spot
					if (tolerance < 0.0f // don't worry about where we land
						|| (hit - node.pos).length_squared() < tolerance * tolerance) // check the tolerance
						could_go_before_crawling = true;
				}

				if (could_go_before_crawling)
				{
					// try to crawl toward the target
					Vec3 old_pos = get<Transform>()->pos;
					Quat old_rot = get<Transform>()->rot;
					Vec3 old_lerped_pos = get<Drone>()->lerped_pos;
					Quat old_lerped_rot = get<Drone>()->lerped_rotation;
					Transform* old_parent = get<Transform>()->parent.ref();
					get<Drone>()->crawl(to_target_crawl, u);

					Vec3 new_pos = get<Transform>()->absolute_pos();

					// make sure we can still go where we need to go
					b8 revert = true;
					Vec3 hit;
					if (get<Drone>()->can_shoot(Vec3::normalize(node.pos - new_pos), &hit))
					{
						// we can go generally toward the target
						// now make sure we're actually going to land at the right spot
						if (tolerance < 0.0f // don't worry about where we land
							|| (hit - node.pos).length_squared() < tolerance * tolerance) // check the tolerance
							revert = false;
					}

					if (revert)
					{
						// revert the crawling we just did
						get<Transform>()->pos = old_pos;
						get<Transform>()->rot = old_rot;
						get<Transform>()->parent = old_parent;
						get<Drone>()->lerped_pos = old_lerped_pos;
						get<Drone>()->lerped_rotation = old_lerped_rot;
						get<Drone>()->update_offset();
					}
				}
				else
				{
					// we can't currently get to the target
					// crawl toward our current path node in an attempt to get a clear shot
					get<Drone>()->crawl(node_prev.pos - get<Transform>()->absolute_pos(), u);
				}
			}
		}
	}

	// shooting / dashing

	// aiming

	const AI::Config& config = player.ref()->config;

	Vec3 pos = get<Transform>()->absolute_pos();
	Vec3 to_target = Vec3::normalize(node.pos - pos);

	// check if we can't hit the goal and return false immediately, don't wait to aim
	if ((pos - position_before_crawling).length_squared() > 0.001f * 0.001f) // if we're still crawling, we may be able to hit it eventually
	{
		if (only_crawling_dashing)
		{
			// don't dash around corners or anything; only dash toward coplanar points
			if (fabsf(to_target.dot(get<Transform>()->absolute_rot() * Vec3(0, 0, 1))) > 0.1f)
				return false;
		}
		else
		{
			Vec3 hit;
			if (get<Drone>()->can_shoot(to_target, &hit))
			{
				// make sure we're actually going to land at the right spot
				if ((hit - node.pos).length_squared() > tolerance * tolerance) // check the tolerance
					return false;
			}
			else
				return false;
		}
	}

	b8 can_shoot = false;

	aim_timer += u.time.delta;
	if (can_move && get<Drone>()->cooldown_can_shoot())
	{
		aim_timeout += u.time.delta;
		if (aim_timer > config.aim_min_delay)
			can_shoot = true;
	}

	if (can_shoot)
	{
		Vec2 target_angles = aim(u, to_target);

		// cooldown is done; we can shoot.
		// check if we're done aiming
		if (common->angle_horizontal == target_angles.x
			&& common->angle_vertical == target_angles.y)
		{
			// aim is lined up
			Vec3 look_dir = common->look_dir();
			if (only_crawling_dashing)
			{
				// don't dash around corners or anything; only dash toward coplanar points
				if (fabsf(look_dir.dot(get<Transform>()->absolute_rot() * Vec3(0, 0, 1))) < 0.1f)
				{
					if (!get<Drone>()->dash_start(look_dir))
						return false;
				}
				else
					return false;
			}
			else
			{
				Vec3 hit;
				if (get<Drone>()->can_shoot(look_dir, &hit))
				{
					// make sure we're actually going to land at the right spot
					if ((hit - node.pos).length_squared() < tolerance * tolerance) // check the tolerance
						get<Drone>()->go(look_dir);
					else
						return false;
				}
				else
					return false;
			}
		}
	}
	
	return true;
}

b8 default_filter(const PlayerControlAI* control, const Entity* e)
{
	AI::Team team = control->get<AIAgent>()->team;
	return ForceField::hash(team, control->get<Transform>()->absolute_pos())
		== ForceField::hash(team, e->get<Transform>()->absolute_pos());
}

b8 energy_pickup_enemy_filter(const PlayerControlAI* control, const Entity* e)
{
	if (!default_filter(control, e))
		return false;

	AI::Team team = e->get<Battery>()->team;
	return team != AI::TeamNone && team != control->get<AIAgent>()->team;
}

b8 energy_pickup_filter(const PlayerControlAI* control, const Entity* e)
{
	if (!default_filter(control, e))
		return false;

	return e->get<Battery>()->team != control->get<AIAgent>()->team;
}

b8 minion_filter(const PlayerControlAI* control, const Entity* e)
{
	if (!default_filter(control, e))
		return false;

	return e->get<AIAgent>()->team != control->get<AIAgent>()->team
		&& !e->get<AIAgent>()->stealth;
}

s32 danger(const PlayerControlAI* control)
{
	if (control->get<PlayerCommon>()->incoming_attacker())
		return 3;

	r32 closest_drone;
	Drone::closest(~(1 << control->get<AIAgent>()->team), control->get<Transform>()->absolute_pos(), &closest_drone);

	if (closest_drone < DRONE_MAX_DISTANCE * 0.5f)
		return 2;

	if (closest_drone < DRONE_MAX_DISTANCE)
		return 1;

	return 0;
}

b8 control_point_filter(const PlayerControlAI* control, const Entity* e)
{
	if (!default_filter(control, e))
		return false;

	return danger(control) <= 0;
}

b8 enemy_control_point_filter(const PlayerControlAI* control, const Entity* e)
{
	if (!control_point_filter(control, e))
		return false;

	AI::Team team = control->get<AIAgent>()->team;
	ControlPoint* c = e->get<ControlPoint>();
	if (team == 0) // defend
		return c->team_next != AI::TeamNone && c->team_next != team;
	else // attack
		return c->team != team && c->team_next != team;
}

MemoryStatus minion_memory_filter(const PlayerControlAI* control, const Entity* e)
{
	if (e->get<AIAgent>()->stealth)
		return MemoryStatus::Keep;

	if (e->get<AIAgent>()->team == control->get<AIAgent>()->team)
		return MemoryStatus::Forget;
	else
		return MemoryStatus::Update;
}

MemoryStatus sensor_memory_filter(const PlayerControlAI* control, const Entity* e)
{
	if (e->get<Sensor>()->team == control->get<AIAgent>()->team || e->has<Battery>())
		return MemoryStatus::Forget;
	else
		return MemoryStatus::Update;
}

MemoryStatus drone_memory_filter(const PlayerControlAI* control, const Entity* e)
{
	if (e->get<AIAgent>()->stealth)
		return MemoryStatus::Keep; // don't update it, but also don't forget it

	if (e->get<AIAgent>()->team == control->get<AIAgent>()->team)
		return MemoryStatus::Forget; // don't care
	else
		return MemoryStatus::Update;
}

b8 drone_run_filter(const PlayerControlAI* control, const Entity* e)
{
	r32 run_chance = control->get<Health>()->hp == 1 ? 0.5f : 0.2f;
	return e->get<AIAgent>()->team != control->get<AIAgent>()->team
		&& e->get<Health>()->hp > control->get<Health>()->hp
		&& mersenne::randf_co() < run_chance
		&& (e->get<Drone>()->can_hit(control->get<Target>()) || (e->get<Transform>()->absolute_pos() - control->get<Transform>()->absolute_pos()).length_squared() < DRONE_MAX_DISTANCE * 0.5f * DRONE_MAX_DISTANCE * 0.5f);
}

b8 drone_find_filter(const PlayerControlAI* control, const Entity* e)
{
	if (!default_filter(control, e))
		return false;

	s16 my_hp = control->get<Health>()->hp;
	s16 enemy_hp = e->get<Health>()->hp;
	return e->get<AIAgent>()->team != control->get<AIAgent>()->team
		&& !e->get<AIAgent>()->stealth
		&& (e->has<Decoy>() || e->get<Drone>()->invincible_timer == 0.0f || (enemy_hp == 1 && my_hp > enemy_hp + 1))
		&& (enemy_hp <= my_hp || (my_hp > 1 && control->get<Drone>()->invincible_timer > 0.0f));
}

b8 drone_react_filter(const PlayerControlAI* control, const Entity* e)
{
	if (!drone_find_filter(control, e))
		return false;

	if (e->has<Decoy>())
		return true;
	else
	{
		Drone* a = e->get<Drone>();
		return a->state() == Drone::State::Crawl && a->invincible_timer == 0.0f;
	}
}

b8 force_field_filter(const PlayerControlAI* control, const Entity* e)
{
	if (!default_filter(control, e))
		return false;

	ForceField* field = e->get<ForceField>();
	return field->team != control->get<AIAgent>()->team && field->contains(control->get<Transform>()->absolute_pos());
}

b8 aicue_sensor_filter(const PlayerControlAI* control, const Entity* e)
{
	// only interested in interest points we don't have control over yet
	if (e->get<AICue>()->type & AICue::Type::Sensor)
	{
		r32 closest_distance;
		Sensor::closest(1 << control->get<AIAgent>()->team, e->get<Transform>()->absolute_pos(), &closest_distance);
		return closest_distance > SENSOR_RANGE;
	}
	return false;
}

MemoryStatus default_memory_filter(const PlayerControlAI* control, const Entity* e)
{
	return MemoryStatus::Update;
}

b8 attack_inbound(const PlayerControlAI* control)
{
	return control->get<PlayerCommon>()->incoming_attacker() != nullptr;
}

s32 geometry_query(const PlayerControlAI* control, r32 range, r32 angle_range, s32 count)
{
	Vec3 pos;
	Quat rot;
	control->get<Transform>()->absolute(&pos, &rot);

	s16 mask = ~DRONE_PERMEABLE_MASK & ~control->get<Drone>()->ally_force_field_mask();
	s32 result = 0;
	for (s32 i = 0; i < count; i++)
	{
		Vec3 ray = rot * (Quat::euler(PI + (mersenne::randf_co() - 0.5f) * angle_range, (PI * 0.5f) + (mersenne::randf_co() - 0.5f) * angle_range, 0) * Vec3(1, 0, 0));
		btCollisionWorld::ClosestRayResultCallback ray_callback(pos, pos + ray * range);
		Physics::raycast(&ray_callback, mask);
		if (ray_callback.hasHit())
			result++;
	}

	return result;
}

s32 team_density(AI::TeamMask mask, const Vec3& pos, r32 radius)
{
	r32 radius_sq = radius * radius;
	s32 score = 0;
	for (auto i = Drone::list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->get<AIAgent>()->team, mask)
			&& (i.item()->get<Transform>()->absolute_pos() - pos).length_squared() < radius_sq)
		{
			score += 3;
		}
	}

	for (auto i = MinionCommon::list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->get<AIAgent>()->team, mask)
			&& (i.item()->get<Transform>()->absolute_pos() - pos).length_squared() < radius_sq)
		{
			score += 2;
		}
	}

	for (auto i = ForceField::list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask)
			&& (i.item()->get<Transform>()->absolute_pos() - pos).length_squared() < radius_sq)
		{
			score += 2;
		}
	}

	for (auto i = Rocket::list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team(), mask)
			&& (i.item()->get<Transform>()->absolute_pos() - pos).length_squared() < radius_sq)
		{
			score += 1;
		}
	}

	for (auto i = Sensor::list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask)
			&& (i.item()->get<Transform>()->absolute_pos() - pos).length_squared() < radius_sq)
		{
			score += 1;
		}
	}

	return score;
}

void PlayerControlAI::update_memory()
{
	update_component_memory<Battery>(this, &default_memory_filter);
	update_component_memory<MinionCommon>(this, &minion_memory_filter);
	update_component_memory<Sensor>(this, &sensor_memory_filter);
	update_component_memory<AICue>(this, &default_memory_filter);
	update_component_memory<ControlPoint>(this, &default_memory_filter, UpdateMemoryFlags(0)); // unlimited range
	update_component_memory<ForceField>(this, &default_memory_filter);

	// update memory of enemy DRONE positions based on team sensor data

	update_component_memory<Drone>(this, &drone_memory_filter);

	const Team& team = Team::list[(s32)get<AIAgent>()->team];
	for (s32 i = 0; i < MAX_PLAYERS; i++)
	{
		const Team::SensorTrack& track = team.player_tracks[i];
		if (track.tracking && track.entity.ref())
			add_memory(&player.ref()->memory, track.entity.ref(), track.entity.ref()->get<Transform>()->absolute_pos());
	}
}

void PlayerControlAI::sniper_or_bolter_cancel()
{
	Ability a = get<Drone>()->current_ability;
	if (a == Ability::Sniper || a == Ability::Bolter)
		get<Drone>()->current_ability = Ability::None;
}

void PlayerControlAI::set_target(Entity* t, r32 delay)
{
	aim_timer = aim_timeout = -delay;
	target = t;
	hit_target = false;
	path.length = 0;
}

void PlayerControlAI::set_path(const AI::DronePath& p)
{
	path = p;
	path_index = 1; // first point is the starting point, should be roughly where we are already
	aim_timer = 0.0f;
	aim_timeout = 0.0f;
	target = nullptr;
	hit_target = false;
}

void PlayerControlAI::action_clear()
{
	current.action.type = AI::RecordedLife::Action::TypeNone;
	current.priority = 0;
	path.length = 0;
	target = nullptr;
}

b8 want_upgrade(PlayerControlAI* player, Upgrade u)
{
	PlayerManager* manager = player->get<PlayerCommon>()->manager.ref();
	return manager->upgrade_available(u) && manager->upgrade_cost(u) < manager->energy;
}

void action_go(PlayerControlAI* player, const AI::RecordedLife::Action& action)
{

}

void PlayerControlAI::action_done(b8 success)
{
#if DEBUG_AI_CONTROL
	vi_debug("Complete: %s", success ? "success" : "fail");
#endif

	static Upgrade upgrade_priorities[MAX_ABILITIES] = { Upgrade::Minion, Upgrade::Bolter, Upgrade::ForceField };

	action_clear();
	
	if (success)
		action_queue.clear(); // force calculation of new actions

	if (action_queue.size() == 0) // get new actions
	{
		AI::RecordedLife::Tag tag;
		tag.init(entity());

		PlayerManager* manager = get<PlayerCommon>()->manager.ref();

		AI::Team my_team = get<AIAgent>()->team;

		if (manager->at_upgrade_point())
		{
			for (s32 i = 0; i < MAX_ABILITIES; i++)
			{
				if (want_upgrade(this, upgrade_priorities[i]))
				{
					// buy upgrade
					AI::RecordedLife::Action action;
					action.type = AI::RecordedLife::Action::TypeUpgrade;
					action.ability = s8(upgrade_priorities[i]);
					action_queue.push({ 0, action });
				}
			}
		}

		if (Net::popcount(u32(tag.upgrades)) < MAX_ABILITIES)
		{
			for (s32 i = 0; i < MAX_ABILITIES; i++)
			{
				if (want_upgrade(this, upgrade_priorities[i]))
				{
					// go to upgrade
					AI::RecordedLife::Action action;
					action.type = AI::RecordedLife::Action::TypeMove;
					action.pos = manager->team.ref()->player_spawn.ref()->get<Transform>()->absolute_pos();
					action.normal = Vec3(0, 1, 0);
					action_queue.push({ 0, action });
				}
			}
		}

		if (Game::level.type == GameType::Rush)
		{
			b8 capture = false;
			b8 attacking = false;
			if (manager->team.ref()->team() == AI::Team(0)) // defend
			{
				if (tag.control_point_state.a == AI::RecordedLife::ControlPointState::StateLosingFirstHalf
					|| tag.control_point_state.a == AI::RecordedLife::ControlPointState::StateLosingSecondHalf
					|| tag.control_point_state.b == AI::RecordedLife::ControlPointState::StateLosingFirstHalf
					|| tag.control_point_state.b == AI::RecordedLife::ControlPointState::StateLosingSecondHalf)
				{
					capture = true;
				}
			}
			else // attack
			{
				attacking = true;
				if (tag.control_point_state.a == AI::RecordedLife::ControlPointState::StateNormal
					|| tag.control_point_state.a == AI::RecordedLife::ControlPointState::StateRecapturingFirstHalf
					|| tag.control_point_state.b == AI::RecordedLife::ControlPointState::StateNormal
					|| tag.control_point_state.b == AI::RecordedLife::ControlPointState::StateRecapturingFirstHalf)
				{
					capture = true;
				}
			}

			if (capture)
			{
				s32 priority = 0 - (255 - tag.time_remaining) / 70;

				if (manager->at_control_point())
				{
					priority -= 1;
					AI::RecordedLife::Action action;
					action.type = AI::RecordedLife::Action::TypeCapture;
					action_queue.push({ priority, action });
				}
				else
				{
					// go to control point
					AI::RecordedLife::Action action;
					action.type = AI::RecordedLife::Action::TypeMove;

					r32 closest_distance_sq = FLT_MAX;
					ControlPoint* closest_control_point = nullptr;
					for (auto i = ControlPoint::list.iterator(); !i.is_last(); i.next())
					{
						if ((attacking || i.item()->capture_timer > 0.0f)
							&& i.item()->team_next != my_team)
						{
							r32 distance_sq = (i.item()->get<Transform>()->absolute_pos() - tag.pos).length_squared();
							if (distance_sq < closest_distance_sq)
							{
								closest_distance_sq = distance_sq;
								closest_control_point = i.item();
							}
						}
					}

					{
						Quat rot;
						closest_control_point->get<Transform>()->absolute(&action.pos, &rot);
						action.normal = rot * Vec3(0, 1, 0);
					}

					action_queue.push({ priority, action });
				}
			}
		}
	}

	// execute action
	if (action_queue.size() == 0)
	{
#if DEBUG_AI_CONTROL
		vi_debug("Action: %d", 0);
#endif
		current.action.type = AI::RecordedLife::Action::TypeNone; // just do something
	}
	else
	{
		current = action_queue.pop();
#if DEBUG_AI_CONTROL
		vi_debug("Action: %d", s32(current.action.type));
#endif

		switch (current.action.type)
		{
			case AI::RecordedLife::Action::TypeMove:
			{
				auto callback = ObjectLinkEntryArg<PlayerControlAI, const AI::DroneResult&, &PlayerControlAI::path_callback>(id());
				Vec3 pos;
				Quat rot;
				get<Transform>()->absolute(&pos, &rot);
				AI::drone_pathfind(AI::DronePathfind::LongRange, AI::DroneAllow::All, get<AIAgent>()->team, pos, rot * Vec3(0, 0, 1), current.action.pos, current.action.normal, callback);
				break;
			}
			case AI::RecordedLife::Action::TypeUpgrade:
			{
				if (!get<PlayerCommon>()->manager.ref()->at_upgrade_point()
					|| !get<PlayerCommon>()->manager.ref()->upgrade_start(Upgrade(current.action.upgrade)))
					action_done(false); // fail
				break;
			}
			case AI::RecordedLife::Action::TypeCapture:
			{
				ControlPoint* c = get<PlayerCommon>()->manager.ref()->at_control_point();
				if (!c || !get<PlayerCommon>()->manager.ref()->capture_start())
					action_done(false); // fail
				break;
			}
			/*
			TypeAttack
			TypeAbility
			TypeWait
			*/
			default:
			{
				vi_assert(false);
				break;
			}
		}
	}
}

void PlayerControlAI::path_callback(const AI::DroneResult& result)
{
	if (result.path.length == 0)
		action_done(false); // failed
	else
		set_path(result.path);
}

void PlayerControlAI::update(const Update& u)
{
	if (get<Drone>()->state() == Drone::State::Crawl && !Team::game_over)
	{
		const AI::Config& config = player.ref()->config;

		// new random look direction
		if (s32(u.time.total * config.aim_speed * 0.3f) != s32((u.time.total - u.time.delta) * config.aim_speed * 0.3f))
			random_look = get<PlayerCommon>()->attach_quat * (Quat::euler(PI + (mersenne::randf_co() - 0.5f) * PI * 1.2f, (PI * 0.5f) + (mersenne::randf_co() - 0.5f) * PI * 1.2f, 0) * Vec3(1, 0, 0));

		if (target.ref())
		{
			if (target.ref()->has<Target>())
			{
				// trying to a hit a moving thingy
				r32 speed;
				switch (get<Drone>()->current_ability)
				{
					case Ability::Bolter:
					{
						speed = PROJECTILE_SPEED;
						break;
					}
					case Ability::Sniper:
					{
						speed = 0.0f; // instant
						break;
					}
					default:
					{
						speed = DRONE_FLY_SPEED;
						break;
					}
				}
				Vec3 intersection;
				if (aim_timeout < config.aim_timeout
					&& get<Drone>()->can_hit(target.ref()->get<Target>(), &intersection, speed))
					aim_and_shoot_target(u, intersection, target.ref()->get<Target>());
				else
					action_done(false); // we can't hit it
			}
			else
			{
				// just trying to go to a certain spot (probably our spawn)
				if (aim_timeout > config.aim_timeout)
					action_done(false); // something went wrong
				else
				{
					Vec3 target_pos;
					Quat target_rot;
					target.ref()->get<Transform>()->absolute(&target_pos, &target_rot);
					AI::DronePathNode target;
					target.crawl = false;
					target.pos = target_pos;
					target.normal = target_rot * Vec3(0, 0, 1);
					target.ref = DRONE_NAV_MESH_NODE_NONE;
					if (!go(u, target, target, CONTROL_POINT_RADIUS)) // assume the target is a control point
						action_done(false); // can't hit it
				}
			}
		}
		else if (path_index < path.length)
		{
			if (AbilityInfo::list[s32(get<Drone>()->current_ability)].type == AbilityInfo::Type::Shoot)
				sniper_or_bolter_cancel();

			// look at next target
			if (aim_timeout > config.aim_timeout)
			{
				// timeout; we can't hit it
				// mark path bad
#if DEBUG_AI_CONTROL
				vi_debug("Marking bad Drone adjacency");
#endif
				AI::drone_mark_adjacency_bad(path[path_index - 1].ref, path[path_index].ref);
				action_done(false); // action failed
			}
			else
			{
				if (!go(u, path[path_index - 1], path[path_index], DRONE_RADIUS)) // path_index starts at 1 so we're good here
				{
#if DEBUG_AI_CONTROL
					vi_debug("Marking bad Drone adjacency");
#endif
					AI::drone_mark_adjacency_bad(path[path_index - 1].ref, path[path_index].ref);
					action_done(false);
				}
			}
		}
		else
		{
			// look randomly
			aim(u, random_look);

			if (current.action.type == AI::RecordedLife::Action::TypeNone)
			{
				// pathfinding routines failed; we are stuck
				PlayerCommon* common = get<PlayerCommon>();
				if (common->movement_enabled())
				{
					// cooldown is done; we can shoot.
					Vec3 look_dir = common->look_dir();
					get<Drone>()->crawl(look_dir, u);
					if (get<Drone>()->can_shoot(look_dir))
						get<Drone>()->go(look_dir);
				}
			}
		}

		if (current.action.type != AI::RecordedLife::Action::TypeNone)
		{
			if (target.ref())
			{
				// current action is waiting for a callback; see if we're done executing it
				if (target.ref()->has<Target>())
				{
					if (shot_at_target)
					{
						if (get<Drone>()->current_ability != Ability::Bolter)
							action_done(hit_target); // call it success if we hit our target, or if there was nothing to hit
					}
				}
				else
				{
					// the only other kind of target we can have is a control point
					if ((target.ref()->get<Transform>()->absolute_pos() - get<Transform>()->absolute_pos()).length_squared() < CONTROL_POINT_RADIUS * CONTROL_POINT_RADIUS)
						action_done(true);
				}
			}
			else if (path.length > 0)
			{
				// current action is waiting for a callback; see if we're done executing it
				// following a path
				if (path_index >= path.length)
					action_done(path.length > 1); // call it success if the path we followed was actually valid
			}
		}
	}

#if DEBUG_AI_CONTROL
	// update camera
	s32 player_count = PlayerHuman::list.count() + PlayerAI::list.count();
	Camera::ViewportBlueprint* viewports = Camera::viewport_blueprints[player_count - 1];
	Camera::ViewportBlueprint* blueprint = &viewports[PlayerHuman::count_local() + player.id];

	camera->viewport =
	{
		Vec2((s32)(blueprint->x * (r32)u.input->width), (s32)(blueprint->y * (r32)u.input->height)),
		Vec2((s32)(blueprint->w * (r32)u.input->width), (s32)(blueprint->h * (r32)u.input->height)),
	};
	r32 aspect = camera->viewport.size.y == 0 ? 1 : (r32)camera->viewport.size.x / (r32)camera->viewport.size.y;
	camera->perspective((80.0f * PI * 0.5f / 180.0f), aspect, 0.02f, Game::level.skybox.far_plane);
	camera->rot = Quat::euler(0.0f, get<PlayerCommon>()->angle_horizontal, get<PlayerCommon>()->angle_vertical);
	PlayerHuman::camera_setup_drone(entity(), camera, DRONE_THIRD_PERSON_OFFSET);
#endif
}

const AI::Config& PlayerControlAI::config() const
{
	return player.ref()->config;
}


}
