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
	hp_start(1),
	interval_memory_update(0.1f),
	interval_low_level(0.3f),
	interval_high_level(1.0f),
	inaccuracy_min(PI * 0.002f),
	inaccuracy_range(PI * 0.022f),
	aim_timeout(2.5f),
	aim_speed(2.0f)
{

}

AIPlayer::AIPlayer(PlayerManager* m)
	: manager(m), revision(), config()
{
	m->spawn.link<AIPlayer, &AIPlayer::spawn>(this);
	m->ready = true;
}

void AIPlayer::spawn()
{
	Entity* e = World::create<AwkEntity>(manager.ref()->team.ref()->team());

	e->add<PlayerCommon>(manager.ref());

	e->get<Health>()->set(config.hp_start);

	manager.ref()->entity = e;

	e->add<AIPlayerControl>(this);
	Vec3 pos;
	Quat rot;
	manager.ref()->team.ref()->player_spawn.ref()->absolute(&pos, &rot);
	pos += Vec3(0, 0, PLAYER_SPAWN_RADIUS * 0.5f); // spawn it around the edges
	e->get<Transform>()->absolute(pos, rot);
}

AIPlayerControl::AIPlayerControl(AIPlayer* p)
	: player(p),
	path_index(),
	memory(),
	target_path_callback(),
	path_priority(),
	path(),
	loop_high_level(),
	loop_low_level(),
	loop_low_level_2(),
	loop_memory(),
	target(),
	hit_target()
{
#if DEBUG_AI_CONTROL
	camera = Camera::add();
#endif
}

void AIPlayerControl::awake()
{
#if DEBUG_AI_CONTROL
	camera->fog = false;
	camera->team = (u8)get<AIAgent>()->team;
	camera->mask = 1 << camera->team;
	camera->range = AWK_MAX_DISTANCE;
#endif
	link<&AIPlayerControl::awk_attached>(get<Awk>()->attached);
	link_arg<Entity*, &AIPlayerControl::awk_hit>(get<Awk>()->hit);
	link<&AIPlayerControl::awk_detached>(get<Awk>()->detached);
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

void AIPlayerControl::awk_attached()
{
	const AIPlayer::Config& config = player.ref()->config;
	inaccuracy = config.inaccuracy_min + (mersenne::randf_cc() * config.inaccuracy_range);
	aim_timer = 0.0f;
	if (path_index < path.length)
	{
		if ((path[path_index] - get<Awk>()->center()).length_squared() < (AWK_RADIUS * 2.0f) * (AWK_RADIUS * 2.0f))
			path_index++;
	}
}

void AIPlayerControl::awk_detached()
{
	hit_target = false;
	aim_timer = 0.0f;
}

void AIPlayerControl::awk_hit(Entity* e)
{
	hit_target = true;
}

void AIPlayerControl::set_target(Target* t)
{
	aim_timer = 0.0f;
	target = t;
	hit_target = false;
	path.length = 0;
}

void AIPlayerControl::set_path(const AI::Path& p)
{
	path = p;
	path_index = 0;
	aim_timer = 0.0f;
	target = nullptr;
	hit_target = false;
}

void AIPlayerControl::behavior_start(Behavior* caller, b8 callback, s8 priority)
{
	// if this gets called by either loop_low_level or loop_low_level_2
	// we need to abort the other one and restart it
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

#if DEBUG_AI_CONTROL
	const char* loop;
	if (r == loop_low_level)
		loop = "low-level 1";
	else if (r == loop_low_level_2)
		loop = "low-level 2";
	else
		loop = "high-level";
	vi_debug("Awk %s: %s", loop, typeid(*caller).name());
#endif

	if (callback)
		target_path_callback = caller;
	else
		target_path_callback = nullptr;
	path_priority = priority;
}

void AIPlayerControl::behavior_done(b8 success)
{
#if DEBUG_AI_CONTROL
	vi_debug("Awk behavior done: %d", success);
#endif
	Behavior* cb = target_path_callback;
	target_path_callback = nullptr;
	path_priority = 0;
	path.length = 0;
	target = nullptr;
	if (cb)
		cb->done(success);
}

b8 AIPlayerControl::restore_loops()
{
	// return to normal state
	if (!loop_high_level->active())
		loop_high_level->run();

	if (loop_low_level_2->active())
		loop_low_level_2->abort();

	if (!loop_low_level->active())
		loop_low_level->run();

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

template<typename Component>
void update_component_memory(AIPlayerControl* control, b8 (*filter)(const AIPlayerControl*, const Entity*))
{
	AIPlayerControl::MemoryArray* component_memories = &control->memory[Component::family];
	// remove outdated memories
	for (s32 i = 0; i < component_memories->length; i++)
	{
		AIPlayerControl::Memory* m = &(*component_memories)[i];
		if (control->in_range(m->pos, VISIBLE_RANGE))
		{
			b8 now_in_range = m->entity.ref() && control->in_range(m->entity.ref()->get<Transform>()->absolute_pos(), VISIBLE_RANGE) && filter(control, m->entity.ref());
			if (!now_in_range)
			{
				component_memories->remove(i);
				i--;
			}
		}
	}

	// add new memories
	if (component_memories->length < component_memories->capacity())
	{
		for (auto i = Component::list.iterator(); !i.is_last(); i.next())
		{
			Vec3 pos = i.item()->template get<Transform>()->absolute_pos();
			if (control->in_range(pos, VISIBLE_RANGE) && filter(control, i.item()->entity()))
			{
				add_memory(component_memories, i.item()->entity(), pos);
				if (component_memories->length == component_memories->capacity())
					break;
			}
		}
	}
}

// if exact is true, we need to land exactly at the given target point
b8 AIPlayerControl::aim_and_shoot(const Update& u, const Vec3& target, b8 exact)
{
	PlayerCommon* common = get<PlayerCommon>();

	if (common->cooldown == 0.0f)
		aim_timer += u.time.delta;

	b8 enable_movement = common->movement_enabled();

	// crawling
	if (enable_movement)
	{
		Vec3 pos = get<Awk>()->center();
		Vec3 to_target = Vec3::normalize(target - pos);

		b8 could_go_before_crawling;
		{
			Vec3 hit;
			could_go_before_crawling = get<Awk>()->can_go(to_target, &hit);
			if (could_go_before_crawling)
			{
				// we could go generally toward the target
				if (exact && (hit - target).length() > AWK_RADIUS) // make sure we would actually land at the right spot
					could_go_before_crawling = false;
			}
		}

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
				if (!exact || (hit - target).length() < AWK_RADIUS) // make sure we're actually going to land at the right spot
					revert = false;
			}
		}

		if (revert)
			get<Awk>()->move(old_pos, old_rot, old_parent); // revert the crawling we just did
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

	if (enable_movement && common->cooldown == 0.0f)
	{
		// cooldown is done; we can shoot.
		// check if we're done aiming
		b8 aim_lined_up;
		if (exact)
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
				if (!exact || (hit - target).length() < AWK_RADIUS) // make sure we're actually going to land at the right spot
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
	return !e->get<HealthPickup>()->owner.ref();
}

b8 minion_filter(const AIPlayerControl* control, const Entity* e)
{
	return e->get<AIAgent>()->team != control->get<AIAgent>()->team;
}

b8 awk_filter(const AIPlayerControl* control, const Entity* e)
{
	return e->get<AIAgent>()->team != control->get<AIAgent>()->team
		&& !e->get<AIAgent>()->stealth
		&& e->get<Health>()->hp <= control->get<Health>()->hp;
}

b8 default_filter(const AIPlayerControl* control, const Entity* e)
{
	return true;
}

b8 should_spawn_sensor(const AIPlayerControl* control)
{
	PlayerManager* manager = control->player.ref()->manager.ref();
	const AbilityInfo& info = AbilityInfo::list[(s32)Ability::Sensor];
	if (manager->credits > info.spawn_cost * 2)
	{
		Vec3 me = control->get<Transform>()->absolute_pos();

		r32 closest_friendly_sensor;
		Sensor::closest(control->get<AIAgent>()->team, me, &closest_friendly_sensor);
		if (closest_friendly_sensor > SENSOR_RANGE)
		{
			if (SensorInterestPoint::in_range(me))
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
					Delay::alloc(config.interval_low_level),
					Sequence::alloc
					(
						Succeed::alloc
						(
							Select::alloc // if any of these succeed, they will abort the high level loop
							(
								AIBehaviors::ReactTarget::alloc(Awk::family, 4, 5, &awk_filter),
								Sequence::alloc
								(
									Invert::alloc(Execute::alloc()->method<Health, &Health::is_full>(control->get<Health>())), // make sure we need health
									AIBehaviors::ReactTarget::alloc(HealthPickup::family, 3, 4, &health_pickup_filter)
								),
								AIBehaviors::ReactTarget::alloc(MinionAI::family, 3, 4, &default_filter),
								AIBehaviors::AbilitySpawn::alloc(3, Ability::Sensor, &should_spawn_sensor)
							)
						),
						Execute::alloc()->method<AIPlayerControl, &AIPlayerControl::restore_loops>(control) // restart the high level loop if necessary
					)
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
					Succeed::alloc
					(
						Select::alloc
						(
							Sequence::alloc
							(
								Invert::alloc(Execute::alloc()->method<Health, &Health::is_full>(control->get<Health>())), // make sure we need health
								AIBehaviors::Find::alloc(HealthPickup::family, 2, &health_pickup_filter)
							),
							AIBehaviors::Find::alloc(MinionAI::family, 2, &minion_filter),
							AIBehaviors::Find::alloc(Awk::family, 2, &awk_filter),
							AIBehaviors::RandomPath::alloc()
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

void AIPlayerControl::init_behavior_trees()
{
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

b8 AIPlayerControl::update_memory()
{
	update_component_memory<HealthPickup>(this, &default_filter);
	update_component_memory<MinionAI>(this, &minion_filter);
	update_component_memory<SensorInterestPoint>(this, &default_filter);
	update_component_memory<Awk>(this, &awk_filter);

	// update memory of enemy AWK positions based on team sensor data
	const Team& team = Team::list[(s32)get<AIAgent>()->team];
	for (s32 i = 0; i < MAX_PLAYERS; i++)
	{
		const Team::SensorTrack& track = team.player_tracks[i];
		if (track.tracking && track.entity.ref())
			add_memory(&memory[Awk::family], track.entity.ref(), track.entity.ref()->get<Transform>()->absolute_pos());
	}

	return true;
}

void AIPlayerControl::update(const Update& u)
{
	if (get<Transform>()->parent.ref())
	{
		if (!loop_high_level)
			init_behavior_trees();

		const AIPlayer::Config& config = player.ref()->config;

		if (target.ref())
		{
			Vec3 intersection;
			if (get<Awk>()->can_hit(target.ref(), &intersection))
				aim_and_shoot(u, intersection, false);
			else
				behavior_done(false); // we can't hit it
		}
		else if (path_index < path.length)
		{
			// look at next target
			if (aim_timer > config.aim_timeout)
				behavior_done(false); // we can't hit it
			else
				aim_and_shoot(u, path[path_index], true);
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
		}
	}

	if (target_path_callback && ((!target.ref() && path_index >= path.length) || (target.ref() && hit_target)))
		behavior_done(hit_target || path.length > 0);

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
	if (path_priority < control->path_priority)
	{
		done(false);
		return;
	}

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
		this->pathfind(closest->pos, false);
	else
		this->done(false);
}

RandomPath::RandomPath()
{
	RandomPath::path_priority = 1;
}

void RandomPath::run()
{
	active(true);
	if (control->path_priority < RandomPath::path_priority)
		AI::awk_random_path(control->get<AIAgent>()->team, control->get<Transform>()->absolute_pos(), ObjectLinkEntryArg<Base<RandomPath>, const AI::Result&, &Base<RandomPath>::path_callback>(id()));
	else
		done(false);
}

AbilitySpawn::AbilitySpawn(s8 priority, Ability ability, AbilitySpawnFilter filter)
	: ability(ability), filter(filter)
{
	AbilitySpawn::path_priority = priority;
}

void AbilitySpawn::set_context(void* ctx)
{
	Base::set_context(ctx);
	control->player.ref()->manager.ref()->ability_spawned.link<AbilitySpawn, Ability, &AbilitySpawn::completed>(this);
}

void AbilitySpawn::completed(Ability a)
{
	if (active())
	{
		control->behavior_done(a == ability);
		done(a == ability);
	}
}

void AbilitySpawn::run()
{
	active(true);

	PlayerManager* manager = control->player.ref()->manager.ref();

	if (AbilitySpawn::path_priority > control->path_priority
		&& control->get<Transform>()->parent.ref()
		&& manager->ability_level[(s32)ability] > 0
		&& filter(control))
	{
		if (manager->ability_spawn_start(ability))
			control->behavior_start(this, false, AbilitySpawn::path_priority);
		else
			done(false);
	}
	else
		done(false);
}

void AbilitySpawn::abort()
{
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
	if (can_path || can_react)
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
				control->behavior_start(this, true, react_priority);
				control->set_target(closest->get<Target>());
				return;
			}
			else if (can_path)
			{
				pathfind(closest->get<Target>()->absolute_pos(), true);
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
