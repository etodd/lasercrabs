#include "ai_player.h"
#include "mersenne/mersenne-twister.h"
#include "entities.h"
#include "console.h"
#include "awk.h"
#include "minion.h"
#include "bullet/src/BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "minion.h"
#include "noise.h"
#if DEBUG_AI_CONTROL
#include "render/views.h"
#endif

namespace VI
{


PinArray<AIPlayer, MAX_AI_PLAYERS> AIPlayer::list;

AIPlayer::Config::Config()
	: low_level(LowLevelLoop::Default),
	high_level(HighLevelLoop::Default),
	interval_memory_update(0.1f),
	interval_low_level(0.2f),
	interval_high_level(0.5f),
	inaccuracy_min(PI * 0.001f),
	inaccuracy_range(PI * 0.01f),
	cooldown_skip_chance(0.1f),
	aim_timeout(2.0f),
	aim_speed(4.0f),
	upgrade_priority
	{
		Upgrade::Minion,
		Upgrade::Sensor,
		Upgrade::Rocket,
		Upgrade::HealthBuff,
		Upgrade::HealthSteal,
		Upgrade::ContainmentField,
	},
	upgrade_strategies
	{
		UpgradeStrategy::IfAvailable, // sensor
		UpgradeStrategy::Ignore, // rocket
		UpgradeStrategy::SaveUp, // minion
		UpgradeStrategy::IfAvailable, // containment field
		UpgradeStrategy::IfAvailable, // health steal
		UpgradeStrategy::Ignore, // health buff
	}
{
}

AIPlayer::Config AIPlayer::generate_config()
{
	Config config;

	if (Game::save.level_index < 5)
	{
		// slower, less accurate
		config.interval_low_level = 1.0f;
		config.interval_high_level = 3.0f;
		config.interval_memory_update = 0.25f;
		config.inaccuracy_min = PI * 0.003f;
		config.inaccuracy_range = PI * 0.03f;
		config.aim_timeout = 4.0f;
		config.aim_speed = 1.0f;

		if (Game::save.level_index <= Game::tutorial_levels || mersenne::rand() % 2 == 0)
		{
			config.upgrade_priority[0] = Upgrade::Minion;
			config.upgrade_priority[1] = Upgrade::Sensor;
			config.upgrade_priority[2] = Upgrade::Rocket;
			config.upgrade_priority[3] = Upgrade::HealthBuff;
			config.upgrade_priority[4] = Upgrade::HealthSteal;
			config.upgrade_priority[5] = Upgrade::ContainmentField;
			config.upgrade_strategies[0] = UpgradeStrategy::IfAvailable; // sensor
			config.upgrade_strategies[1] = UpgradeStrategy::Ignore; // rocket
			config.upgrade_strategies[2] = UpgradeStrategy::SaveUp; // minion
			config.upgrade_strategies[3] = UpgradeStrategy::IfAvailable; // containment field
			config.upgrade_strategies[4] = UpgradeStrategy::IfAvailable; // health steal
			config.upgrade_strategies[5] = UpgradeStrategy::Ignore; // health buff
		}
		else
		{
			config.upgrade_priority[0] = Upgrade::Sensor;
			config.upgrade_priority[1] = Upgrade::Minion;
			config.upgrade_priority[2] = Upgrade::Rocket;
			config.upgrade_priority[3] = Upgrade::HealthBuff;
			config.upgrade_priority[4] = Upgrade::HealthSteal;
			config.upgrade_priority[5] = Upgrade::ContainmentField;
			config.upgrade_strategies[0] = UpgradeStrategy::SaveUp; // sensor
			config.upgrade_strategies[1] = UpgradeStrategy::Ignore; // rocket
			config.upgrade_strategies[2] = UpgradeStrategy::IfAvailable; // minion
			config.upgrade_strategies[3] = UpgradeStrategy::IfAvailable; // containment field
			config.upgrade_strategies[4] = UpgradeStrategy::IfAvailable; // health steal
			config.upgrade_strategies[5] = UpgradeStrategy::Ignore; // health buff
		}
	}

	return config;
}

AIPlayer::AIPlayer(PlayerManager* m, const Config& config)
	: manager(m),
	revision(),
	config(config)
{
	m->spawn.link<AIPlayer, &AIPlayer::spawn>(this);
}

void AIPlayer::update(const Update& u)
{
}

void AIPlayer::spawn()
{
	Entity* e = World::create<AwkEntity>(manager.ref()->team.ref()->team());

	Vec3 pos;
	Quat rot;
	manager.ref()->team.ref()->player_spawn.ref()->absolute(&pos, &rot);
	pos += Vec3(0, 0, PLAYER_SPAWN_RADIUS * 0.5f); // spawn it around the edges
	e->get<Transform>()->absolute(pos, rot);

	e->add<PlayerCommon>(manager.ref());

	manager.ref()->entity = e;

	e->add<AIPlayerControl>(this);
}

Upgrade AIPlayer::saving_up() const
{
	for (s32 i = 0; i < (s32)Upgrade::count; i++)
	{
		Upgrade upgrade = config.upgrade_priority[i];
		if (manager.ref()->upgrade_available(upgrade)
			&& !manager.ref()->has_upgrade(upgrade)
			&& config.upgrade_strategies[(s32)upgrade] == UpgradeStrategy::SaveUp)
		{
			return upgrade;
		}
	}
	return Upgrade::None;
}

AIPlayerControl::AIPlayerControl(AIPlayer* p)
	: player(p),
	path_index(),
	memory(),
	active_behavior(),
	path_priority(),
	path(),
	loop_high_level(),
	loop_low_level(),
	loop_low_level_2(),
	loop_memory(),
	target(),
	shot_at_target(),
	hit_target(),
	panic(),
	cooldown_skip()
{
#if DEBUG_AI_CONTROL
	camera = Camera::add();
#endif
}

Repeat* make_high_level_loop(AIPlayerControl*, const AIPlayer::Config&);
Repeat* make_low_level_loop(AIPlayerControl*, const AIPlayer::Config&);

void AIPlayerControl::awake()
{
#if DEBUG_AI_CONTROL
	camera->fog = false;
	camera->team = (u8)get<AIAgent>()->team;
	camera->mask = 1 << camera->team;
	camera->range = AWK_MAX_DISTANCE;
#endif
	link<&AIPlayerControl::awk_done_flying_or_dashing>(get<Awk>()->done_flying);
	link<&AIPlayerControl::awk_done_flying_or_dashing>(get<Awk>()->done_dashing);
	link_arg<Entity*, &AIPlayerControl::awk_hit>(get<Awk>()->hit);
	link<&AIPlayerControl::awk_detached>(get<Awk>()->detached);
	link<&AIPlayerControl::awk_detached>(get<Awk>()->dashed);

	// init behavior trees
	const AIPlayer::Config& config = player.ref()->config;

	loop_memory = Repeat::alloc // memory update loop
	(
		Sequence::alloc
		(
			Delay::alloc(config.interval_memory_update),
			Execute::alloc()->method<AIPlayerControl, &AIPlayerControl::update_memory >(this)
		)
	);
	loop_memory->set_context(this);

	loop_high_level = make_high_level_loop(this, config);

	loop_low_level = make_low_level_loop(this, config);

	loop_low_level_2 = make_low_level_loop(this, config);

	loop_memory->run();
	loop_high_level->run();
	loop_low_level->run();
}

b8 AIPlayerControl::in_range(const Vec3& p, r32 range) const
{
	Vec3 to_entity = p - get<Transform>()->absolute_pos();
	r32 distance_squared = to_entity.length_squared();
	return distance_squared < range * range;
}

AIPlayerControl::~AIPlayerControl()
{
#if DEBUG_AI_CONTROL
	camera->remove();
#endif
	loop_high_level->~Repeat();
	loop_low_level->~Repeat();
	loop_low_level_2->~Repeat();
	loop_memory->~Repeat();
}

void AIPlayerControl::awk_done_flying_or_dashing()
{
	const AIPlayer::Config& config = player.ref()->config;
	inaccuracy = config.inaccuracy_min + (mersenne::randf_cc() * config.inaccuracy_range);
	cooldown_skip = mersenne::randf_cc() < config.cooldown_skip_chance;
	aim_timer = 0.0f;
	if (path_index < path.length)
		path_index++;
}

void AIPlayerControl::awk_detached()
{
	shot_at_target = true;
	hit_target = false;
	aim_timer = 0.0f;
}

void AIPlayerControl::awk_hit(Entity* e)
{
	hit_target = true;
}

void AIPlayerControl::set_target(Entity* t)
{
	aim_timer = 0.0f;
	target = t;
	hit_target = false;
	path.length = 0;
}

void AIPlayerControl::set_path(const AI::AwkPath& p)
{
	path = p;
	path_index = 1; // first point is the starting point, should be roughly where we are already
	aim_timer = 0.0f;
	target = nullptr;
	hit_target = false;
}

void AIPlayerControl::behavior_start(Behavior* caller, s8 priority)
{
	// depending on which loop this behavior is in,
	// we need to abort the others and restart them
	Behavior* r = caller->root();
	if (r == loop_low_level)
	{
		if (loop_high_level->active())
			loop_high_level->abort();
		if (loop_low_level_2->active())
			loop_low_level_2->abort();
		loop_low_level_2->run();
	}
	else if (r == loop_low_level_2)
	{
		if (loop_high_level->active())
			loop_high_level->abort();
		if (loop_low_level->active())
			loop_low_level->abort();
		loop_low_level->run();
	}
	else
	{
		// high-level loop
		if (loop_low_level->active())
			loop_low_level->abort();
		if (loop_low_level_2->active())
			loop_low_level_2->abort();
		loop_low_level->run();
	}

#if DEBUG_AI_CONTROL
	const char* loop;
	if (r == loop_low_level)
		loop = "Low-level 1";
	else if (r == loop_low_level_2)
		loop = "Low-level 2";
	else
		loop = "High-level";
	vi_debug("%s: %s", loop, typeid(*caller).name());
#endif

	vi_assert(!active_behavior);

	active_behavior = caller;
	path_priority = priority;
}

void AIPlayerControl::behavior_clear()
{
	active_behavior = nullptr;
	shot_at_target = false;
	path_priority = 0;
	path.length = 0;
	target = nullptr;
}

b8 AIPlayerControl::restore_loops()
{
	// return to normal state
	if (!active_behavior)
	{
		if (loop_low_level_2->active())
			loop_low_level_2->abort();

		if (!loop_high_level->active())
			loop_high_level->run();

		if (!loop_low_level->active())
			loop_low_level->run();
	}

	return true;
}

void add_memory(AIPlayerControl::MemoryArray* component_memories, Entity* entity, const Vec3& pos)
{
	b8 already_found = false;
	for (s32 j = 0; j < component_memories->length; j++)
	{
		AIPlayerControl::Memory* m = &(*component_memories)[j];
		if (m->entity.ref() == entity)
		{
			m->pos = pos;
			already_found = true;
			break;
		}
	}

	if (!already_found)
	{
		AIPlayerControl::Memory* m = component_memories->add();
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

template<typename Component>
void update_component_memory(AIPlayerControl* control, MemoryStatus (*filter)(const AIPlayerControl*, const Entity*))
{
	AIPlayerControl::MemoryArray* component_memories = &control->memory[Component::family];
	// remove outdated memories
	for (s32 i = 0; i < component_memories->length; i++)
	{
		AIPlayerControl::Memory* m = &(*component_memories)[i];
		if (control->in_range(m->pos, VISIBLE_RANGE))
		{
			MemoryStatus status = MemoryStatus::Keep;
			Entity* entity = m->entity.ref();
			if (entity && control->in_range(entity->get<Transform>()->absolute_pos(), VISIBLE_RANGE) && filter(control, entity) == MemoryStatus::Forget)
			{
				component_memories->remove(i);
				i--;
			}
		}
	}

	// add or update memories
	if (component_memories->length < component_memories->capacity())
	{
		for (auto i = Component::list.iterator(); !i.is_last(); i.next())
		{
			Vec3 pos = i.item()->template get<Transform>()->absolute_pos();
			if (control->in_range(pos, VISIBLE_RANGE) && filter(control, i.item()->entity()) == MemoryStatus::Update)
			{
				add_memory(component_memories, i.item()->entity(), pos);
				if (component_memories->length == component_memories->capacity())
					break;
			}
		}
	}
}

// if tolerance is greater than 0, we need to land within that distance of the given target point
b8 AIPlayerControl::aim_and_shoot(const Update& u, const Vec3& path_node, const Vec3& target, r32 tolerance)
{
	PlayerCommon* common = get<PlayerCommon>();

	b8 can_move = common->movement_enabled();
	b8 can_shoot = can_move && (get<Awk>()->cooldown == 0.0f || (cooldown_skip && get<Awk>()->cooldown_can_go()));

	if (can_shoot)
		aim_timer += u.time.delta;

	// crawling
	if (can_move)
	{
		Vec3 pos = get<Awk>()->center();
		Vec3 to_target = Vec3::normalize(target - pos);

		b8 could_go_before_crawling;
		{
			Vec3 hit;
			could_go_before_crawling = get<Awk>()->can_go(to_target, &hit);
			if (could_go_before_crawling)
			{
				// we can go generally toward the target
				if (tolerance > 0.0f && (hit - target).length_squared() > tolerance * tolerance) // make sure we would actually land at the right spot
					could_go_before_crawling = false;
			}
		}

		if (could_go_before_crawling)
		{
			// try to crawl toward the target
			Vec3 old_pos;
			Quat old_rot;
			get<Transform>()->absolute(&old_pos, &old_rot);
			ID old_parent = get<Transform>()->parent.ref()->entity_id;
			get<Awk>()->crawl(to_target, u);

			Vec3 new_pos = get<Transform>()->absolute_pos();

			b8 revert = false;

			if (could_go_before_crawling)
			{
				revert = true;
				Vec3 hit;
				if (get<Awk>()->can_go(Vec3::normalize(target - new_pos), &hit))
				{
					// we can go generally toward the target
					// now make sure we're actually going to land at the right spot
					if (tolerance < 0.0f // don't worry about where we land
						|| (hit - target).length_squared() < tolerance * tolerance) // check the tolerance
						revert = false;
				}
			}

			if (revert)
				get<Awk>()->move(old_pos, old_rot, old_parent); // revert the crawling we just did
		}
		else
		{
			// we can't currently get to the target
			// crawl toward our current path node in an attempt to get a clear shot
			get<Awk>()->crawl(path_node - get<Transform>()->absolute_pos(), u);
		}
	}

	Vec3 pos = get<Awk>()->center();
	Vec3 to_target = Vec3::normalize(target - pos);
	Vec3 wall_normal = common->attach_quat * Vec3(0, 0, 1);

	const AIPlayer::Config& config = player.ref()->config;

	r32 target_angle_horizontal;
	{
		target_angle_horizontal = LMath::closest_angle(atan2(to_target.x, to_target.z), common->angle_horizontal);
		r32 dir_horizontal = target_angle_horizontal > common->angle_horizontal ? 1.0f : -1.0f;

		{
			// make sure we don't try to turn through the wall
			r32 half_angle = (common->angle_horizontal + target_angle_horizontal) * 0.5f;
			if ((Quat::euler(0, half_angle, 0) * Vec3(0, 0, 1)).dot(wall_normal) < -0.5f)
				dir_horizontal *= -1.0f; // go the other way
		}

		common->angle_horizontal = dir_horizontal > 0.0f
			? vi_min(target_angle_horizontal, common->angle_horizontal + vi_max(0.2f, target_angle_horizontal - common->angle_horizontal) * config.aim_speed * u.time.delta)
			: vi_max(target_angle_horizontal, common->angle_horizontal + vi_min(-0.2f, target_angle_horizontal - common->angle_horizontal) * config.aim_speed * u.time.delta);
		common->angle_horizontal = LMath::angle_range(common->angle_horizontal);
	}

	r32 target_angle_vertical;
	{
		target_angle_vertical = LMath::closest_angle(atan2(-to_target.y, Vec2(to_target.x, to_target.z).length()), common->angle_vertical);
		r32 dir_vertical = target_angle_vertical > common->angle_vertical ? 1.0f : -1.0f;

		{
			// make sure we don't try to turn through the wall
			r32 half_angle = (common->angle_vertical + target_angle_vertical) * 0.5f;
			if (half_angle < -PI * 0.5f
				|| half_angle > PI * 0.5f
				|| (Quat::euler(half_angle, common->angle_horizontal, 0) * Vec3(0, 0, 1)).dot(wall_normal) < -0.5f)
				dir_vertical *= -1.0f; // go the other way
		}

		common->angle_vertical = dir_vertical > 0.0f
			? vi_min(target_angle_vertical, common->angle_vertical + vi_max(0.2f, target_angle_vertical - common->angle_vertical) * config.aim_speed * u.time.delta)
			: vi_max(target_angle_vertical, common->angle_vertical + vi_min(-0.2f, target_angle_vertical - common->angle_vertical) * config.aim_speed * u.time.delta);
		common->angle_vertical = LMath::angle_range(common->angle_vertical);
	}

	common->angle_vertical = LMath::clampf(common->angle_vertical, PI * -0.495f, PI * 0.495f);
	common->clamp_rotation(wall_normal, 0.5f);

	if (can_shoot)
	{
		// cooldown is done; we can shoot.
		// check if we're done aiming
		b8 aim_lined_up;
		if (tolerance > 0.0f)
		{
			// must aim exactly
			aim_lined_up = common->angle_horizontal == target_angle_horizontal
				&& common->angle_vertical == target_angle_vertical;
		}
		else
		{
			// include some inaccuracy
			aim_lined_up = fabs(LMath::angle_to(common->angle_horizontal, target_angle_horizontal)) < inaccuracy
				&& fabs(LMath::angle_to(common->angle_vertical, target_angle_vertical)) < inaccuracy;
		}

		if (aim_lined_up)
		{
			Vec3 look_dir = common->look_dir();
			Vec3 hit;
			if (get<Awk>()->can_go(look_dir, &hit))
			{
				// make sure we're actually going to land at the right spot
				if (tolerance < 0.0f // don't worry about where we land
					|| (hit - target).length_squared() < tolerance * tolerance) // check the tolerance
				{
					if (get<Awk>()->detach(look_dir))
						return true;
				}
			}
		}
	}
	
	return false;
}

b8 health_pickup_filter(const AIPlayerControl* control, const Entity* e)
{
	Health* owner = e->get<HealthPickup>()->owner.ref();
	if (control->player.ref()->manager.ref()->has_upgrade(Upgrade::HealthSteal))
		return !owner || owner != control->get<Health>();
	else
		return !owner;
}

b8 minion_filter(const AIPlayerControl* control, const Entity* e)
{
	return e->get<AIAgent>()->team != control->get<AIAgent>()->team
		&& !e->get<AIAgent>()->stealth;
}

MemoryStatus minion_memory_filter(const AIPlayerControl* control, const Entity* e)
{
	if (e->get<AIAgent>()->stealth)
		return MemoryStatus::Keep;

	if (e->get<AIAgent>()->team == control->get<AIAgent>()->team)
		return MemoryStatus::Forget;
	else
		return MemoryStatus::Update;
}

MemoryStatus sensor_memory_filter(const AIPlayerControl* control, const Entity* e)
{
	if (e->get<Sensor>()->team == control->get<AIAgent>()->team)
		return MemoryStatus::Forget;
	else
		return MemoryStatus::Update;
}

MemoryStatus awk_memory_filter(const AIPlayerControl* control, const Entity* e)
{
	if (e->get<AIAgent>()->stealth)
		return MemoryStatus::Keep; // don't update it, but also don't forget it

	if (e->get<AIAgent>()->team == control->get<AIAgent>()->team)
		return MemoryStatus::Forget; // don't care
	else
		return MemoryStatus::Update;
}

b8 awk_run_filter(const AIPlayerControl* control, const Entity* e)
{
	return e->get<AIAgent>()->team != control->get<AIAgent>()->team
		&& e->get<Health>()->hp > control->get<Health>()->hp && (control->get<Health>()->hp == 1 || mersenne::randf_co() < 0.05f)
		&& (e->get<Awk>()->can_hit(control->get<Target>()) || (e->get<Transform>()->absolute_pos() - control->get<Transform>()->absolute_pos()).length_squared() < AWK_RUN_RADIUS * AWK_RUN_RADIUS);
}

b8 awk_attack_filter(const AIPlayerControl* control, const Entity* e)
{
	u16 my_hp = control->get<Health>()->hp;
	u16 enemy_hp = e->get<Health>()->hp;
	return e->get<AIAgent>()->team != control->get<AIAgent>()->team
		&& !e->get<AIAgent>()->stealth
		&& (e->get<Awk>()->invincible_timer == 0.0f || (enemy_hp == 1 && my_hp > enemy_hp + 1))
		&& (enemy_hp <= my_hp || (my_hp > 1 && control->get<Awk>()->invincible_timer > 0.0f));
}

b8 sensor_interest_point_filter(const AIPlayerControl* control, const Entity* e)
{
	// only interested in interest points we don't have control over yet
	r32 closest_distance;
	Sensor::closest(1 << control->get<AIAgent>()->team, e->get<Transform>()->absolute_pos(), &closest_distance);
	return closest_distance > SENSOR_RANGE;
}

b8 default_filter(const AIPlayerControl* control, const Entity* e)
{
	return true;
}

MemoryStatus default_memory_filter(const AIPlayerControl* control, const Entity* e)
{
	return MemoryStatus::Update;
}

Upgrade want_available_upgrade(const AIPlayerControl* control)
{
	if (!Game::level.has_feature(Game::FeatureLevel::Abilities))
		return Upgrade::None;

	PlayerManager* manager = control->player.ref()->manager.ref();
	const AIPlayer::Config& config = control->config();
	Upgrade if_available = Upgrade::None;
	for (s32 i = 0; i < (s32)Upgrade::count; i++)
	{
		Upgrade upgrade = config.upgrade_priority[i];
		if (manager->upgrade_available(upgrade)
			&& manager->credits >= manager->upgrade_cost(upgrade))
		{
			AIPlayer::UpgradeStrategy strategy = config.upgrade_strategies[(s32)upgrade];
			if (strategy == AIPlayer::UpgradeStrategy::SaveUp)
				return upgrade;
			else if (strategy == AIPlayer::UpgradeStrategy::IfAvailable && if_available == Upgrade::None)
				if_available = upgrade;
		}
	}
	return if_available;
}

b8 should_spawn_sensor(const AIPlayerControl* control)
{
	Vec3 me = control->get<Transform>()->absolute_pos();

	r32 closest_friendly_sensor;
	Sensor::closest(1 << control->get<AIAgent>()->team, me, &closest_friendly_sensor);
	if (closest_friendly_sensor > SENSOR_RANGE)
	{
		if (control->player.ref()->saving_up() == Upgrade::None) // only capture other stuff if we're not saving up for anything
		{
			if (SensorInterestPoint::in_range(me))
				return true;
		}
	}

	return false;
}

b8 should_spawn_rocket(const AIPlayerControl* control)
{
	if (control->player.ref()->saving_up() == Upgrade::None) // rockets are a luxury
	{
		// todo
		return false;
	}
	return false;
}

b8 should_spawn_minion(const AIPlayerControl* control)
{
	if (control->player.ref()->saving_up() == Upgrade::None) // minions are a luxury
	{
		AI::Team my_team = control->get<AIAgent>()->team;
		Vec3 my_pos;
		Quat my_rot;
		control->get<Transform>()->absolute(&my_pos, &my_rot);
		r32 closest_minion;
		MinionCommon::closest(1 << my_team, my_pos, &closest_minion);
		if (closest_minion > AWK_MAX_DISTANCE)
		{
			b8 spawn = false;
			r32 closest_enemy_sensor;
			Sensor::closest(~(1 << my_team), my_pos, &closest_enemy_sensor);
			if (closest_enemy_sensor < SENSOR_RANGE + AWK_MAX_DISTANCE)
				spawn = true;

			if (!spawn)
			{
				r32 closest_enemy_rocket;
				Rocket::closest(~(1 << my_team), my_pos, &closest_enemy_rocket);
				if (closest_enemy_rocket < ROCKET_RANGE)
					spawn = true;
			}

			if (!spawn)
			{
				r32 closest_enemy_field;
				ContainmentField::closest(~(1 << my_team), my_pos, &closest_enemy_field);
				if (closest_enemy_field < CONTAINMENT_FIELD_RADIUS + AWK_MAX_DISTANCE)
					spawn = true;
			}

			if (spawn)
			{
				// make sure the minion has a reasonably close surface to stand on
				Vec3 ray_start = my_pos + my_rot * Vec3(0, 0, 1);
				btCollisionWorld::ClosestRayResultCallback ray_callback(ray_start, ray_start + Vec3(0, -5, 0));
				Physics::raycast(&ray_callback, ~CollisionWalker & ~CollisionTarget & ~CollisionShield & ~CollisionAwk & ~CollisionTeamAContainmentField & ~CollisionTeamBContainmentField);
				if (ray_callback.hasHit())
					return true;
			}
		}
	}
	return false;
}

Repeat* make_low_level_loop(AIPlayerControl* control, const AIPlayer::Config& config)
{
	Repeat* loop;
	switch (config.low_level)
	{
		case AIPlayer::LowLevelLoop::Default:
		{
			loop = Repeat::alloc
			(
				Sequence::alloc
				(
					Succeed::alloc
					(
						Sequence::alloc
						(
							Delay::alloc(config.interval_low_level),
							AIBehaviors::WaitForAttachment::alloc(),
							Select::alloc // if any of these succeed, they will abort the high level loop
							(
								Select::alloc
								(
									Sequence::alloc
									(
										AIBehaviors::AttackInbound::alloc(),
										AIBehaviors::Panic::alloc(10000)
									),
									AIBehaviors::RunAway::alloc(Awk::family, 5, &awk_run_filter),
									AIBehaviors::ReactTarget::alloc(Awk::family, 4, 6, &awk_attack_filter),
									AIBehaviors::ReactTarget::alloc(MinionAI::family, 4, 5, &default_filter),
									Sequence::alloc
									(
										Invert::alloc(Execute::alloc()->method<Health, &Health::is_full>(control->get<Health>())), // make sure we need health
										AIBehaviors::ReactTarget::alloc(HealthPickup::family, 3, 4, &health_pickup_filter)
									)
								),
								Select::alloc
								(
									AIBehaviors::ReactTarget::alloc(Sensor::family, 3, 3, &default_filter),
									AIBehaviors::AbilitySpawn::alloc(4, Upgrade::Minion, Ability::Minion, &should_spawn_minion),
									AIBehaviors::AbilitySpawn::alloc(4, Upgrade::Sensor, Ability::Sensor, &should_spawn_sensor),
									AIBehaviors::AbilitySpawn::alloc(4, Upgrade::Rocket, Ability::Rocket, &should_spawn_rocket),
									Sequence::alloc
									(
										AIBehaviors::WantUpgrade::alloc(),
										AIBehaviors::ReactSpawn::alloc(4),
										AIBehaviors::DoUpgrade::alloc(4)
									)
								)
							)
						)
					),
					Execute::alloc()->method<AIPlayerControl, &AIPlayerControl::restore_loops>(control) // restart the high level loop if necessary
				)
			);
			break;
		}
		case AIPlayer::LowLevelLoop::Noop:
		{
			loop = Repeat::alloc(Delay::alloc(1.0f));
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}
	loop->set_context(control);
	return loop;
}

Repeat* make_high_level_loop(AIPlayerControl* control, const AIPlayer::Config& config)
{
	Repeat* loop;
	switch (config.high_level)
	{
		case AIPlayer::HighLevelLoop::Default:
		{
			loop = Repeat::alloc
			(
				Sequence::alloc
				(
					Delay::alloc(config.interval_high_level),
					AIBehaviors::WaitForAttachment::alloc(),
					Succeed::alloc
					(
						Select::alloc
						(
							Sequence::alloc
							(
								Invert::alloc(Execute::alloc()->method<Health, &Health::is_full>(control->get<Health>())), // make sure we need health
								AIBehaviors::Find::alloc(HealthPickup::family, 2, &health_pickup_filter)
							),
							Sequence::alloc
							(
								AIBehaviors::WantUpgrade::alloc(),
								AIBehaviors::ToSpawn::alloc(4)
							),
							AIBehaviors::Find::alloc(MinionAI::family, 2, &minion_filter),
							AIBehaviors::Find::alloc(Sensor::family, 2, &default_filter),
							AIBehaviors::Find::alloc(Awk::family, 2, &awk_attack_filter),
							Sequence::alloc
							(
								AIBehaviors::HasUpgrade::alloc(Upgrade::Sensor),
								AIBehaviors::Find::alloc(SensorInterestPoint::family, 2, &sensor_interest_point_filter)
							),
							AIBehaviors::RandomPath::alloc(1),
							AIBehaviors::Panic::alloc(1)
						)
					)
				)
			);
			break;
		}
		case AIPlayer::HighLevelLoop::Noop:
		{
			loop = Repeat::alloc(Delay::alloc(1.0f));
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}
	loop->set_context(control);
	return loop;
}

b8 AIPlayerControl::update_memory()
{
	update_component_memory<HealthPickup>(this, &default_memory_filter);
	update_component_memory<MinionAI>(this, &minion_memory_filter);
	update_component_memory<Sensor>(this, &sensor_memory_filter);
	update_component_memory<SensorInterestPoint>(this, &default_memory_filter);

	// update memory of enemy AWK positions based on team sensor data
	const Team& team = Team::list[(s32)get<AIAgent>()->team];
	for (s32 i = 0; i < MAX_PLAYERS; i++)
	{
		const Team::SensorTrack& track = team.player_tracks[i];
		if (track.tracking && track.entity.ref())
			add_memory(&memory[Awk::family], track.entity.ref(), track.entity.ref()->get<Transform>()->absolute_pos());
	}

	update_component_memory<Awk>(this, &awk_memory_filter);

	return true; // this returns true so we can call this from an Execute behavior
}

void AIPlayerControl::update(const Update& u)
{
	if (get<Awk>()->state() == Awk::State::Crawl && !Team::game_over)
	{
		const AIPlayer::Config& config = player.ref()->config;

		if (target.ref())
		{
			if (target.ref()->has<Target>())
			{
				// trying to a hit a moving thingy
				Vec3 intersection;
				if (get<Awk>()->can_hit(target.ref()->get<Target>(), &intersection))
					aim_and_shoot(u, intersection, intersection, -1.0f);
				else
					active_behavior->done(false); // we can't hit it
			}
			else
			{
				// just trying to go to a certain spot (probably our spawn)
				if (aim_timer > config.aim_timeout)
					active_behavior->done(false); // something went wrong
				else
				{
					Vec3 t = target.ref()->get<Transform>()->absolute_pos();
					aim_and_shoot(u, t, t, PLAYER_SPAWN_RADIUS); // assume the target is a player spawn
				}
			}
		}
		else if (path_index < path.length)
		{
			// look at next target
			if (aim_timer > config.aim_timeout)
			{
				// timeout; we can't hit it
				// mark path bad
#if DEBUG_AI_CONTROL
				vi_debug("Marking bad Awk adjacency");
#endif
				AI::awk_mark_adjacency_bad(path[path_index - 1].ref, path[path_index].ref);
				active_behavior->done(false); // active behavior failed
			}
			else
				aim_and_shoot(u, path[path_index - 1].pos, path[path_index].pos, AWK_RADIUS); // path_index starts at 1 so we're good here
		}
		else
		{
			// look randomly
			PlayerCommon* common = get<PlayerCommon>();
			r32 offset = Game::time.total * 0.2f;
			common->angle_horizontal += noise::sample3d(Vec3(offset)) * config.aim_speed * 2.0f * u.time.delta;
			common->angle_vertical += noise::sample3d(Vec3(offset + 64)) * config.aim_speed * u.time.delta;
			common->angle_vertical = LMath::clampf(common->angle_vertical, PI * -0.495f, PI * 0.495f);
			common->clamp_rotation(common->attach_quat * Vec3(0, 0, 1), 0.5f);

			if (panic)
			{
				// pathfinding routines failed; we are stuck
				if (common->movement_enabled())
				{
					// cooldown is done; we can shoot.
					Vec3 look_dir = common->look_dir();
					get<Awk>()->crawl(look_dir, u);
					if (get<Awk>()->can_go(look_dir))
					{
						if (get<Awk>()->detach(look_dir))
							active_behavior->done(true);
					}
				}
			}
		}
	}

	if (!panic)
	{
		if (target.ref())
		{
			// a behavior is waiting for a callback; see if we're done executing it
			if (target.ref()->has<Target>())
			{
				if (shot_at_target)
					active_behavior->done(hit_target); // call it success if we hit our target, or if there was nothing to hit
			}
			else
			{
				// the only other kind of target we can have is our spawn
				if ((target.ref()->get<Transform>()->absolute_pos() - get<Transform>()->absolute_pos()).length_squared() < PLAYER_SPAWN_RADIUS * PLAYER_SPAWN_RADIUS)
					active_behavior->done(true);
			}
		}
		else if (path.length > 0)
		{
			// a behavior is waiting for a callback; see if we're done executing it
			// following a path
			if (path_index >= path.length)
				active_behavior->done(path.length > 1); // call it success if the path we followed was actually valid
		}
	}

#if DEBUG_AI_CONTROL
	// update camera
	s32 player_count = LocalPlayer::list.count() + AIPlayer::list.count();
	Camera::ViewportBlueprint* viewports = Camera::viewport_blueprints[player_count - 1];
	Camera::ViewportBlueprint* blueprint = &viewports[LocalPlayer::list.count() + player.id];

	camera->viewport =
	{
		Vec2((s32)(blueprint->x * (r32)u.input->width), (s32)(blueprint->y * (r32)u.input->height)),
		Vec2((s32)(blueprint->w * (r32)u.input->width), (s32)(blueprint->h * (r32)u.input->height)),
	};
	r32 aspect = camera->viewport.size.y == 0 ? 1 : (r32)camera->viewport.size.x / (r32)camera->viewport.size.y;
	camera->perspective((80.0f * PI * 0.5f / 180.0f), aspect, 0.02f, Game::level.skybox.far_plane);
	camera->rot = Quat::euler(0.0f, get<PlayerCommon>()->angle_horizontal, get<PlayerCommon>()->angle_vertical);
	camera->range = AWK_MAX_DISTANCE;
	camera->wall_normal = camera->rot.inverse() * ((get<Transform>()->absolute_rot() * get<Awk>()->lerped_rotation) * Vec3(0, 0, 1));
	camera->pos = get<Awk>()->center();
#endif
}

const AIPlayer::Config& AIPlayerControl::config() const
{
	return player.ref()->config;
}

namespace AIBehaviors
{

Find::Find(Family fam, s8 priority, b8(*filter)(const AIPlayerControl*, const Entity*))
	: filter(filter), family(fam)
{
	path_priority = priority;
}

void Find::run()
{
	active(true);
	if (control->get<Awk>()->state() == Awk::State::Crawl && path_priority > control->path_priority)
	{
		const AIPlayerControl::MemoryArray& memory = control->memory[family];
		const AIPlayerControl::Memory* closest = nullptr;
		Entity* closest_entity;
		r32 closest_distance = FLT_MAX;
		Vec3 pos = control->get<Transform>()->absolute_pos();
		for (s32 i = 0; i < memory.length; i++)
		{
			r32 distance = (memory[i].pos - pos).length_squared();
			if (distance < closest_distance)
			{
				if (!control->in_range(memory[i].pos, VISIBLE_RANGE) || (memory[i].entity.ref() && filter(control, memory[i].entity.ref())))
				{
					closest_distance = distance;
					closest = &memory[i];
				}
			}
		}
		if (closest)
		{
			pathfind(closest->pos, Vec3::zero, AI::AwkPathfind::LongRange);
			return;
		}
	}
	done(false);
}

RandomPath::RandomPath(s8 priority)
{
	RandomPath::path_priority = priority;
}

void RandomPath::run()
{
	active(true);
	if (control->get<Awk>()->state() == Awk::State::Crawl && path_priority > control->path_priority)
	{
		Vec3 pos;
		Quat rot;
		control->get<Transform>()->absolute(&pos, &rot);
		AI::awk_random_path(control->get<AIAgent>()->team, pos, rot * Vec3(0, 0, 1), ObjectLinkEntryArg<Base<RandomPath>, const AI::AwkResult&, &Base<RandomPath>::path_callback>(id()));
	}
	else
		done(false);
}

void AttackInbound::run()
{
	active(true);
	done(control->get<Awk>()->incoming_attacker() != nullptr);
}

HasUpgrade::HasUpgrade(Upgrade u)
	: upgrade(u)
{
}

void HasUpgrade::run()
{
	active(true);
	done(control->player.ref()->manager.ref()->has_upgrade(upgrade));
}

Panic::Panic(s8 priority)
{
	Panic::path_priority = priority;
}

void Panic::abort()
{
	control->panic = false;
	Base<Panic>::abort();
}

void Panic::done(b8 a)
{
	control->panic = false;
	Base<Panic>::done(a);
}

// pathfinding routines failed; we are stuck
void Panic::run()
{
	active(true);
	if (path_priority > control->path_priority)
	{
		control->panic = true;
		control->behavior_start(this, 10000000); // if we're panicking, nothing can interrupt us
	}
	else
		done(false);
}

// WaitForAttachment waits for us to be on a surface and done with any ability upgrades
void WaitForAttachment::set_context(void* ctx)
{
	Base::set_context(ctx);
	control->get<Awk>()->done_flying.link<WaitForAttachment, &WaitForAttachment::done_flying_or_dashing>(this);
	control->get<Awk>()->done_dashing.link<WaitForAttachment, &WaitForAttachment::done_flying_or_dashing>(this);
	control->player.ref()->manager.ref()->upgrade_completed.link<WaitForAttachment, Upgrade, &WaitForAttachment::upgrade_completed>(this);
}

void WaitForAttachment::done_flying_or_dashing()
{
	if (active() && control->player.ref()->manager.ref()->current_upgrade == Upgrade::None)
		done(true);
}

void WaitForAttachment::upgrade_completed(Upgrade)
{
	if (active() && control->get<Awk>()->state() == Awk::State::Crawl)
		done(true);
}

void WaitForAttachment::run()
{
	active(true);
	if (control->get<Awk>()->state() == Awk::State::Crawl && control->player.ref()->manager.ref()->current_upgrade == Upgrade::None)
		done(true);
}

AbilitySpawn::AbilitySpawn(s8 priority, Upgrade required_upgrade, Ability ability, AbilitySpawnFilter filter)
	: required_upgrade(required_upgrade), ability(ability), filter(filter)
{
	AbilitySpawn::path_priority = priority;
}

void AbilitySpawn::set_context(void* ctx)
{
	Base::set_context(ctx);
	control->player.ref()->manager.ref()->ability_spawned.link<AbilitySpawn, Ability, &AbilitySpawn::completed>(this);
	control->player.ref()->manager.ref()->ability_spawn_canceled.link<AbilitySpawn, Ability, &AbilitySpawn::canceled>(this);
}

void AbilitySpawn::completed(Ability a)
{
	if (active())
		done(a == ability);
}

void AbilitySpawn::canceled(Ability a)
{
	if (active() && a == ability)
		done(false);
}

void AbilitySpawn::run()
{
	active(true);

	PlayerManager* manager = control->player.ref()->manager.ref();

	const AbilityInfo& info = AbilityInfo::list[(s32)Ability::Sensor];

	if (AbilitySpawn::path_priority > control->path_priority
		&& control->get<Awk>()->state() == Awk::State::Crawl
		&& manager->has_upgrade(required_upgrade)
		&& manager->credits > info.spawn_cost
		&& filter(control))
	{
		if (manager->ability_spawn_start(ability))
		{
			control->behavior_start(this, AbilitySpawn::path_priority);
			return;
		}
	}

	done(false);
}

void AbilitySpawn::abort()
{
	// we have to deactivate ourselves first before we call ability_spawn_stop()
	// that way, when we get the notification that the ability spawn has been canceled, we'll already be inactive, so we won't care.
	Base<AbilitySpawn>::abort();
	control->player.ref()->manager.ref()->ability_spawn_stop(ability);
}

ReactTarget::ReactTarget(Family fam, s8 priority_path, s8 react_priority, b8(*filter)(const AIPlayerControl*, const Entity*))
	: react_priority(react_priority), filter(filter), family(fam)
{
	path_priority = priority_path;
}

void ReactTarget::run()
{
	active(true);
	b8 can_path = path_priority > control->path_priority;
	b8 can_react = react_priority > control->path_priority;
	if (control->get<Awk>()->state() == Awk::State::Crawl && (can_path || can_react))
	{
		Entity* closest = nullptr;
		r32 closest_distance = AWK_MAX_DISTANCE * AWK_MAX_DISTANCE;
		Vec3 pos = control->get<Transform>()->absolute_pos();
		const AIPlayerControl::MemoryArray& memory = control->memory[family];
		for (s32 i = 0; i < memory.length; i++)
		{
			r32 distance = (memory[i].pos - pos).length_squared();
			if (distance < closest_distance)
			{
				if (!control->in_range(memory[i].pos, AWK_MAX_DISTANCE) || (memory[i].entity.ref() && filter(control, memory[i].entity.ref())))
				{
					closest_distance = distance;
					closest = memory[i].entity.ref();
				}
			}
		}

		if (closest)
		{
			b8 can_hit_now = control->get<Awk>()->can_hit(closest->get<Target>());
			if (can_hit_now && can_react)
			{
				control->behavior_start(this, react_priority);
				control->set_target(closest);
				return;
			}
			else if (can_path)
			{
				pathfind(closest->get<Target>()->absolute_pos(), Vec3::zero, AI::AwkPathfind::Target);
				return;
			}
		}
	}
	done(false);
}

RunAway::RunAway(Family fam, s8 priority_path, b8(*filter)(const AIPlayerControl*, const Entity*))
	: filter(filter), family(fam)
{
	path_priority = priority_path;
}

void RunAway::run()
{
	active(true);
	Vec3 pos = control->get<Transform>()->absolute_pos();
	if (control->get<Awk>()->state() == Awk::State::Crawl
		&& !control->get<AIAgent>()->stealth // if we're stealthed, no need to run away
		&& control->get<Awk>()->invincible_timer == 0.0f // if we're invincible, no need to run away
		&& path_priority > control->path_priority
		&& (HealthPickup::available_count() > 0 || control->player.ref()->manager.ref()->has_upgrade(Upgrade::HealthSteal))
		&& !ContainmentField::inside(control->get<AIAgent>()->team, pos)) // if we're inside a containment field, running away is probably useless
	{
		Entity* closest = nullptr;
		r32 closest_distance = AWK_MAX_DISTANCE * AWK_MAX_DISTANCE;
		const AIPlayerControl::MemoryArray& memory = control->memory[family];
		for (s32 i = 0; i < memory.length; i++)
		{
			r32 distance = (memory[i].pos - pos).length_squared();
			if (distance < closest_distance)
			{
				if (control->in_range(memory[i].pos, AWK_MAX_DISTANCE)
					&& memory[i].entity.ref()
					&& filter(control, memory[i].entity.ref()))
				{
					closest_distance = distance;
					closest = memory[i].entity.ref();
				}
			}
		}
		if (closest)
		{
			Vec3 enemy_pos;
			Quat enemy_rot;
			closest->get<Transform>()->absolute(&enemy_pos, &enemy_rot);
			pathfind(enemy_pos, enemy_rot * Vec3(0, 0, 1), AI::AwkPathfind::Away);
			return;
		}
	}
	done(false);
}

WantUpgrade::WantUpgrade()
{
}

void WantUpgrade::run()
{
	active(true);
	Upgrade u = want_available_upgrade(control);
	done(u != Upgrade::None);
}

ToSpawn::ToSpawn(s8 priority)
{
	path_priority = priority;
}

void ToSpawn::run()
{
	active(true);
	if (control->get<Awk>()->state() == Awk::State::Crawl && path_priority > control->path_priority)
	{
		PlayerManager* manager = control->player.ref()->manager.ref();
		pathfind(manager->team.ref()->player_spawn.ref()->absolute_pos(), Vec3(0, 1, 0), AI::AwkPathfind::LongRange);
		return;
	}
	done(false);
}

ReactSpawn::ReactSpawn(s8 priority)
{
	path_priority = priority;
}

void ReactSpawn::run()
{
	active(true);
	if (path_priority > control->path_priority)
	{
		PlayerManager* manager = control->player.ref()->manager.ref();
		Transform* spawn = manager->team.ref()->player_spawn.ref();

		Vec3 me = control->get<Awk>()->center();
		Vec3 target = spawn->absolute_pos();
		Vec3 to_target = target - me;
		r32 distance_squared = to_target.length_squared();
		if (distance_squared < PLAYER_SPAWN_RADIUS * PLAYER_SPAWN_RADIUS)
		{
			done(true);
			return;
		}
		if (distance_squared < AWK_MAX_DISTANCE * AWK_MAX_DISTANCE)
		{
			Vec3 hit;
			if (control->get<Awk>()->can_go(to_target, &hit))
			{
				if ((hit - target).length_squared() < PLAYER_SPAWN_RADIUS * PLAYER_SPAWN_RADIUS)
				{
					control->behavior_start(this, path_priority);
					control->set_target(spawn->entity());
					return;
				}
			}
		}
	}
	done(false);
}

DoUpgrade::DoUpgrade(s8 priority)
{
	path_priority = priority;
}

void DoUpgrade::set_context(void* ctx)
{
	Base::set_context(ctx);
	control->player.ref()->manager.ref()->upgrade_completed.link<DoUpgrade, Upgrade, &DoUpgrade::completed>(this);
}

void DoUpgrade::completed(Upgrade u)
{
	if (active())
	{
#if DEBUG_AI_CONTROL
		vi_debug("Upgrade: %d", u);
#endif
		done(true);
	}
}

void DoUpgrade::run()
{
	active(true);
	if (path_priority > control->path_priority)
	{
		PlayerManager* manager = control->player.ref()->manager.ref();
		if (manager->at_spawn() && manager->current_upgrade == Upgrade::None)
		{
			Upgrade u = want_available_upgrade(control);
			if (u != Upgrade::None)
			{
				if (manager->upgrade_start(u))
				{
					control->behavior_start(this, 10000); // set the priority higher than everything else; upgrades can't be cancelled
					return;
				}
			}
		}
	}
	done(false);
}

void update_active(const Update& u)
{
}

}

}
