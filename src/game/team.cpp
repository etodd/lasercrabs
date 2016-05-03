#include "team.h"
#include "game.h"
#include "data/components.h"
#include "entities.h"
#include "data/animator.h"
#include "asset/animation.h"
#include "asset/mesh.h"
#include "strings.h"
#include "awk.h"
#include "minion.h"
#include "bullet/src/BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "audio.h"
#include "asset/Wwise_IDs.h"
#include "asset/level.h"
#include "walker.h"

#if DEBUG
#define PLAYER_SPAWN_DELAY 1.0f
#else
#define PLAYER_SPAWN_DELAY 5.0f
#endif

#define CREDITS_FLASH_TIME 0.5f

namespace VI
{


const Vec4 Team::color_friend = Vec4(0.05f, 0.45f, 0.6f, MATERIAL_NO_OVERRIDE);
const Vec4 Team::color_enemy = Vec4(1.0f, 0.5f, 0.6f, MATERIAL_NO_OVERRIDE);

const Vec4 Team::ui_color_friend = Vec4(0.4f, 0.9f, 1.0f, 1);
const Vec4 Team::ui_color_enemy = Vec4(1.0f, 0.5f, 0.5f, 1);

StaticArray<Team, (s32)AI::Team::count> Team::list;
r32 Team::control_point_timer;

AbilityInfo AbilityInfo::list[] =
{
	{ Asset::Mesh::icon_sensor, 2.5f, 10, 2, { 50, 50 } },
	{ Asset::Mesh::icon_teleporter, 2.5f, 10, 2, { 50, 50 } },
	{ Asset::Mesh::icon_minion, 1.5f, 5, 2, { 50, 100 } },
};

#define GAME_OVER_TIME 5.0f

Team::Team()
	: victory_timer(GAME_OVER_TIME),
	player_tracks(),
	player_track_history(),
	stealth_enable(true),
	player_spawn()
{
}

void Team::awake()
{
	for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team.ref() != this)
			extract_history(i.item(), &player_track_history[i.index]);
	}
}

s32 teams_with_players()
{
	s32 t = 0;
	for (s32 i = 0; i < Team::list.length; i++)
	{
		if (Team::list[i].has_player())
			t++;
	}
	return t;
}

b8 Team::game_over()
{
	if (NoclipControl::list.count() > 0)
		return false;

	if (!PlayerManager::all_ready())
		return false;

	if (Game::time.total > GAME_TIME_LIMIT)
		return true;

	if (PlayerManager::list.count() == 1)
		return false;

	for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->spawn_timer > 0.0f)
			return false;
	}

	return teams_with_players() < 2;
}

b8 Team::is_draw()
{
	return Game::time.total > GAME_TIME_LIMIT && teams_with_players() != 1;
}

b8 Team::has_player() const
{
	for (auto j = PlayerManager::list.iterator(); !j.is_last(); j.next())
	{
		if (j.item()->team.ref()->team() == team() && j.item()->entity.ref())
			return true;
	}
	return false;
}

void Team::extract_history(PlayerManager* manager, SensorTrackHistory* history)
{
	if (manager->entity.ref())
	{
		Health* health = manager->entity.ref()->get<Health>();
		history->hp = health->hp;
		history->hp_max = health->hp_max;
		history->pos = manager->entity.ref()->get<Transform>()->absolute_pos();
	}
	else
	{
		// initial health
		history->hp = 1;
		history->hp_max = AWK_HEALTH;
		history->pos = manager->team.ref()->player_spawn.ref()->absolute_pos();
	}
}

void level_retry()
{
	if (Game::state.local_multiplayer)
		Menu::transition(Game::state.level, Game::Mode::Pvp);
	else
		Menu::transition(Game::state.level, Game::Mode::Parkour);
}

void level_next()
{
	Game::Mode next_mode;

	if (Game::state.local_multiplayer)
	{
		// we're in local multiplayer mode
		next_mode = Game::Mode::Pvp;

		if (Game::state.local_multiplayer_offset == (s32)AI::Team::count - 1)
			Game::save.level_index++; // advance to next level

		Game::state.local_multiplayer_offset = (Game::state.local_multiplayer_offset + 1) % (s32)AI::Team::count;
	}
	else
	{
		// advance to next level
		next_mode = Game::Mode::Parkour;
		Game::save.level_index++; // advance to next level
	}

	AssetID next_level = Game::levels[Game::save.level_index];
	if (next_level == AssetNull) // done; go to main menu
		Menu::transition(Asset::Level::title, Game::Mode::Special);
	else
		Menu::transition(next_level, next_mode);
}

void Team::update_all(const Update& u)
{
	if (Game::state.mode != Game::Mode::Pvp)
		return;

	// determine which Awks are seen by which teams
	Sensor* visibility[MAX_PLAYERS][(s32)AI::Team::count] = {};
	for (auto player = PlayerManager::list.iterator(); !player.is_last(); player.next())
	{
		Entity* player_entity = player.item()->entity.ref();
		if (!player_entity)
			continue;

		AI::Team player_team = player.item()->team.ref()->team();
		Quat player_rot;
		Vec3 player_pos;
		player_entity->get<Transform>()->absolute(&player_pos, &player_rot);
		player_pos += player_rot * Vec3(0, 0, -AWK_RADIUS);
		for (auto sensor = Sensor::list.iterator(); !sensor.is_last(); sensor.next())
		{
			if (!player_entity->get<AIAgent>()->stealth || sensor.item()->team == player_team)
			{
				Sensor** sensor_visibility = &visibility[player.index][(s32)sensor.item()->team];
				if (!(*sensor_visibility))
				{
					if (sensor.item()->get<PlayerTrigger>()->is_triggered(player_entity))
					{
						if (player_entity->has<Awk>() && player_entity->get<Transform>()->parent.ref())
						{
							// we're on a wall; make sure the wall is facing the sensor
							Vec3 to_sensor = sensor.item()->get<Transform>()->absolute_pos() - player_pos;
							if (to_sensor.dot(player_rot * Vec3(0, 0, 1)) > 0.0f)
								*sensor_visibility = sensor.item();
						}
					}
				}
			}
		}
	}

	// update player visibility
	{
		for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
		{
			Entity* i_entity = i.item()->entity.ref();
			if (!i_entity)
				continue;

			AI::Team team = i.item()->team.ref()->team();

			{
				// if stealth is enabled for this team,
				// and if we are within range of their own sensors
				// and not detected by enemy sensors
				// then we should be stealthed
				b8 stealth_enabled = true;
				if (!i.item()->team.ref()->stealth_enable)
					stealth_enabled = false;
				else if (!visibility[i.index][(s32)team])
					stealth_enabled = false;
				else
				{
					for (s32 t = 0; t < Team::list.length; t++)
					{
						if ((AI::Team)t != team && visibility[i.index][t])
						{
							stealth_enabled = false; // visible to enemy sensors
							break;
						}
					}
				}
				i_entity->get<Awk>()->stealth(stealth_enabled);
			}

			auto j = i;
			j.next();
			for (j; !j.is_last(); j.next())
			{
				Entity* j_entity = j.item()->entity.ref();
				if (!j_entity)
					continue;

				if (team == j.item()->team.ref()->team())
					continue;

				b8 visible;
				{
					Vec3 start = i_entity->has<MinionCommon>() ? i_entity->get<MinionCommon>()->head_pos() : i_entity->get<Awk>()->center();
					Vec3 end = j_entity->has<MinionCommon>() ? j_entity->get<MinionCommon>()->head_pos() : j_entity->get<Awk>()->center();
					Vec3 diff = end - start;

					if (btVector3(diff).fuzzyZero())
						visible = true;
					else
					{
						if (diff.length_squared() > AWK_MAX_DISTANCE * AWK_MAX_DISTANCE)
							visible = false;
						else
						{
							btCollisionWorld::ClosestRayResultCallback rayCallback(start, end);
							rayCallback.m_flags = btTriangleRaycastCallback::EFlags::kF_FilterBackfaces
								| btTriangleRaycastCallback::EFlags::kF_KeepUnflippedNormal;
							rayCallback.m_collisionFilterMask = rayCallback.m_collisionFilterGroup = btBroadphaseProxy::StaticFilter | CollisionInaccessible;
							Physics::btWorld->rayTest(start, end, rayCallback);
							visible = !rayCallback.hasHit();
						}
					}
				}

				b8 i_can_see_j = visible && !j_entity->get<AIAgent>()->stealth;
				b8 j_can_see_i = visible && !i_entity->get<AIAgent>()->stealth;
				PlayerCommon::visibility.set(PlayerCommon::visibility_hash(i_entity->get<PlayerCommon>(), j_entity->get<PlayerCommon>()), i_can_see_j);
				PlayerCommon::visibility.set(PlayerCommon::visibility_hash(j_entity->get<PlayerCommon>(), i_entity->get<PlayerCommon>()), j_can_see_i);

				// update history
				Team* i_team = i.item()->team.ref();
				Team* j_team = j.item()->team.ref();
				if (i_can_see_j || i_team->player_tracks[j.index].tracking) 
					extract_history(j.item(), &i_team->player_track_history[j.index]);
				if (j_can_see_i || j_team->player_tracks[i.index].tracking)
					extract_history(i.item(), &j_team->player_track_history[i.index]);
			}
		}
	}

	b8 game_over = Team::game_over();
	b8 draw = Team::is_draw();
	if (game_over && draw)
	{
		if (Game::time.total > GAME_TIME_LIMIT + GAME_OVER_TIME)
			level_retry(); // it's a draw; try again
	}

	for (s32 team_id = 0; team_id < list.length; team_id++)
	{
		Team* team = &list[team_id];

		if (team->has_player() && game_over)
		{
			// we win
			team->victory_timer -= u.time.delta;
			if (team->victory_timer < 0.0f)
				level_next();
		}

		// update tracking timers

		for (auto player = PlayerManager::list.iterator(); !player.is_last(); player.next())
		{
			Entity* player_entity = player.item()->entity.ref();

			AI::Team player_team = player.item()->team.ref()->team();
			if (team->team() == player_team)
				continue;

			Sensor* sensor = visibility[player.index][team->id()];
			SensorTrack* track = &team->player_tracks[player.index];
			track->visible = sensor != nullptr;
			if (sensor)
			{
				// team's sensors are picking up the Awk
				// if this team is already tracking the Awk, increment the timer
				// if not, add the Awk to the tracking list

				if (track->entity.ref() == player_entity)
				{
					// already tracking
					if (track->tracking) // already alerted
						track->timer = SENSOR_TIMEOUT;
					else
					{
						// tracking but not yet alerted
						track->timer += u.time.delta;
						if (track->timer >= SENSOR_TIME)
						{
							if (sensor->player_manager.ref())
							{
								Entity* player_entity = sensor->player_manager.ref()->entity.ref();
								if (player_entity && player_entity->has<LocalPlayerControl>())
									player_entity->get<LocalPlayerControl>()->player.ref()->msg(_(strings::enemy_detected), true);
								sensor->player_manager.ref()->add_credits(CREDITS_DETECT);
							}
							track->tracking = true; // got em
						}
					}
					break;
				}
				else if (player_entity->get<Transform>()->parent.ref())
				{
					// not tracking yet; insert new track entry
					// (only start tracking if the Awk is attached to a wall; don't start tracking if Awk is mid-air)

					new (track) SensorTrack();
					track->entity = player_entity;
				}
			}
			else
			{
				// team's sensors don't see the Awk
				// remove the Awk's track, if any
				if (track->entity.ref() == player_entity)
				{
					if (track->tracking && track->timer > 0.0f) // track remains active for SENSOR_TIMEOUT seconds
						track->timer -= u.time.delta;
					else
					{
						// done tracking
						track->entity = nullptr;
						track->tracking = false;
					}
				}
			}
		}
	}
}

void PlayerManager::ability_spawn_start(Ability ability)
{
	Entity* awk = entity.ref();
	if (!awk)
		return;

	if (at_spawn())
	{
		// we're upgrading this ability
		if (credits >= ability_upgrade_cost(ability))
		{
			current_spawn_ability = ability;
			spawn_ability_timer = ABILITY_UPGRADE_TIME;
		}
		return;
	}

	const AbilityInfo& info = AbilityInfo::list[(s32)ability];
	if (credits < info.spawn_cost)
		return;

	if (ability_level[(s32)ability] == 0)
		return;

	// need to be sitting on some kind of surface
	if (!awk->get<Transform>()->parent.ref())
		return;

	current_spawn_ability = ability;
	spawn_ability_timer = info.spawn_time;
}

void PlayerManager::ability_spawn_stop(Ability ability)
{
	if (current_spawn_ability == ability)
	{
		current_spawn_ability = Ability::None;
		r32 timer = spawn_ability_timer;
		spawn_ability_timer = 0.0f;

		const AbilityInfo& info = AbilityInfo::list[(s32)ability];

		if (timer > info.spawn_time - ABILITY_USE_TIME && !at_spawn())
			ability_use(ability);
	}
}

void PlayerManager::ability_spawn_complete()
{
	Ability ability = current_spawn_ability;
	current_spawn_ability = Ability::None;

	Entity* awk = entity.ref();
	if (!awk)
		return;

	if (at_spawn())
	{
		// we're upgrading this ability
		ability_upgrade(ability);
		return;
	}

	u16 cost = AbilityInfo::list[(s32)ability].spawn_cost;
	if (credits < cost)
		return;

	const u8 level = ability_level[(s32)ability];
	switch (ability)
	{
		case Ability::Sensor:
		{
			if (awk->get<Transform>()->parent.ref())
			{
				add_credits(-cost);

				// place a proximity sensor
				Vec3 abs_pos;
				Quat abs_rot;
				awk->get<Transform>()->absolute(&abs_pos, &abs_rot);

				Entity* sensor = World::create<SensorEntity>(this, abs_pos + abs_rot * Vec3(0, 0, -AWK_RADIUS + rope_segment_length + SENSOR_RADIUS), abs_rot);

				Audio::post_global_event(AK::EVENTS::PLAY_SENSOR_SPAWN, abs_pos);

				// attach it to the wall
				Rope* rope = Rope::start(awk->get<Transform>()->parent.ref()->get<RigidBody>(), abs_pos + abs_rot * Vec3(0, 0, -AWK_RADIUS), abs_rot * Vec3(0, 0, 1), abs_rot);
				rope->end(abs_pos + abs_rot * Vec3(0, 0, -AWK_RADIUS + rope_segment_length), abs_rot * Vec3(0, 0, -1), sensor->get<RigidBody>());
			}
			break;
		}
		case Ability::Teleporter:
		{
			if (awk->get<Transform>()->parent.ref())
			{
				add_credits(-cost);

				// spawn a teleporter
				Quat rot = awk->get<Transform>()->absolute_rot();
				Vec3 pos = awk->get<Transform>()->absolute_pos() + rot * Vec3(0, 0, -AWK_RADIUS);
				World::create<TeleporterEntity>(pos, rot, team.ref()->team());
			}
			break;
		}
		case Ability::Minion:
		{
			if (awk->get<Transform>()->parent.ref())
			{
				add_credits(-cost);

				// spawn a minion
				Vec3 pos;
				Quat rot;
				awk->get<Transform>()->absolute(&pos, &rot);
				pos += rot * Vec3(0, 0, 1.0f);
				Entity* minion = World::create<Minion>(pos, Quat::euler(0, awk->get<PlayerCommon>()->angle_horizontal, 0), team.ref()->team(), this);

				Audio::post_global_event(AK::EVENTS::PLAY_MINION_SPAWN, pos);
			}
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}
}

b8 PlayerManager::ability_use(Ability ability)
{
	Entity* awk = entity.ref();
	if (!awk)
		return false;

	const u8 level = ability_level[(s32)ability];

	u16 cost = AbilityInfo::list[(s32)ability].use_cost;
	if (credits < cost)
		return false;

	switch (ability)
	{
		case Ability::Sensor:
		{
			// todo: TBD
			return true;
			break;
		}
		case Ability::Teleporter:
		{
			AI::Team t = team.ref()->team();
			r32 closest_dot = 0.8f;
			Teleporter* closest = nullptr;

			Vec3 me = awk->get<Transform>()->absolute_pos();
			Vec3 look_dir = awk->get<PlayerCommon>()->look_dir();

			for (auto teleporter = Teleporter::list.iterator(); !teleporter.is_last(); teleporter.next())
			{
				if (teleporter.item()->team == t)
				{
					r32 dot = look_dir.dot(Vec3::normalize(teleporter.item()->get<Transform>()->absolute_pos() - me));
					if (dot > closest_dot)
					{
						closest = teleporter.item();
						closest_dot = dot;
					}
				}
			}

			if (closest)
			{
				// teleport to selected teleporter
				awk->get<Teleportee>()->go(closest);
				add_credits(-cost);
				return true;
			}
			return false;
		}
		case Ability::Minion:
		{
			// summon existing minions to target location

			Vec3 target;
			if (awk->get<Awk>()->can_go(awk->get<PlayerCommon>()->look_dir(), &target))
			{
				AI::Team t = team.ref()->team();

				for (auto i = MinionAI::list.iterator(); !i.is_last(); i.next())
				{
					if (i.item()->get<AIAgent>()->team == t)
						i.item()->find_goal_near(target);
				}
				add_credits(-cost);
				return true;
			}
			return false;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}

	return false;
}

PinArray<PlayerManager, MAX_PLAYERS> PlayerManager::list;

PlayerManager::PlayerManager(Team* team)
	: spawn_timer(PLAYER_SPAWN_DELAY),
	team(team),
	credits(Game::level.has_feature(Game::FeatureLevel::Abilities) ? CREDITS_INITIAL : 0),
	ability_level{ (u8)(Game::level.has_feature(Game::FeatureLevel::Abilities) ? 1 : 0), 0, 0 },
	entity(),
	spawn(),
	ready(Game::state.mode == Game::Mode::Parkour || Game::level.has_feature(Game::FeatureLevel::All)),
	current_spawn_ability(Ability::None)
{
}

b8 PlayerManager::all_ready()
{
	for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
	{
		if (!i.item()->ready)
			return false;
	}
	return true;
}

b8 PlayerManager::ability_upgrade(Ability a)
{
	u8& level = ability_level[(s32)a];
	if (level >= MAX_ABILITY_LEVELS)
		return false;

	const AbilityInfo& info = AbilityInfo::list[(s32)a];
	u16 cost = ability_upgrade_cost(a);
	if (credits < cost)
		return false;

	level += 1;
	add_credits(-cost);
	if (a == Ability::Sensor)
	{
		if (level == 2)
		{
			// disable other teams' stealth
			for (s32 i = 0; i < Team::list.length; i++)
			{
				if (&Team::list[i] != team.ref())
					Team::list[i].stealth_enable = false;
			}
		}
	}
	else if (a == Ability::Teleporter)
	{
		vi_assert(entity.ref());
		if (level == 2)
			entity.ref()->get<PlayerCommon>()->cooldown_multiplier = 1.25f;
	}
	else if (a == Ability::Minion)
	{
		// todo: make minions attack enemy health pickups
	}

	return true;
}

u16 PlayerManager::ability_upgrade_cost(Ability a) const
{
	vi_assert(a != Ability::None);
	const AbilityInfo& info = AbilityInfo::list[(s32)a];
	return info.upgrade_cost[ability_level[(s32)a]];
}

b8 PlayerManager::ability_upgrade_available(Ability a) const
{
	if (a == Ability::None)
	{
		for (s32 i = 0; i < (s32)Ability::count; i++)
		{
			if (ability_upgrade_available((Ability)i) && credits >= ability_upgrade_cost((Ability)i))
				return true;
		}
		return false;
	}
	else
	{
		const AbilityInfo& info = AbilityInfo::list[(s32)a];
		s32 level = ability_level[(s32)a];
		return level < MAX_ABILITY_LEVELS;
	}
}

void PlayerManager::add_credits(u16 c)
{
	if (c != 0)
	{
		vi_assert(c > 0 || credits >= c);
		credits += c;
		credits_flash_timer = CREDITS_FLASH_TIME;
	}
}

b8 PlayerManager::at_spawn() const
{
	if (Game::state.mode == Game::Mode::Pvp)
		return entity.ref() && team.ref()->player_spawn.ref()->get<PlayerTrigger>()->is_triggered(entity.ref());
	else
		return false;
}

void PlayerManager::update(const Update& u)
{
	credits_flash_timer = vi_max(0.0f, credits_flash_timer - Game::real_time.delta);

	if (!entity.ref() && spawn_timer > 0.0f && all_ready() && team.ref()->player_spawn.ref())
	{
		spawn_timer -= u.time.delta;
		if (spawn_timer <= 0.0f)
		{
			spawn.fire();
			if (Game::state.mode == Game::Mode::Parkour)
				spawn_timer = PLAYER_SPAWN_DELAY; // reset timer so we can respawn next time
		}
	}

	if (spawn_ability_timer > 0.0f)
	{
		spawn_ability_timer = vi_max(0.0f, spawn_ability_timer - u.time.delta);

		if (spawn_ability_timer == 0.0f && current_spawn_ability != Ability::None)
			ability_spawn_complete();
	}
}


}