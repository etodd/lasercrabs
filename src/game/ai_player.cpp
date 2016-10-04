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


PinArray<AIPlayer, MAX_PLAYERS> AIPlayer::list;

AIPlayer::Config::Config()
	: low_level(LowLevelLoop::Default),
	high_level(HighLevelLoop::Default),
	interval_memory_update(0.2f),
	interval_low_level(0.25f),
	interval_high_level(0.5f),
	inaccuracy_min(PI * 0.001f),
	inaccuracy_range(PI * 0.01f),
	aim_timeout(2.0f),
	aim_speed(3.0f),
	aim_min_delay(0.5f),
	dodge_chance(0.2f),
	upgrade_priority { },
	upgrade_strategies { }
{
}

AIPlayer::Config AIPlayer::generate_config()
{
	Config config;

	switch (mersenne::rand() % 5)
	{
		case 0:
		{
			config.upgrade_priority[0] = Upgrade::Minion;
			config.upgrade_strategies[0] = UpgradeStrategy::SaveUp;
			config.upgrade_priority[1] = Upgrade::Teleporter;
			config.upgrade_strategies[1] = UpgradeStrategy::IfAvailable;
			config.upgrade_priority[2] = Upgrade::Rocket;
			config.upgrade_strategies[2] = UpgradeStrategy::SaveUp;
			config.upgrade_priority[3] = Upgrade::None;
			config.upgrade_priority[4] = Upgrade::None;
			config.upgrade_priority[5] = Upgrade::None;
			break;
		}
		case 1:
		{
			config.upgrade_priority[0] = Upgrade::Sensor;
			config.upgrade_strategies[0] = UpgradeStrategy::SaveUp;
			config.upgrade_priority[1] = Upgrade::Teleporter;
			config.upgrade_strategies[1] = UpgradeStrategy::IfAvailable;
			config.upgrade_priority[2] = Upgrade::Sniper;
			config.upgrade_strategies[2] = UpgradeStrategy::IfAvailable;
			config.upgrade_priority[3] = Upgrade::None;
			config.upgrade_priority[4] = Upgrade::None;
			config.upgrade_priority[5] = Upgrade::None;
			break;
		}
		case 2:
		{
			config.upgrade_priority[0] = Upgrade::ContainmentField;
			config.upgrade_strategies[0] = UpgradeStrategy::SaveUp;
			config.upgrade_priority[1] = Upgrade::Sniper;
			config.upgrade_strategies[1] = UpgradeStrategy::IfAvailable;
			config.upgrade_priority[2] = Upgrade::Rocket;
			config.upgrade_strategies[2] = UpgradeStrategy::IfAvailable;
			config.upgrade_priority[3] = Upgrade::None;
			config.upgrade_priority[4] = Upgrade::None;
			config.upgrade_priority[5] = Upgrade::None;
			break;
		}
		case 3:
		{
			config.upgrade_priority[0] = Upgrade::Sensor;
			config.upgrade_strategies[0] = UpgradeStrategy::SaveUp;
			config.upgrade_priority[1] = Upgrade::Rocket;
			config.upgrade_strategies[1] = UpgradeStrategy::SaveUp;
			config.upgrade_priority[2] = Upgrade::Teleporter;
			config.upgrade_strategies[2] = UpgradeStrategy::IfAvailable;
			config.upgrade_priority[3] = Upgrade::None;
			config.upgrade_priority[4] = Upgrade::None;
			config.upgrade_priority[5] = Upgrade::None;
			break;
		}
		case 4:
		{
			config.upgrade_priority[0] = Upgrade::Teleporter;
			config.upgrade_strategies[0] = UpgradeStrategy::IfAvailable;
			config.upgrade_priority[1] = Upgrade::Minion;
			config.upgrade_strategies[1] = UpgradeStrategy::IfAvailable;
			config.upgrade_priority[2] = Upgrade::Rocket;
			config.upgrade_strategies[2] = UpgradeStrategy::IfAvailable;
			config.upgrade_priority[3] = Upgrade::None;
			config.upgrade_priority[4] = Upgrade::None;
			config.upgrade_priority[5] = Upgrade::None;
			break;
		}
		default:
		{
			vi_assert(false);
			break;
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
	pos += Quat::euler(0, (id() * PI * 0.5f), 0) * Vec3(0, 0, CONTROL_POINT_RADIUS * 0.5f); // spawn it around the edges
	e->get<Transform>()->absolute(pos, rot);

	e->add<PlayerCommon>(manager.ref());

	manager.ref()->entity = e;

	e->add<AIPlayerControl>(this);
}

// save up priority ranges from -2 to 3
s32 AIPlayer::save_up_priority() const
{
	u16 increment = manager.ref()->increment();
	u16 credits = manager.ref()->credits;
	for (s32 i = 0; i < (s32)Upgrade::count; i++)
	{
		Upgrade upgrade = config.upgrade_priority[i];
		if (upgrade != Upgrade::None
			&& manager.ref()->upgrade_available(upgrade))
		{
			UpgradeStrategy strategy = config.upgrade_strategies[i];
			s32 priority;
			if (strategy == UpgradeStrategy::Ignore)
				continue;
			else if (strategy == UpgradeStrategy::IfAvailable)
				priority = 1;
			else // save up
				priority = 2;

			if (credits < manager.ref()->upgrade_cost(upgrade) * 1.2f)
				priority += 1;

			if (increment > 20)
				priority -= 2;
			else if (increment > 10)
				priority -= 1;
			else if (increment < 5)
				priority += 1;
			return priority;
		}
	}

	if (credits < 80 && increment < 5)
		return 1;

	return 0;
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
	random_look(0, 0, 1)
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
	r32 range = control->get<Awk>()->range() * 1.5f;
	// remove outdated memories
	for (s32 i = 0; i < component_memories->length; i++)
	{
		AIPlayerControl::Memory* m = &(*component_memories)[i];
		if (control->in_range(m->pos, range))
		{
			MemoryStatus status = MemoryStatus::Keep;
			Entity* entity = m->entity.ref();
			if (entity && control->in_range(entity->get<Transform>()->absolute_pos(), range) && filter(control, entity) == MemoryStatus::Forget)
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
			if (control->in_range(pos, range) && filter(control, i.item()->entity()) == MemoryStatus::Update)
			{
				add_memory(component_memories, i.item()->entity(), pos);
				if (component_memories->length == component_memories->capacity())
					break;
			}
		}
	}
}

Vec2 AIPlayerControl::aim(const Update& u, const Vec3& to_target)
{
	PlayerCommon* common = get<PlayerCommon>();
	Vec3 wall_normal = common->attach_quat * Vec3(0, 0, 1);

	const AIPlayer::Config& config = player.ref()->config;
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

	common->angle_vertical = LMath::clampf(common->angle_vertical, -AWK_VERTICAL_ANGLE_LIMIT, AWK_VERTICAL_ANGLE_LIMIT);
	common->clamp_rotation(wall_normal, 0.5f);

	return Vec2(target_angle_horizontal, target_angle_vertical);
}

// if tolerance is greater than 0, we need to land within that distance of the given target point
b8 AIPlayerControl::aim_and_shoot_target(const Update& u, const Vec3& target, Target* target_entity)
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

		if (get<Awk>()->direction_is_toward_attached_wall(to_target)
			|| (distance_to_target < AWK_DASH_DISTANCE && fabs(to_target.dot(get<Transform>()->absolute_rot() * Vec3(0, 0, 1))) < 0.1f))
			only_crawling_dashing = true;

		// if we're shooting for a normal target (health or something), don't crawl
		// except if we're shooting at an enemy Awk and we're on the same surface as them, then crawl
		if (can_move)
		{
			Vec3 to_target_crawl = Vec3::normalize(target - pos);

			if (only_crawling_dashing)
			{
				// we're only going to be crawling and dashing there
				// crawl toward it, but if it's a target we're trying to shoot/dash through, don't get too close
				if (distance_to_target > AWK_RADIUS * 2.0f)
					get<Awk>()->crawl(to_target_crawl, u);
			}
			else
			{
				// eventually we will shoot there

				// try to crawl toward the target
				Vec3 old_pos = get<Transform>()->pos;
				Quat old_rot = get<Transform>()->rot;
				Vec3 old_lerped_pos = get<Awk>()->lerped_pos;
				Quat old_lerped_rot = get<Awk>()->lerped_rotation;
				Transform* old_parent = get<Transform>()->parent.ref();
				get<Awk>()->crawl(to_target_crawl, u);

				Vec3 new_pos = get<Transform>()->absolute_pos();

				// make sure we can still go where we need to go
				if (!get<Awk>()->can_hit(target_entity))
				{
					// revert the crawling we just did
					get<Transform>()->pos = old_pos;
					get<Transform>()->rot = old_rot;
					get<Transform>()->parent = old_parent;
					get<Awk>()->lerped_pos = old_lerped_pos;
					get<Awk>()->lerped_rotation = old_lerped_rot;
					get<Awk>()->update_offset();
				}
			}
		}
	}

	{
		// shooting / dashing

		const AIPlayer::Config& config = player.ref()->config;

		b8 can_shoot = can_move && get<Awk>()->cooldown_can_shoot() && u.time.total - get<Awk>()->attach_time > config.aim_min_delay;

		if (can_shoot)
			aim_timer += u.time.delta;

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
			b8 lined_up = fabs(LMath::angle_to(common->angle_horizontal, target_angles.x)) < inaccuracy
				&& fabs(LMath::angle_to(common->angle_vertical, target_angles.y)) < inaccuracy;

			Vec3 look_dir = common->look_dir();
			if (only_crawling_dashing)
			{
				if ((lined_up || distance_to_target < AWK_SHIELD_RADIUS) && get<Awk>()->dash_start(look_dir))
					return true;
			}
			else
			{
				if (lined_up && get<Awk>()->can_shoot(look_dir) && get<Awk>()->detach(look_dir))
					return true;
			}
		}
	}
	
	return false;
}

b8 AIPlayerControl::go(const Update& u, const AI::AwkPathNode& node_prev, const AI::AwkPathNode& node, r32 tolerance)
{
	PlayerCommon* common = get<PlayerCommon>();

	b8 can_move = common->movement_enabled();

	b8 only_crawling_dashing = false;

	{
		// crawling

		Vec3 pos = get<Transform>()->absolute_pos();
		Vec3 diff = node.pos - pos;
		r32 distance_to_target = diff.length();
		if (distance_to_target < AWK_RADIUS * 1.2f)
		{
			// and we're already there
			awk_done_flying_or_dashing();
			return true;
		}

		Vec3 to_target = diff / distance_to_target;

		if (node.crawl || get<Awk>()->direction_is_toward_attached_wall(to_target))
			only_crawling_dashing = true;

		// crawling
		// if we're shooting for a normal target (health or something), don't crawl
		// except if we're shooting at an enemy Awk and we're on the same surface as them, then crawl
		if (can_move)
		{
			Vec3 wall_normal = common->attach_quat * Vec3(0, 0, 1);
			Vec3 to_target_convex = (node.pos + node.normal * AWK_RADIUS) - pos;
			Vec3 to_target_crawl;
			if (wall_normal.dot(to_target_convex) > 0.0f && node.normal.dot(wall_normal) < 0.9f)
			{
				// concave corner
				to_target_crawl = Vec3::normalize((node.pos + node.normal * -AWK_RADIUS) - pos);
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
				get<Awk>()->crawl(to_target_crawl, u);
			}
			else
			{
				// eventually we will shoot there
				b8 could_go_before_crawling = false;
				Vec3 hit;
				if (get<Awk>()->can_shoot(to_target, &hit))
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
					Vec3 old_lerped_pos = get<Awk>()->lerped_pos;
					Quat old_lerped_rot = get<Awk>()->lerped_rotation;
					Transform* old_parent = get<Transform>()->parent.ref();
					get<Awk>()->crawl(to_target_crawl, u);

					Vec3 new_pos = get<Transform>()->absolute_pos();

					// make sure we can still go where we need to go
					b8 revert = true;
					Vec3 hit;
					if (get<Awk>()->can_shoot(Vec3::normalize(node.pos - new_pos), &hit))
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
						get<Awk>()->lerped_pos = old_lerped_pos;
						get<Awk>()->lerped_rotation = old_lerped_rot;
						get<Awk>()->update_offset();
					}
				}
				else
				{
					// we can't currently get to the target
					// crawl toward our current path node in an attempt to get a clear shot
					get<Awk>()->crawl(node_prev.pos - get<Transform>()->absolute_pos(), u);
				}
			}
		}
	}

	// shooting / dashing

	if (get<Awk>()->current_ability == Ability::Sniper)
	{
		// we're in sniper mode, we're not going to be shooting anything, just look around randomly
		aim(u, random_look);
	}
	else
	{
		// aiming

		const AIPlayer::Config& config = player.ref()->config;

		b8 can_shoot = can_move && get<Awk>()->cooldown_can_shoot() && u.time.total - get<Awk>()->attach_time > config.aim_min_delay;

		if (can_shoot)
			aim_timer += u.time.delta;

		Vec3 pos = get<Transform>()->absolute_pos();
		Vec3 to_target = Vec3::normalize(node.pos - pos);
		Vec3 wall_normal = common->attach_quat * Vec3(0, 0, 1);

		Vec2 target_angles = aim(u, to_target);

		if (can_shoot)
		{
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
					if (fabs(look_dir.dot(get<Transform>()->absolute_rot() * Vec3(0, 0, 1))) < 0.1)
					{
						if (get<Awk>()->dash_start(look_dir))
							return true;
					}
				}
				else
				{
					Vec3 hit;
					if (get<Awk>()->can_shoot(look_dir, &hit))
					{
						// make sure we're actually going to land at the right spot
						if ((hit - node.pos).length_squared() < tolerance * tolerance) // check the tolerance
						{
							if (get<Awk>()->detach(look_dir))
								return true;
						}
					}
				}
			}
		}
	}
	
	return false;
}

b8 default_filter(const AIPlayerControl* control, const Entity* e)
{
	AI::Team team = control->get<AIAgent>()->team;
	return ContainmentField::hash(team, control->get<Transform>()->absolute_pos())
		== ContainmentField::hash(team, e->get<Transform>()->absolute_pos());
}

b8 energy_pickup_filter(const AIPlayerControl* control, const Entity* e)
{
	if (!default_filter(control, e))
		return false;

	return e->get<EnergyPickup>()->team != control->get<AIAgent>()->team;
}

b8 minion_filter(const AIPlayerControl* control, const Entity* e)
{
	if (!default_filter(control, e))
		return false;

	return e->get<AIAgent>()->team != control->get<AIAgent>()->team
		&& !e->get<AIAgent>()->stealth;
}

s32 danger(const AIPlayerControl* control)
{
	if (control->get<Awk>()->incoming_attacker())
		return 3;

	r32 closest_awk;
	Awk::closest(~(1 << control->get<AIAgent>()->team), control->get<Transform>()->absolute_pos(), &closest_awk);

	if (closest_awk < AWK_MAX_DISTANCE * 0.5f)
		return 2;

	if (closest_awk < AWK_MAX_DISTANCE)
		return 1;

	return 0;
}

b8 control_point_filter(const AIPlayerControl* control, const Entity* e)
{
	if (!default_filter(control, e))
		return false;

	return danger(control) <= 0;
}

b8 enemy_control_point_filter(const AIPlayerControl* control, const Entity* e)
{
	if (!control_point_filter(control, e))
		return false;

	return Game::level.has_feature(Game::FeatureLevel::Abilities)
		&& e->get<ControlPoint>()->team != control->get<AIAgent>()->team;
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
	r32 run_chance = control->get<Health>()->hp == 1 ? 0.5f : 0.2f;
	return e->get<AIAgent>()->team != control->get<AIAgent>()->team
		&& e->get<Health>()->hp > control->get<Health>()->hp
		&& mersenne::randf_co() < run_chance
		&& (e->get<Awk>()->can_hit(control->get<Target>()) || (e->get<Transform>()->absolute_pos() - control->get<Transform>()->absolute_pos()).length_squared() < AWK_MAX_DISTANCE * 0.5f * AWK_MAX_DISTANCE * 0.5f);
}

b8 awk_find_filter(const AIPlayerControl* control, const Entity* e)
{
	if (!default_filter(control, e))
		return false;

	u16 my_hp = control->get<Health>()->hp;
	u16 enemy_hp = e->get<Health>()->hp;
	return e->get<AIAgent>()->team != control->get<AIAgent>()->team
		&& !e->get<AIAgent>()->stealth
		&& (e->get<Awk>()->invincible_timer == 0.0f || (enemy_hp == 1 && my_hp > enemy_hp + 1))
		&& (enemy_hp <= my_hp || (my_hp > 1 && control->get<Awk>()->invincible_timer > 0.0f));
}

b8 awk_react_filter(const AIPlayerControl* control, const Entity* e)
{
	if (!awk_find_filter(control, e))
		return false;

	return e->get<Awk>()->state() == Awk::State::Crawl;
}

b8 containment_field_filter(const AIPlayerControl* control, const Entity* e)
{
	if (!default_filter(control, e))
		return false;

	ContainmentField* field = e->get<ContainmentField>();
	return field->team != control->get<AIAgent>()->team && field->contains(control->get<Transform>()->absolute_pos());
}

b8 aicue_sensor_filter(const AIPlayerControl* control, const Entity* e)
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
	for (s32 i = 0; i < (s32)Upgrade::count; i++)
	{
		Upgrade upgrade = config.upgrade_priority[i];
		if (upgrade != Upgrade::None
			&& manager->upgrade_available(upgrade)
			&& manager->credits > manager->upgrade_cost(upgrade)
			&& config.upgrade_strategies[i] != AIPlayer::UpgradeStrategy::Ignore)
		{
			return upgrade;
		}
	}
	return Upgrade::None;
}

b8 want_upgrade_filter(const AIPlayerControl* control)
{
	return want_available_upgrade(control) != Upgrade::None && control->player.ref()->save_up_priority() > 0;
}

b8 really_want_upgrade_filter(const AIPlayerControl* control)
{
	return want_available_upgrade(control) != Upgrade::None && control->player.ref()->save_up_priority() > 1;
}

b8 should_spawn_sensor(const AIPlayerControl* control)
{
	Vec3 me = control->get<Transform>()->absolute_pos();

	r32 closest_friendly_sensor;
	Sensor::closest(1 << control->get<AIAgent>()->team, me, &closest_friendly_sensor);
	if (closest_friendly_sensor > SENSOR_RANGE)
	{
		s32 cues_in_range;
		AICue::in_range(AICue::Type::Sensor, me, SENSOR_RANGE, &cues_in_range);
		return cues_in_range > 0 && cues_in_range >= control->player.ref()->save_up_priority();
	}

	return false;
}

b8 attack_inbound(const AIPlayerControl* control)
{
	return control->get<Awk>()->incoming_attacker() != nullptr;
}

s32 geometry_query(const AIPlayerControl* control, r32 range, r32 angle_range, s32 count)
{
	Vec3 pos;
	Quat rot;
	control->get<Transform>()->absolute(&pos, &rot);

	s16 mask = ~AWK_PERMEABLE_MASK & ~CollisionAwk & ~control->get<Awk>()->ally_containment_field_mask();
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

b8 should_spawn_rocket(const AIPlayerControl* control)
{
	Vec3 pos;
	Quat rot;
	control->get<Transform>()->absolute(&pos, &rot);

	{
		r32 closest_rocket;
		Rocket::closest(1 << control->get<AIAgent>()->team, pos, &closest_rocket);
		if (closest_rocket < 0.75f)
			return false;
	}

	s32 priority = AICue::in_range(AICue::Type::Rocket, pos, 8.0f) ? 2 : 1;

	if (Sensor::can_see(control->get<AIAgent>()->team, pos, rot * Vec3(0, 0, 1)))
		priority += 1;

	if (ContainmentField::inside(1 << control->get<AIAgent>()->team, pos))
		priority += 1;

	{
		r32 closest_enemy_awk;
		Awk::closest(~(1 << control->get<AIAgent>()->team), control->get<Transform>()->absolute_pos(), &closest_enemy_awk);
		if (closest_enemy_awk < 8.0f)
			priority -= 1; // too close
		else if (closest_enemy_awk < AWK_MAX_DISTANCE)
			priority += 1;
	}

	if (control->player.ref()->save_up_priority() >= priority)
		return false;

	// make sure it's in a relatively open area
	return geometry_query(control, AWK_MAX_DISTANCE * 0.4f, PI * 0.35f, 8) < 4;
}

b8 sniping(const AIPlayerControl* control)
{
	return control->get<Awk>()->current_ability == Ability::Sniper;
}

b8 should_snipe(const AIPlayerControl* control)
{
	Vec3 pos;
	Quat rot;
	control->get<Transform>()->absolute(&pos, &rot);

	if (mersenne::randf_co() < 0.5f)
		return false;

	s32 priority = AICue::in_range(AICue::Type::Snipe, pos, 8.0f) ? 2 : 1;

	if (Sensor::can_see(control->get<AIAgent>()->team, pos, rot * Vec3(0, 0, 1)))
		priority += 1;

	if (ContainmentField::inside(1 << control->get<AIAgent>()->team, pos))
		priority += 1;

	if (control->player.ref()->save_up_priority() >= priority)
		return false;

	if (geometry_query(control, AWK_MAX_DISTANCE * 0.4f, PI * 0.35f, 8) < 4)
	{
		b8 result = false;
		{
			const AIPlayerControl::MemoryArray& memory = control->memory[Awk::family];
			for (s32 i = 0; i < memory.length; i++)
			{
				Vec3 to_awk = memory[i].pos - pos;
				if (to_awk.length_squared() < AWK_MAX_DISTANCE * 0.6f * AWK_MAX_DISTANCE * 0.6f)
					return false; // too close
				if (!control->get<Awk>()->direction_is_toward_attached_wall(to_awk))
					result = true; // the awk is at the right distance and it's in front of us
			}
		}

		{
			const AIPlayerControl::MemoryArray& memory = control->memory[MinionCommon::family];
			for (s32 i = 0; i < memory.length; i++)
			{
				Vec3 to_minion = memory[i].pos - pos;
				if (!control->get<Awk>()->direction_is_toward_attached_wall(to_minion))
				{
					if (to_minion.length_squared() < AWK_MAX_DISTANCE * 0.6f * AWK_MAX_DISTANCE * 0.6f)
						return false; // too close
					result = true; // the minion is at the right distance and it's in front of us
				}
			}
		}

		return result;
	}

	return false;
}

b8 should_spawn_containment_field(const AIPlayerControl* control)
{
	Vec3 me = control->get<Transform>()->absolute_pos();
	AI::Team team = control->get<AIAgent>()->team;

	// make sure we're not overlapping with an existing friendly containment field
	for (auto i = ContainmentField::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team == team
			&& (i.item()->get<Transform>()->absolute_pos() - me).length_squared() < CONTAINMENT_FIELD_RADIUS * 2.0f * CONTAINMENT_FIELD_RADIUS * 2.0f)
		{
			return false;
		}
	}

	s32 save_up_priority = control->player.ref()->save_up_priority();
	if (save_up_priority < 2)
	{
		r32 closest_distance;
		EnergyPickup::closest(1 << team, me, &closest_distance);
		if (closest_distance < CONTAINMENT_FIELD_RADIUS)
			return true;
	}

	if (save_up_priority < 1)
	{
		r32 closest_distance;
		Awk* closest = Awk::closest(~(1 << team), me, &closest_distance);
		if (closest_distance < CONTAINMENT_FIELD_RADIUS && closest->get<Health>()->hp <= control->get<Health>()->hp)
			return true;

		// todo: use containment field to protect friendly minions, control points, and sensors
	}
	return false;
}

s32 team_density(AI::TeamMask mask, const Vec3& pos, r32 radius)
{
	r32 radius_sq = radius * radius;
	s32 score = 0;
	for (auto i = Awk::list.iterator(); !i.is_last(); i.next())
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

	for (auto i = ContainmentField::list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask)
			&& (i.item()->get<Transform>()->absolute_pos() - pos).length_squared() < radius_sq)
		{
			score += 2;
		}
	}

	for (auto i = Rocket::list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask)
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

	for (auto i = Teleporter::list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask)
			&& (i.item()->get<Transform>()->absolute_pos() - pos).length_squared() < radius_sq)
		{
			score += 1;
		}
	}

	return score;
}

b8 should_teleport(const AIPlayerControl* control)
{
	if (control->get<Health>()->hp <= 2 && danger(control) > 1)
		return true;

	AI::Team team = control->get<AIAgent>()->team;
	Vec3 pos = control->get<Transform>()->absolute_pos();

	if (MinionCommon::count(1 << team) > 2
		&& team_density(1 << team, pos, AWK_MAX_DISTANCE) < 5
		&& team_density(~(1 << team), pos, AWK_MAX_DISTANCE) > 3)
		return true;

	return false;
}

b8 should_spawn_minion(const AIPlayerControl* control)
{
	if (control->player.ref()->save_up_priority() < 2 && danger(control) <= 1)
	{
		AI::Team my_team = control->get<AIAgent>()->team;
		Vec3 my_pos;
		Quat my_rot;
		control->get<Transform>()->absolute(&my_pos, &my_rot);

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
			Physics::raycast(&ray_callback, ~AWK_PERMEABLE_MASK & ~CollisionAwk & ~control->get<Awk>()->ally_containment_field_mask());
			if (ray_callback.hasHit())
				return true;
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
										// sniper mode
										AIBehaviors::Test::alloc(&sniping),
										Select::alloc
										(
											AIBehaviors::ReactTarget::alloc(Awk::family, 0, 6, &awk_react_filter),
											AIBehaviors::ReactTarget::alloc(MinionCommon::family, 0, 5, &default_filter),
											AIBehaviors::RandomPath::alloc(4),
											Sequence::alloc
											(
												AIBehaviors::Chance::alloc(0.05f),
												Execute::alloc()->method<AIPlayerControl, &AIPlayerControl::snipe_stop>(control)
											),
											Succeed::alloc() // make sure we never hit minions or anything
										)
									),
									Sequence::alloc
									(
										AIBehaviors::Chance::alloc(config.dodge_chance),
										AIBehaviors::Test::alloc(&attack_inbound),
										AIBehaviors::Panic::alloc(127)
									),
									AIBehaviors::RunAway::alloc(Awk::family, 6, &awk_run_filter),
									AIBehaviors::ReactTarget::alloc(ContainmentField::family, 4, 6, &containment_field_filter),
									AIBehaviors::ReactTarget::alloc(Awk::family, 4, 6, &awk_react_filter),
									AIBehaviors::ReactTarget::alloc(MinionCommon::family, 4, 5, &default_filter),
									AIBehaviors::ReactTarget::alloc(EnergyPickup::family, 3, 4, &energy_pickup_filter)
								),
								Select::alloc
								(
									AIBehaviors::ReactTarget::alloc(Sensor::family, 3, 4, &default_filter),
									AIBehaviors::AbilitySpawn::alloc(),
									Sequence::alloc
									(
										AIBehaviors::ReactControlPoint::alloc(4, &enemy_control_point_filter),
										AIBehaviors::CaptureControlPoint::alloc(5)
									),
									Sequence::alloc
									(
										AIBehaviors::Test::alloc(&want_upgrade_filter),
										AIBehaviors::ReactControlPoint::alloc(5, &control_point_filter),
										AIBehaviors::CaptureControlPoint::alloc(5),
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
							Select::alloc
							(
								AIBehaviors::Find::alloc(EnergyPickup::family, 2, &energy_pickup_filter),
								Sequence::alloc
								(
									AIBehaviors::Test::alloc(&want_upgrade_filter),
									AIBehaviors::Find::alloc(ControlPoint::family, 3, &default_filter)
								),
								Sequence::alloc
								(
									AIBehaviors::Test::alloc(&really_want_upgrade_filter),
									AIBehaviors::Find::alloc(ControlPoint::family, 4, &default_filter)
								),
								AIBehaviors::Find::alloc(ControlPoint::family, 2, &enemy_control_point_filter),
								AIBehaviors::Find::alloc(MinionCommon::family, 2, &minion_filter),
								AIBehaviors::Find::alloc(Sensor::family, 2, &default_filter)
							),
							Select::alloc
							(
								AIBehaviors::Find::alloc(Awk::family, 2, &awk_find_filter),
								Sequence::alloc
								(
									AIBehaviors::HasUpgrade::alloc(Upgrade::Sensor),
									AIBehaviors::Find::alloc(AICue::family, 2, &aicue_sensor_filter)
								),
								AIBehaviors::RandomPath::alloc(1),
								AIBehaviors::Panic::alloc(1)
							)
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
	update_component_memory<EnergyPickup>(this, &default_memory_filter);
	update_component_memory<MinionCommon>(this, &minion_memory_filter);
	update_component_memory<Sensor>(this, &sensor_memory_filter);
	update_component_memory<AICue>(this, &default_memory_filter);
	update_component_memory<ControlPoint>(this, &default_memory_filter);
	update_component_memory<ContainmentField>(this, &default_memory_filter);

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

b8 AIPlayerControl::snipe_stop()
{
	if (get<Awk>()->current_ability == Ability::Sniper)
		get<Awk>()->current_ability = Ability::None;
	return true; // this returns true so we can call this from an Execute behavior
}

void AIPlayerControl::update(const Update& u)
{
	if (get<Awk>()->state() == Awk::State::Crawl && !Team::game_over)
	{
		const AIPlayer::Config& config = player.ref()->config;

		// new random look direction
		if ((s32)(u.time.total * config.aim_speed * 0.3f) != (s32)((u.time.total - u.time.delta) * config.aim_speed * 0.3f))
			random_look = get<PlayerCommon>()->attach_quat * (Quat::euler(PI + (mersenne::randf_co() - 0.5f) * PI * 1.2f, (PI * 0.5f) + (mersenne::randf_co() - 0.5f) * PI * 1.2f, 0) * Vec3(1, 0, 0));

		if (target.ref())
		{
			if (target.ref()->has<Target>())
			{
				// trying to a hit a moving thingy
				Vec3 intersection;
				if (aim_timer < config.aim_timeout && get<Awk>()->can_hit(target.ref()->get<Target>(), &intersection))
					aim_and_shoot_target(u, intersection, target.ref()->get<Target>());
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
					Vec3 target_pos;
					Quat target_rot;
					target.ref()->get<Transform>()->absolute(&target_pos, &target_rot);
					AI::AwkPathNode target;
					target.crawl = false;
					target.pos = target_pos;
					target.normal = target_rot * Vec3(0, 0, 1);
					target.ref = AWK_NAV_MESH_NODE_NONE;
					go(u, target, target, CONTROL_POINT_RADIUS); // assume the target is a control point
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
				go(u, path[path_index - 1], path[path_index], AWK_RADIUS); // path_index starts at 1 so we're good here
		}
		else
		{
			// look randomly
			aim(u, random_look);

			if (panic)
			{
				// pathfinding routines failed; we are stuck
				PlayerCommon* common = get<PlayerCommon>();
				if (common->movement_enabled())
				{
					// cooldown is done; we can shoot.
					Vec3 look_dir = common->look_dir();
					get<Awk>()->crawl(look_dir, u);
					if (get<Awk>()->can_shoot(look_dir))
					{
						if (get<Awk>()->detach(look_dir))
							active_behavior->done(true);
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
					// the only other kind of target we can have is a control point
					if ((target.ref()->get<Transform>()->absolute_pos() - get<Transform>()->absolute_pos()).length_squared() < CONTROL_POINT_RADIUS * CONTROL_POINT_RADIUS)
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
	camera->range = get<Awk>()->range();
	Vec3 abs_wall_normal = ((get<Transform>()->absolute_rot() * get<Awk>()->lerped_rotation) * Vec3(0, 0, 1));;
	const r32 third_person_offset = 2.0f;
	camera->pos = get<Awk>()->center_lerped() + camera->rot * Vec3(0, 0, -third_person_offset);
	if (get<Transform>()->parent.ref())
	{
		camera->pos += abs_wall_normal * 0.5f;
		camera->pos.y += 0.5f - vi_min((r32)fabs(abs_wall_normal.y), 0.5f);
	}
	Quat inverse_rot = camera->rot.inverse();
	camera->wall_normal = inverse_rot * abs_wall_normal;
	camera->range_center = inverse_rot * (get<Awk>()->center_lerped() - camera->pos);
	camera->cull_range = third_person_offset + 0.5f;
	camera->cull_behind_wall = abs_wall_normal.dot(camera->pos - get<Awk>()->center_lerped()) < 0.0f;
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
				if (memory[i].entity.ref() && filter(control, memory[i].entity.ref()))
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
	path_priority = priority;
}

void RandomPath::run()
{
	active(true);
	if (control->get<Awk>()->state() == Awk::State::Crawl && path_priority > control->path_priority)
	{
		Vec3 pos;
		Quat rot;
		control->get<Transform>()->absolute(&pos, &rot);
		AI::AwkAllow rule;
		if (control->get<Awk>()->current_ability == Ability::Sniper)
			rule = AI::AwkAllow::Crawl;
		else
			rule = AI::AwkAllow::All;
		AI::awk_random_path(rule, control->get<AIAgent>()->team, pos, rot * Vec3(0, 0, 1), ObjectLinkEntryArg<Base<RandomPath>, const AI::AwkResult&, &Base<RandomPath>::path_callback>(id()));
	}
	else
		done(false);
}

Chance::Chance(r32 odds)
	: odds(odds)
{
}

void Chance::run()
{
	active(true);
	done(mersenne::randf_co() < odds);
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
	path_priority = priority;
}

void Panic::abort()
{
	control->panic = false;
	Base<Panic>::abort();
}

void Panic::done(b8 success)
{
	if (success)
		control->panic = false;
	Base<Panic>::done(success);
}

// pathfinding routines failed; we are stuck
void Panic::run()
{
	active(true);
	if (!control->panic && path_priority > control->path_priority)
	{
		control->panic = true;
		control->snipe_stop();
		control->behavior_start(this, 127); // if we're panicking, nothing can interrupt us
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

AbilitySpawn::AbilitySpawn()
	: ability(Ability::None)
{
	path_priority = 0;
}

b8 AbilitySpawn::try_spawn(s8 priority, Upgrade required_upgrade, Ability a, AbilitySpawnFilter filter)
{
	PlayerManager* manager = control->player.ref()->manager.ref();
	const AbilityInfo& info = AbilityInfo::list[(s32)Ability::Sensor];
	if (priority > control->path_priority
		&& control->get<Awk>()->state() == Awk::State::Crawl
		&& manager->has_upgrade(required_upgrade)
		&& manager->credits > info.spawn_cost
		&& filter(control))
	{
		Vec3 pos;
		Vec3 normal;
		if (!control->get<Awk>()->can_spawn(a, control->get<PlayerCommon>()->look_dir(), &pos, &normal))
			return false;
		if (manager->ability_spawn(a, pos, Quat::look(normal)))
		{
			ability = a;
			path_priority = priority;
			return true;
		}
	}
	return false;
}

void AbilitySpawn::run()
{
	active(true);

	if (try_spawn(5, Upgrade::Teleporter, Ability::Teleporter, &should_teleport))
		done(true);
	else if (try_spawn(4, Upgrade::Minion, Ability::Minion, &should_spawn_minion))
		done(true);
	else if (try_spawn(4, Upgrade::Sensor, Ability::Sensor, &should_spawn_sensor))
		done(true);
	else if (try_spawn(4, Upgrade::Rocket, Ability::Rocket, &should_spawn_rocket))
		done(true);
	else if (try_spawn(4, Upgrade::ContainmentField, Ability::ContainmentField, &should_spawn_containment_field))
		done(true);
	else if (try_spawn(4, Upgrade::Sniper, Ability::Sniper, &should_snipe))
		done(true);
	else
		done(false);
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
		r32 range = control->get<Awk>()->range();
		r32 closest_distance = range * range;
		Vec3 pos = control->get<Transform>()->absolute_pos();
		const AIPlayerControl::MemoryArray& memory = control->memory[family];
		for (s32 i = 0; i < memory.length; i++)
		{
			r32 distance = (memory[i].pos - pos).length_squared();
			if (distance < closest_distance)
			{
				if (!control->in_range(memory[i].pos, range) || (memory[i].entity.ref() && filter(control, memory[i].entity.ref())))
				{
					closest_distance = distance;
					closest = memory[i].entity.ref();
				}
			}
		}

		if (closest)
		{
			if (can_react && control->get<Awk>()->can_hit(closest->get<Target>()))
			{
				control->behavior_start(this, react_priority);
				control->set_target(closest);
				return;
			}
			else if (can_path)
			{
				pathfind(closest->get<Target>()->absolute_pos(), Vec3::zero, AI::AwkPathfind::Target, control->get<Awk>()->current_ability == Ability::Sniper ? AI::AwkAllow::Crawl : AI::AwkAllow::All);
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
		&& path_priority > control->path_priority)
	{
		Entity* closest = nullptr;
		r32 range = control->get<Awk>()->range();
		r32 closest_distance = range * range;
		const AIPlayerControl::MemoryArray& memory = control->memory[family];
		for (s32 i = 0; i < memory.length; i++)
		{
			r32 distance = (memory[i].pos - pos).length_squared();
			if (distance < closest_distance)
			{
				if (control->in_range(memory[i].pos, range)
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

Test::Test(b8(*f)(const AIPlayerControl*))
	: filter(f)
{
}

void Test::run()
{
	active(true);
	done(filter(control));
}

ReactControlPoint::ReactControlPoint(s8 priority, b8(*f)(const AIPlayerControl*, const Entity*))
{
	path_priority = priority;
	filter = f;
}

void ReactControlPoint::run()
{
	active(true);
	if (path_priority > control->path_priority)
	{
		Vec3 me = control->get<Transform>()->absolute_pos();
		r32 closest_distance;
		ControlPoint* control_point = ControlPoint::closest(AI::TeamNone, me, &closest_distance);
		if (control_point && filter(control, control_point->entity()))
		{
			if (closest_distance < CONTROL_POINT_RADIUS)
			{
				done(true);
				return;
			}
			if (closest_distance < AWK_MAX_DISTANCE)
			{
				Vec3 target = control_point->get<Transform>()->absolute_pos();
				Vec3 hit;
				if (control->get<Awk>()->can_shoot(target - me, &hit))
				{
					if ((hit - target).length_squared() < CONTROL_POINT_RADIUS * CONTROL_POINT_RADIUS)
					{
						control->behavior_start(this, path_priority);
						control->set_target(control_point->entity());
						return;
					}
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
		if (manager->at_upgrade_point())
		{
			Upgrade u = want_available_upgrade(control);
			if (u != Upgrade::None
				&& manager->current_upgrade == Upgrade::None
				&& manager->upgrade_start(u)
				)
			{
				control->behavior_start(this, 127); // set the priority higher than everything else; upgrades can't be cancelled
				return;
			}
		}
	}
	done(false);
}

CaptureControlPoint::CaptureControlPoint(s8 priority)
{
	path_priority = priority;
}

void CaptureControlPoint::set_context(void* ctx)
{
	Base::set_context(ctx);
	control->player.ref()->manager.ref()->control_point_capture_completed.link<CaptureControlPoint, b8, &CaptureControlPoint::completed>(this);
}

void CaptureControlPoint::completed(b8 success)
{
	if (active())
	{
#if DEBUG_AI_CONTROL
		vi_debug("Control point captured: %s", success ? "succ" : "fail");
#endif
		done(success);
	}
}

void CaptureControlPoint::run()
{
	active(true);
	if (path_priority > control->path_priority)
	{
		PlayerManager* manager = control->player.ref()->manager.ref();
		ControlPoint* control_point = manager->at_control_point();
		if (control_point) 
		{
			if (manager->friendly_control_point(control_point))
			{
				done(true); // already captured
				return;
			}
			else if (manager->capture_start())
			{
				control->behavior_start(this, 127); // can't be canceled
				return;
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
