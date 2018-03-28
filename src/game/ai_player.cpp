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
#include "settings.h"

namespace VI
{


#define REEVAL_INTERVAL 0.25f

PinArray<PlayerAI, MAX_PLAYERS> PlayerAI::list;

AI::Config PlayerAI::generate_config(AI::Team team, r32 spawn_time)
{
	AI::Config config;

	config.team = team;
	config.spawn_time = spawn_time;

	config.interval_memory_update = 0.2f;
	config.interval_low_level = 0.25f;
	config.interval_high_level = 0.5f;
	config.inaccuracy_min = PI * 0.01f;
	config.inaccuracy_range = PI * 0.02f;
	config.aim_min_delay = 1.0f;
	config.aim_timeout = 3.0f;
	config.aim_speed = 2.0f;
	
	s32 upgrade_count = 0;
	for (s32 i = 0; i < s32(Upgrade::count); i++)
	{
		Upgrade u = Upgrade(i);
		if ((Game::session.config.ruleset.upgrades_allow & ~Game::session.config.ruleset.upgrades_default) & (1 << s16(u)))
		{
			b8 start_with = false;

			const UpgradeInfo& info = UpgradeInfo::list[s32(u)];
			if (info.type == UpgradeInfo::Type::Ability)
			{
				for (s32 j = 0; j < Game::session.config.ruleset.start_abilities.length; j++)
				{
					if (Game::session.config.ruleset.start_abilities[j] == Ability(u))
					{
						start_with = true;
						break;
					}
				}
			}

			if (!start_with)
			{
				config.upgrade_priorities[upgrade_count] = u;
				upgrade_count++;
			}
		}
	}

	// shuffle
	for (s32 i = 0; i < upgrade_count - 1; i++)
	{
		Upgrade tmp = config.upgrade_priorities[i];
		s32 j = i + mersenne::rand() % (upgrade_count - i);
		config.upgrade_priorities[i] = config.upgrade_priorities[j];
		config.upgrade_priorities[j] = tmp;
	}

	// fill rest of array
	for (s32 i = upgrade_count; i < s32(Upgrade::count); i++)
		config.upgrade_priorities[i] = Upgrade::None;

	return config;
}

PlayerAI::PlayerAI(PlayerManager* m, const AI::Config& config)
	: manager(m),
	revision(),
	config(config),
	spawning()
{
	m->flag(PlayerManager::FlagCanSpawn, true);
	m->spawn.link<PlayerAI, const SpawnPosition&, &PlayerAI::spawn>(this);
}

void PlayerAI::update_server(const Update& u)
{
	if (Team::match_state == Team::MatchState::Done)
	{
		if (Game::real_time.total - Team::game_over_real_time > SCORE_SUMMARY_DELAY + SCORE_SUMMARY_ACCEPT_DELAY
			&& mersenne::randf_co() < u.time.delta / 6.0f)
		{
			// randomly disconnect
			World::remove(manager.ref()->entity());
			PlayerAI::list.remove(id());
		}
	}
}

void ai_player_spawn(const Vec3& pos, const Quat& rot, PlayerAI* player)
{
	// HACK: clear links so we can respawn the entity and have room for more links
	player->manager.ref()->upgrade_completed.entries.length = 0;

	Entity* e = World::create<DroneEntity>(player->manager.ref()->team.ref()->team(), pos);

	e->get<Transform>()->absolute(pos, rot);
	e->get<Drone>()->velocity = rot * Vec3(0, 0, DRONE_FLY_SPEED);

	e->add<PlayerCommon>(player->manager.ref());

	e->add<PlayerControlAI>(player);
	Net::finalize(e);

	ParticleEffect::spawn(ParticleEffect::Type::SpawnDrone, pos, Quat::look(Vec3(0, 1, 0)));
}

void PlayerAI::spawn_callback(const AI::DronePathNode& node)
{
	spawning = false;
	ai_player_spawn(node.pos, Quat::look(-node.normal), this);
}

void PlayerAI::spawn(const SpawnPosition& spawn_pos)
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

PlayerControlAI::Action::Action()
{
	memset(this, 0, sizeof(*this));
}

PlayerControlAI::Action& PlayerControlAI::Action::operator=(const Action& other)
{
	memcpy(this, &other, sizeof(*this));
	return *this;
}

b8 PlayerControlAI::Action::fuzzy_equal(const PlayerControlAI::Action& other) const
{
	if (type == other.type)
	{
		switch (type)
		{
			case TypeNone:
				return true;
			case TypeMove:
				return (pos - other.pos).length_squared() < DRONE_MAX_DISTANCE * 0.1f * DRONE_MAX_DISTANCE * 0.1f;
			case TypeAttack:
				return target.ref() == other.target.ref();
			case TypeUpgrade:
				return upgrade == other.upgrade;
			case TypeAbility:
				return ability == other.ability;
		}
	}
	return false;
}

PlayerControlAI::PlayerControlAI(PlayerAI* p)
	: path_index(),
	player(p),
	path(),
	target(),
	target_shot_at(),
	target_hit(),
	target_active(),
	target_pos(),
	aim_timer(),
	aim_timeout(),
	inaccuracy(),
	action_current(),
	action_queue_key(),
	action_queue(&action_queue_key),
	random_look(0, 0, 1),
	recent_failed_actions(),
	reeval_timer(REEVAL_INTERVAL)
{
#if DEBUG_AI_CONTROL
	camera = Camera::add(0);
#endif
}

void PlayerControlAI::awake()
{
#if DEBUG_AI_CONTROL
	camera.ref()->flag(CameraFlagFog, false);
	camera.ref()->team = s8(get<AIAgent>()->team);
	camera.ref()->mask = 1 << camera.ref()->team;
	camera.ref()->range = DRONE_MAX_DISTANCE;
#endif
	link<&PlayerControlAI::drone_done_flying_or_dashing>(get<Drone>()->done_flying);
	link<&PlayerControlAI::drone_done_flying_or_dashing>(get<Drone>()->done_dashing);
	link_arg<Entity*, &PlayerControlAI::drone_hit>(get<Drone>()->hit);
	link<&PlayerControlAI::drone_detaching>(get<Drone>()->detaching);
	link<&PlayerControlAI::drone_detaching>(get<Drone>()->dashing);
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
	camera.ref()->remove();
#endif
}

void PlayerControlAI::drone_done_flying_or_dashing()
{
	const AI::Config& config = player.ref()->config;
	inaccuracy = config.inaccuracy_min + (mersenne::randf_cc() * config.inaccuracy_range);
	aim_timer = 0.0f;
	aim_timeout = 0.0f;
	if (action_current.type == Action::TypeNone)
		action_done(true); // successfully panicked
	else if (path_index < path.length)
		path_index++;
}

void PlayerControlAI::drone_detaching()
{
	target_hit = false;
	aim_timer = 0.0f;
	aim_timeout = 0.0f;
}

void PlayerControlAI::drone_hit(Entity* e)
{
	target_hit = true;
}

void PlayerControlAI::upgrade_completed(Upgrade upgrade)
{
	action_done(true);
}

Vec2 PlayerControlAI::aim(const Update& u, const Vec3& to_target, r32 inaccuracy)
{
	PlayerCommon* common = get<PlayerCommon>();
	Vec3 wall_normal = get<Drone>()->rotation_clamp();

	const AI::Config& config = player.ref()->config;
	r32 target_angle_horizontal;
	{
		r32 angle = atan2f(to_target.x, to_target.z);
		target_angle_horizontal = LMath::closest_angle(angle + noise::sample2d(Vec2(Game::time.total, 0)) * inaccuracy, common->angle_horizontal);

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
		r32 angle = atan2f(-to_target.y, Vec2(to_target.x, to_target.z).length());
		target_angle_vertical = LMath::closest_angle(angle + noise::sample2d(Vec2(0, Game::time.total)) * inaccuracy, common->angle_vertical);

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

b8 juke_needed(const PlayerControlAI* control)
{
	return !control->get<Drone>()->cooldown_can_shoot() && danger(control) > 1;
}

void juke(PlayerControlAI* control, const Update& u)
{
	// crawl randomly
	r32 angle = (noise::sample2d(Vec2(Game::time.total * 0.5f)) + 1.0f) * PI;
	control->get<Drone>()->crawl(control->get<Transform>()->absolute_rot() * Vec3(cosf(angle), sinf(angle), 0), u.time.delta);
}

void PlayerControlAI::aim_and_shoot_target(const Update& u, const Vec3& target, Target* target_entity)
{
	PlayerCommon* common = get<PlayerCommon>();

	b8 can_move = common->movement_enabled();

	if (action_current.type == Action::TypeAttack && get<Drone>()->current_ability != action_current.ability)
		get<Drone>()->ability(action_current.ability);

	b8 only_crawling_dashing = false;

	if (juke_needed(this))
		juke(this, u);
	else
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
					get<Drone>()->crawl(to_target_crawl, u.time.delta);
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
				get<Drone>()->crawl(to_target_crawl, u.time.delta);

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
		Vec3 wall_normal = get<Drone>()->rotation_clamp();

		Vec2 target_angles = aim(u, to_target, inaccuracy);

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
					get<Drone>()->dash_start(look_dir, target);
			}
			else
			{
				if (lined_up
					&& get<Drone>()->can_shoot(look_dir)
					&& (get<Drone>()->current_ability != Ability::Bolter || get<Drone>()->bolter_can_fire())
					&& get<Drone>()->go(look_dir))
					target_shot_at = true;
			}
		}
	}
}

// if tolerance is greater than 0, we need to land within that distance of the given target point
// returns true as long as it's possible for us to eventually hit the goal
b8 PlayerControlAI::aim_and_shoot_location(const Update& u, const AI::DronePathNode& node_prev, const AI::DronePathNode& node, r32 tolerance)
{
	if (get<Drone>()->current_ability != Ability::None)
		get<Drone>()->ability(Ability::None);

	PlayerCommon* common = get<PlayerCommon>();

	b8 can_move = common->movement_enabled();

	b8 only_crawling_dashing = false;

	Vec3 position_before_crawling = get<Transform>()->absolute_pos();

	if (juke_needed(this))
		juke(this, u);
	else
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

		if (get<Drone>()->current_ability == Ability::None && (node.flag(AI::DronePathNode::FlagCrawledFromParent) || get<Drone>()->direction_is_toward_attached_wall(to_target)))
			only_crawling_dashing = true;

		// crawling
		// if we're shooting for a normal target (health or something), don't crawl
		// except if we're shooting at an enemy Drone and we're on the same surface as them, then crawl
		if (can_move)
		{
			Vec3 wall_normal = get<Drone>()->rotation_clamp();
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
				get<Drone>()->crawl(to_target_crawl, u.time.delta);
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
					get<Drone>()->crawl(to_target_crawl, u.time.delta);

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
					get<Drone>()->crawl(node_prev.pos - get<Transform>()->absolute_pos(), u.time.delta);
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
	if ((pos - position_before_crawling).length_squared() < 0.001f * 0.001f) // if we're still crawling, we may be able to hit it eventually
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
		Vec2 target_angles = aim(u, to_target, 0.0f);

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
					if (!get<Drone>()->dash_start(look_dir, node.pos))
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

void PlayerControlAI::set_path(const AI::DronePath& p)
{
	path = p;
	path_index = 1; // first point is the starting point, should be roughly where we are already
	aim_timer = 0.0f;
	aim_timeout = 0.0f;
}

void PlayerControlAI::action_clear()
{
	s8 action_type = action_current.type;

	active_callback = 0;
	action_current.type = Action::TypeNone;
	action_current.priority = 0;
	path.length = 0;
	path_index = 0;
	target = nullptr;
	target_active = false;
	target_shot_at = false;
	target_hit = false;
	reeval_timer = REEVAL_INTERVAL;
}

b8 want_upgrade(PlayerControlAI* player, Upgrade u)
{
	return player->get<PlayerCommon>()->manager.ref()->upgrade_available(u);
}

typedef void EntityForeach(PlayerControlAI*, const Vec3&, Entity*, void*);

void entity_foreach(PlayerControlAI* player, const Vec3& player_pos, AI::TeamMask mask, EntityForeach* action, void* context)
{
	for (auto i = Drone::list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->get<AIAgent>()->team, mask))
			action(player, player_pos, i.item()->entity(), context);
	}

	for (auto i = Minion::list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->get<AIAgent>()->team, mask))
			action(player, player_pos, i.item()->entity(), context);
	}

	for (auto i = ForceField::list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask))
			action(player, player_pos, i.item()->entity(), context);
	}

	for (auto i = Rectifier::list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask))
			action(player, player_pos, i.item()->entity(), context);
	}

	for (auto i = Turret::list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask))
			action(player, player_pos, i.item()->entity(), context);
	}

	for (auto i = Battery::list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask))
			action(player, player_pos, i.item()->entity(), context);
	}

	for (auto i = MinionSpawner::list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask))
			action(player, player_pos, i.item()->entity(), context);
	}

	for (auto i = Grenade::list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask))
			action(player, player_pos, i.item()->entity(), context);
	}
}

struct ProcessEnemyContext
{
	Vec3 danger_center;
	s32 danger;
	s32 danger_count;
};

Ability attack_ability(PlayerControlAI* player, Entity* target)
{
	PlayerManager* manager = player->get<PlayerCommon>()->manager.ref();
	if (manager->ability_valid(Ability::Bolter) && mersenne::randf_cc() < 0.5f)
		return Ability::Bolter;
	else if (manager->ability_valid(Ability::Sniper) && mersenne::randf_cc() < 0.75f)
		return Ability::Sniper;
	else if (manager->ability_valid(Ability::Shotgun))
		return Ability::Shotgun;
	else if (manager->ability_valid(Ability::Grenade) && mersenne::randf_cc() < 0.5f)
		return Ability::Grenade;
	else
		return Ability::None;
}

void process_enemy(PlayerControlAI* player, const Vec3& player_pos, Entity* target, void* context_in)
{
	ProcessEnemyContext* context = (ProcessEnemyContext*)(context_in);

	Vec3 target_pos = target->get<Transform>()->absolute_pos();
	r32 target_distance = (target_pos - player_pos).length();

	s8 priority = 1;

	Ability ability = attack_ability(player, target);

	if (target->has<Drone>())
	{
		if (target->get<Health>()->can_take_damage(player->entity()))
		{
			s8 shield = target->get<Health>()->shield;
			s8 shield_max = target->get<Health>()->shield_max;
			if (shield > shield_max / 2)
				priority -= 2;
			else
				priority -= 3;
		}
	}
	else if (target->has<Minion>() || target->has<Turret>())
		priority -= 1;
	else if (target->has<Battery>())
	{
		priority -= 3;
		if (Game::session.config.game_type == GameType::Assault)
			priority -= 2;
	}

	if (player->get<Drone>()->can_hit(target->get<Target>()))
	{
		priority -= 1;

		if (target->has<Grenade>())
			priority -= 2;

		if (target->has<Turret>()
			|| target->has<Minion>()
			|| target->has<Drone>()
			|| target->has<Grenade>())
		{
			context->danger_center += target_pos;
			context->danger_count++;
			context->danger += vi_min(1, -priority);
		}
	}
	else if (target_distance > DRONE_MAX_DISTANCE)
		priority += 1;

	PlayerControlAI::Action action;
	action.type = PlayerControlAI::Action::TypeAttack;
	action.ability = ability;
	action.target = target;
	action.priority = priority;
	player->action_queue.push(action);
}

void PlayerControlAI::actions_populate()
{
	PlayerManager* manager = get<PlayerCommon>()->manager.ref();

	AI::Team my_team = get<AIAgent>()->team;

	if (UpgradeStation::drone_at(get<Drone>())) // buy upgrade
	{
		for (s32 i = 0; i < s32(Upgrade::count); i++)
		{
			Upgrade u = player.ref()->config.upgrade_priorities[i];
			if (u == Upgrade::None)
				break;
			else if (want_upgrade(this, u))
			{
				Action action;
				action.type = Action::TypeUpgrade;
				action.upgrade = u;
				action.priority = -3;
				action_queue.push(action);
			}
		}
	}
	else if (manager->ability_count() < MAX_ABILITIES) // go to upgrade station
	{
		for (s32 i = 0; i < s32(Upgrade::count); i++)
		{
			Upgrade u = player.ref()->config.upgrade_priorities[i];
			if (u == Upgrade::None)
				break;
			else if (want_upgrade(this, u))
			{
				UpgradeStation* station = UpgradeStation::closest_available(my_team, get<Transform>()->absolute_pos());
				if (station)
				{
					Action action;
					action.type = Action::TypeMove;
					action.pos = station->get<Transform>()->absolute_pos();
					action.normal = Vec3(0, 1, 0);
					action.priority = 0;
					action_queue.push(action);
				}
			}
		}
	}

	// attack entities and/or run away
	{
		Vec3 pos = get<Transform>()->absolute_pos();

		AI::Team mask = ~(1 << get<AIAgent>()->team);

		ProcessEnemyContext context;
		entity_foreach(this, pos, mask, &process_enemy, &context);
		if (context.danger > 5)
		{
			r32 run_chance = get<Health>()->shield <= 1 ? 0.3f : 0.1f;
			if (context.danger > 20)
				run_chance *= 2.0f;

			if (mersenne::randf_co() < run_chance)
			{
				Action action;
				action.type = Action::TypeRunAway;
				action.pos = context.danger_center / context.danger_count;
				action.priority = -2 - (get<Health>()->shield_max - get<Health>()->shield);
				action_queue.push(action);
			}
		}
	}

	if (Game::session.config.game_type == GameType::CaptureTheFlag)
	{
		AI::Team my_team = get<AIAgent>()->team;
		Vec3 my_pos = get<Transform>()->absolute_pos();
		if (get<Drone>()->flag.ref())
		{
			// carrying enemy flag
			Vec3 my_base_pos(0, 0, FLAG_RADIUS);
			Quat my_base_rot = Quat::identity;
			Team::list[my_team].flag_base.ref()->to_world(&my_base_pos, &my_base_rot);
			if (Flag::for_team(my_team)->at_base
				|| Team::list[my_team].player_count() > 1 && (my_base_pos - my_pos).length_squared() > DRONE_MAX_DISTANCE * 2.0f * DRONE_MAX_DISTANCE * 2.0f)
			{
				// go to base
				Action action;
				action.type = Action::TypeMove;
				action.pos = my_base_pos;
				action.normal = my_base_rot * Vec3(0, 0, 1);
				action.priority = -4;
				action_queue.push(action);
			}
		}

		// if enemy is carrying our flag, kill them
		if (Transform* carrier = Flag::for_team(my_team)->get<Transform>()->parent.ref())
		{
			PlayerControlAI::Action action;
			action.type = PlayerControlAI::Action::TypeAttack;
			action.ability = attack_ability(this, carrier->entity());
			action.target = carrier->entity();
			r32 distance_sq = (my_pos - carrier->absolute_pos()).length_squared();
			action.priority = -4 - (distance_sq < DRONE_MAX_DISTANCE * 2.0f * DRONE_MAX_DISTANCE * 2.0f ? 1 : 0);
			action_queue.push(action);
		}

		{
			// get the enemy flag
			Entity* enemy_flag = Flag::for_team(my_team == 0 ? 1 : 0)->entity();
			if (!enemy_flag->get<Transform>()->parent.ref()) // ignore if it's already being carried by a teammate
			{
				PlayerControlAI::Action action;
				action.type = PlayerControlAI::Action::TypeAttack;
				action.ability = Ability::None;
				action.target = enemy_flag;
				r32 distance_sq = (my_pos - enemy_flag->get<Transform>()->absolute_pos()).length();
				action.priority = -4 - (distance_sq < DRONE_MAX_DISTANCE * 2.0f  * DRONE_MAX_DISTANCE * 2.0f ? 1 : 0);
				action_queue.push(action);
			}
		}
	}

	// last resort: panic
	{
		Action action;
		action.type = Action::TypeNone;
		action.priority = 4096;
		action_queue.push(action);
	}

	for (s32 i = 0; i < recent_failed_actions.length; i++)
	{
		const FailedAction& failed = recent_failed_actions[i];
		for (s32 j = 0; j < action_queue.heap.length; j++)
		{
			if (failed.timestamp > Game::time.total - 3.0f && action_queue.heap[j].fuzzy_equal(failed.action))
			{
				action_queue.remove(j);
				break;
			}
		}
	}
}

void PlayerControlAI::action_done(b8 success)
{
#if DEBUG_AI_CONTROL
	vi_debug("Complete: %s", success ? "success" : "fail");
#endif

	if (action_current.type == Action::TypeUpgrade)
	{
		UpgradeStation* upgrade_station = UpgradeStation::drone_inside(get<Drone>());
		if (upgrade_station)
			upgrade_station->drone_exit();
	}

	if (!success)
	{
		if (recent_failed_actions.length == recent_failed_actions.capacity())
			recent_failed_actions.remove(recent_failed_actions.length - 1);
		recent_failed_actions.insert(0, { Game::time.total, action_current });
	}

	action_clear();
	
	if (success)
		action_queue.clear(); // force calculation of new actions

	if (action_queue.size() == 0) // get new actions
		actions_populate();

	// execute action
	action_execute(action_queue.pop());
}

void PlayerControlAI::action_execute(const Action& a)
{
#if DEBUG_AI_CONTROL
	if (action_current.type == Action::TypeNone)
	{
		vi_debug("Executing action: %d", s32(a.type));
		if (Entity* target = a.target.ref())
		{
			const char* target_type;
			if (target->has<Drone>())
				target_type = "Drone";
			else if (target->has<Minion>())
				target_type = "Minion";
			else if (target->has<ForceField>())
				target_type = "ForceField";
			else if (target->has<Rectifier>())
				target_type = "Rectifier";
			else if (target->has<Turret>())
				target_type = "Turret";
			else if (target->has<Battery>())
				target_type = "Battery";
			else if (target->has<MinionSpawner>())
				target_type = "MinionSpawner";
			else if (target->has<Grenade>())
				target_type = "Grenade";
			else if (target->has<Flag>())
				target_type = "Flag";
			else
				target_type = "Other";
			vi_debug("Target: %s", target_type);
		}
	}
#endif
	action_current = a;

	switch (action_current.type)
	{
		case Action::TypeMove:
		{
			auto callback = ObjectLinkEntryArg<PlayerControlAI, const AI::DroneResult&, &PlayerControlAI::callback_path>(id());
			Vec3 pos;
			Quat rot;
			get<Transform>()->absolute(&pos, &rot);
			active_callback = AI::drone_pathfind(AI::DronePathfind::LongRange, AI::DroneAllow::All, get<AIAgent>()->team, pos, rot * Vec3(0, 0, 1), action_current.pos, action_current.normal, callback);
			break;
		}
		case Action::TypeRunAway:
		{
			auto callback = ObjectLinkEntryArg<PlayerControlAI, const AI::DroneResult&, &PlayerControlAI::callback_path>(id());
			Vec3 pos;
			Quat rot;
			get<Transform>()->absolute(&pos, &rot);
			active_callback = AI::drone_pathfind(AI::DronePathfind::Away, AI::DroneAllow::All, get<AIAgent>()->team, pos, rot * Vec3(0, 0, 1), action_current.pos, Vec3::zero, callback);
			break;
		}
		case Action::TypeUpgrade:
		{
			UpgradeStation* upgrade_station = UpgradeStation::drone_at(get<Drone>());
			if (!upgrade_station)
				action_done(false); // fail
			else
			{
				upgrade_station->drone_enter(get<Drone>());
				if (!get<PlayerCommon>()->manager.ref()->upgrade_start(Upgrade(action_current.upgrade)))
					action_done(false); // fail
			}
			break;
		}
		case Action::TypeAttack:
		{
			if (Entity* attack_target = action_current.target.ref())
			{
				target_active = true;
				target = attack_target;
				target_pos = attack_target->get<Target>()->absolute_pos();
				if (!get<Drone>()->can_hit(attack_target->get<Target>()))
				{
					// pathfind
					Vec3 pos;
					Quat rot;
					get<Transform>()->absolute(&pos, &rot);
					auto callback = ObjectLinkEntryArg<PlayerControlAI, const AI::DroneResult&, &PlayerControlAI::callback_path>(id());
					active_callback = AI::drone_pathfind(AI::DronePathfind::Target, AI::DroneAllow::All, get<AIAgent>()->team, pos, rot * Vec3(0, 0, 1), attack_target->get<Target>()->absolute_pos(), Vec3::zero, callback);
				}
			}
			else
				action_done(false); // fail
			break;
		}
		case Action::TypeNone:
			break; // update method will handle panicking
		// todo: TypeAbility
		default:
		{
			vi_assert(false);
			break;
		}
	}
}

void PlayerControlAI::callback_path(const AI::DroneResult& result)
{
	if (result.id == active_callback)
	{
		active_callback = 0;
		if (result.path.length == 0)
			action_done(false); // failed
		else
			set_path(result.path);
	}
}

b8 action_can_interrupt(s8 type)
{
	return type != PlayerControlAI::Action::TypeUpgrade;
}

void PlayerControlAI::update_server(const Update& u)
{
	if (get<Drone>()->state() == Drone::State::Crawl && Team::match_state == Team::MatchState::Active)
	{
		const AI::Config& config = player.ref()->config;

		if (action_current.type != Action::TypeNone)
		{
			// reevaulate whether we should interrupt the current action and switch to another
			if (action_can_interrupt(action_current.type))
			{
				reeval_timer -= u.time.delta;
				if (reeval_timer < 0.0f)
				{
					reeval_timer += REEVAL_INTERVAL;

					action_queue.clear();
					actions_populate();

					if (action_queue.peek().fuzzy_equal(action_current))
						action_queue.pop(); // make sure we don't have a duplicate in the action queue

					if (action_queue.peek().priority < action_current.priority) // switch to a new action
					{
						action_clear();
						action_execute(action_queue.pop());
					}
				}
			}

			// current action is waiting for a callback; see if we're done executing it
			if (target_active)
			{
				if (!target.ref())
					action_done(target_shot_at); // target's gone
				else if (target.ref()->has<Target>())
				{
					if ((target.ref()->get<Target>()->absolute_pos() - target_pos).length_squared() > 8.0f * 8.0f)
						action_execute(action_current); // recalculate path

					if (target_shot_at)
					{
						if (get<Drone>()->current_ability == Ability::None)
							action_done(target_hit); // call it success if we hit our target, or if there was nothing to hit
					}
				}
				else
				{
					// the only other kind of target we can have is an upgrade station
					vi_assert(target.ref()->has<UpgradeStation>());
					if ((target.ref()->get<Transform>()->absolute_pos() - get<Transform>()->absolute_pos()).length_squared() < UPGRADE_STATION_RADIUS * UPGRADE_STATION_RADIUS)
						action_done(true);
				}
			}
			else if (path.length > 0)
			{
				// following a path
				if (path_index >= path.length)
					action_done(path.length > 1); // call it success if the path we followed was actually valid
			}
		}

		// new random look direction
		if (s32(u.time.total * config.aim_speed * 0.3f) != s32((u.time.total - u.time.delta) * config.aim_speed * 0.3f))
			random_look = get<Transform>()->absolute_rot() * (Quat::euler(PI + (mersenne::randf_co() - 0.5f) * PI * 1.2f, (PI * 0.5f) + (mersenne::randf_co() - 0.5f) * PI * 1.2f, 0) * Vec3(1, 0, 0));

		b8 follow_path = true;

		if (target_active)
		{
			// trying to a hit a moving thingy
			Vec3 intersection;
			if (aim_timeout < config.aim_timeout
				&& get<Drone>()->can_hit(target.ref()->get<Target>(), &intersection, get<Drone>()->target_prediction_speed()))
			{
				follow_path = false;
				aim_and_shoot_target(u, intersection, target.ref()->get<Target>());
			}
			else if (!active_callback && path_index >= path.length)
				action_done(false); // we can't hit it
		}

		if (follow_path)
		{
			if (get<Drone>()->current_ability != Ability::None)
				get<Drone>()->ability(Ability::None);

			if (path_index < path.length)
			{
				// look at next target
				if (aim_timeout > config.aim_timeout
					|| !aim_and_shoot_location(u, path[path_index - 1], path[path_index], DRONE_RADIUS))
				{
#if DEBUG_AI_CONTROL
					vi_debug("Marking bad Drone adjacency");
#endif
					AI::drone_mark_adjacency_bad(path[path_index - 1].ref, path[path_index].ref);
					action_done(false); // action failed
				}
			}
			else
			{
				// look randomly
				aim(u, random_look, inaccuracy);

				if (action_current.type == Action::TypeNone)
				{
					// pathfinding routines failed; panic
					PlayerCommon* common = get<PlayerCommon>();
					if (common->movement_enabled())
					{
						// cooldown is done; we can shoot.
						Vec3 look_dir = common->look_dir();
						get<Drone>()->crawl(look_dir, u.time.delta);
						if (get<Drone>()->can_shoot(look_dir))
							get<Drone>()->go(look_dir);
					}
				}
			}
		}
	}

#if DEBUG_AI_CONTROL
	// update camera
	s32 player_count = PlayerHuman::list.count() + PlayerAI::list.count();
	Camera::ViewportBlueprint* viewports = Camera::viewport_blueprints[player_count - 1];
	Camera::ViewportBlueprint* blueprint = &viewports[PlayerHuman::count_local() + player.id];

	const DisplayMode& display = Settings::display();
	camera.ref()->viewport =
	{
		Vec2(s32(blueprint->x * r32(display.width)), s32(blueprint->y * r32(display.height))),
		Vec2(s32(blueprint->w * r32(display.width)), s32(blueprint->h * r32(display.height))),
	};
	camera.ref()->perspective(80.0f * PI * 0.5f / 180.0f, 0.02f, Game::level.skybox.far_plane);
	camera.ref()->rot = Quat::euler(0.0f, get<PlayerCommon>()->angle_horizontal, get<PlayerCommon>()->angle_vertical);
	PlayerHuman::camera_setup_drone(get<Drone>(), camera.ref(), nullptr, DRONE_THIRD_PERSON_OFFSET);
#endif
}

const AI::Config& PlayerControlAI::config() const
{
	return player.ref()->config;
}


}
