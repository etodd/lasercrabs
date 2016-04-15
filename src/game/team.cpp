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

#if DEBUG
#define PLAYER_SPAWN_DELAY 1.0f
#else
#define PLAYER_SPAWN_DELAY 5.0f
#endif

#define CREDITS_FLASH_TIME 0.5f
#define SENSOR_TIME_1 2.0f
#define SENSOR_TIME_2 1.0f
#define CONTROL_POINT_INTERVAL 60.0f

namespace VI
{


const Vec4 Team::colors[(s32)AI::Team::count] =
{
	Vec4(0.05f, 0.45f, 0.6f, 1),
	Vec4(1.0f, 0.5f, 0.6f, 1),
};

const Vec4 Team::ui_colors[(s32)AI::Team::count] =
{
	Vec4(0.4f, 0.9f, 1.0f, 1),
	Vec4(1.0f, 0.5f, 0.5f, 1),
};

StaticArray<Team, (s32)AI::Team::count> Team::list;
b8 Team::abilities_enabled;
r32 Team::control_point_timer;

#define GAME_OVER_TIME 5.0f

Team::Team()
	: victory_timer(GAME_OVER_TIME),
	sensor_time(SENSOR_TIME_1),
	sensor_explode(),
	player_tracks(),
	player_track_history()
{
}

void Team::awake()
{
	for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
		extract_history(i.item(), &player_track_history[i.index]);
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

	if (Game::time.total <= PLAYER_SPAWN_DELAY)
		return false;

	if (Game::time.total > GAME_TIME_LIMIT)
		return true;

	return teams_with_players() < 2;
}

b8 Team::is_draw()
{
	return Game::time.total > GAME_TIME_LIMIT && teams_with_players() >= 2;
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
	history->ability = manager->ability;
	history->ability_level = manager->ability_level[(s32)manager->ability];
	history->credits = manager->credits;
	if (manager->entity.ref())
	{
		Health* health = manager->entity.ref()->get<Health>();
		history->hp = health->hp;
		history->hp_max = health->hp_max;
	}
	else
	{
		// initial health
		history->hp = 1;
		history->hp_max = AWK_HEALTH;
	}
}

void Team::update_all(const Update& u)
{
	if (Game::data.mode != Game::Mode::Pvp)
		return;

	// control points
	control_point_timer -= u.time.delta;
	if (control_point_timer < 0.0f)
	{
		// give points to teams based on how many control points they own
		s32 reward_buffer[(s32)AI::Team::count] = {};
		for (auto i = ControlPoint::list.iterator(); !i.is_last(); i.next())
		{
			Vec3 control_point_pos = i.item()->get<Transform>()->absolute_pos();

			AI::Team control_point_team = AI::Team::None;
			b8 contested = false;
			for (auto j = Sensor::list.iterator(); !j.is_last(); j.next())
			{
				if (j.item()->get<PlayerTrigger>()->contains(control_point_pos))
				{
					AI::Team sensor_team = j.item()->team;
					if (control_point_team == AI::Team::None)
						control_point_team = sensor_team;
					else if (control_point_team != sensor_team)
					{
						contested = true;
						break;
					}
				}
			}

			if (control_point_team != AI::Team::None && !contested)
				reward_buffer[(s32)control_point_team] += CREDITS_CONTROL_POINT;
		}

		// add credits to players
		for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
		{
			s32 reward = reward_buffer[(s32)i.item()->team.ref()->team()];
			i.item()->add_credits(reward);
		}

		control_point_timer = CONTROL_POINT_INTERVAL;
	}

	// determine which Awks are seen by which teams
	Sensor* visibility[MAX_PLAYERS][(s32)AI::Team::count] = {};
	for (auto player = PlayerManager::list.iterator(); !player.is_last(); player.next())
	{
		Entity* player_entity = player.item()->entity.ref();
		if (!player_entity || (player_entity->has<Awk>() && player_entity->get<Awk>()->stealth_timer > 0.0f))
			continue;

		AI::Team player_team = player.item()->team.ref()->team();
		Quat player_rot;
		Vec3 player_pos;
		player_entity->get<Transform>()->absolute(&player_pos, &player_rot);
		for (auto sensor = Sensor::list.iterator(); !sensor.is_last(); sensor.next())
		{
			if (sensor.item()->team != player_team)
			{
				Sensor** sensor_visibility = &visibility[player.index][(s32)sensor.item()->team];
				if (!(*sensor_visibility))
				{
					if (sensor.item()->get<PlayerTrigger>()->is_triggered(player_entity))
					{
						if (player_entity->has<Awk>() && player_entity->get<Transform>()->parent.ref())
						{
							// we're on a wall; make sure the wall is facing the sensor
							Vec3 to_sensor = sensor.item()->get<Transform>()->absolute_pos() - (player_pos + (player_rot * Vec3(0, 0, -AWK_RADIUS)));
							if (to_sensor.dot(player_rot * Vec3(0, 0, 1)) > 0.0f)
								*sensor_visibility = sensor.item();
						}
						else
							*sensor_visibility = sensor.item();
					}
				}
			}
		}
	}

	// update player visibility
	{
		vi_assert(PlayerCommon::list.count() <= MAX_PLAYERS);

		for (auto i = PlayerCommon::list.iterator(); !i.is_last(); i.next())
		{
			AI::Team team = i.item()->manager.ref()->team.ref()->team();
			auto j = i;
			j.next();
			for (j; !j.is_last(); j.next())
			{
				if (team == j.item()->manager.ref()->team.ref()->team())
					continue;
				b8 visible;
				if (i.item()->has<Awk>() && i.item()->get<Awk>()->stealth_timer > 0.0f)
					visible = false;
				else if (j.item()->has<Awk>() && j.item()->get<Awk>()->stealth_timer > 0.0f)
					visible = false;
				else
				{
					Vec3 start = i.item()->has<MinionCommon>() ? i.item()->get<MinionCommon>()->head_pos() : i.item()->get<Awk>()->center();
					Vec3 end = j.item()->has<MinionCommon>() ? j.item()->get<MinionCommon>()->head_pos() : j.item()->get<Awk>()->center();
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

				PlayerCommon::visibility.set(PlayerCommon::visibility_hash(i.item(), j.item()), visible);
				if (visible) // update history
				{
					extract_history(i.item()->manager.ref(), &j.item()->manager.ref()->team.ref()->player_track_history[i.item()->manager.id]);
					extract_history(j.item()->manager.ref(), &i.item()->manager.ref()->team.ref()->player_track_history[j.item()->manager.id]);
				}
			}
		}
	}

	b8 game_over = Team::game_over();
	b8 draw = Team::is_draw();
	if (game_over && draw)
	{
		if (Game::time.total > GAME_TIME_LIMIT + GAME_OVER_TIME)
		{
			// it's a draw; try again
			Menu::transition(Game::data.level, Game::data.mode); // todo: restart the level with a new opponent
		}
	}

	for (s32 team_id = 0; team_id < list.length; team_id++)
	{
		Team* team = &list[team_id];

		if (team->has_player() && game_over)
		{
			// we win
			team->victory_timer -= u.time.delta;
			if (team->victory_timer < 0.0f)
				Menu::transition(Game::data.next_level, Game::data.next_mode);
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
						if (track->timer >= team->sensor_time)
						{
							if (sensor->player_manager.ref())
							{
								Entity* player_entity = sensor->player_manager.ref()->entity.ref();
								if (player_entity && player_entity->has<LocalPlayerControl>())
									player_entity->get<LocalPlayerControl>()->player.ref()->msg(_(strings::enemy_detected), true);
								sensor->player_manager.ref()->add_credits(CREDITS_DETECT);
							}
							if (team->sensor_explode)
							{
								// todo: explode sensor
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
						// copy track to history and erase
						extract_history(player.item(), &team->player_track_history[player.index]);
						track->entity = nullptr;
						track->tracking = false;
					}
				}
			}
		}
	}
}

AbilityInfo AbilityInfo::list[] =
{
	{ Asset::Mesh::icon_sensor, strings::sensor, 15.0f, 3, { 10, 30, 50 } },
	{ Asset::Mesh::icon_stealth, strings::stealth, 30.0f, 3, { 10, 30, 50 } },
	{ Asset::Mesh::icon_skip_cooldown, strings::skip_cooldown, 30.0f, 3, { 10, 30, 50 } },
};

b8 PlayerManager::ability_use()
{
	Entity* awk = entity.ref();
	if (awk && ability_cooldown < ABILITY_COOLDOWN_USABLE_RANGE && ability != Ability::None)
	{
		r32 cooldown_reset = AbilityInfo::list[(s32)ability].cooldown + ABILITY_COOLDOWN_USABLE_RANGE;
		
		const u8 level = ability_level[(s32)ability];

		switch (ability)
		{
			case Ability::Sensor:
			{
				if (awk->get<Transform>()->parent.ref())
				{
					// place a proximity sensor
					Vec3 abs_pos;
					Quat abs_rot;
					awk->get<Transform>()->absolute(&abs_pos, &abs_rot);

					Entity* sensor = World::create<SensorEntity>(awk->get<Transform>()->parent.ref(), this, abs_pos + abs_rot * Vec3(0, 0, rope_segment_length + SENSOR_RADIUS), abs_rot);

					sensor->get<Audio>()->post_event(AK::EVENTS::PLAY_SENSOR_SPAWN);

					// attach it to the wall
					Rope* rope = Rope::start(awk->get<Transform>()->parent.ref()->get<RigidBody>(), abs_pos + abs_rot * Vec3(0, 0, -AWK_RADIUS), abs_rot * Vec3(0, 0, 1), abs_rot);
					rope->end(abs_pos + abs_rot * Vec3(0, 0, rope_segment_length), abs_rot * Vec3(0, 0, -1), sensor->get<RigidBody>());
					
					ability_cooldown = cooldown_reset;
				}
				break;
			}
			case Ability::Stealth:
			{
				r32 time;
				switch (level)
				{
					case 1:
					{
						time = 5.0f;
						break;
					}
					case 2:
					{
						time = 10.0f;
						break;
					}
					case 3:
					{
						time = 15.0f;
						break;
					}
					default:
					{
						vi_assert(false);
						break;
					}
				}
				awk->get<Awk>()->stealth_enable(time);
				ability_cooldown = cooldown_reset;
				break;
			}
			case Ability::SkipCooldown:
			{
				if (awk->get<PlayerCommon>()->cooldown > 0.0f)
				{
					awk->get<PlayerCommon>()->cooldown = 0.0f;
					ability_cooldown = cooldown_reset;
				}

				// detach if we are a local player
				if (awk->has<LocalPlayerControl>())
				{
					Vec3 dir = awk->get<PlayerCommon>()->look_dir();
					if (awk->get<Awk>()->can_go(dir))
						awk->get<LocalPlayerControl>()->detach(dir);
				}
				break;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}

		return true;
	}
	return false;
}

PinArray<PlayerManager, MAX_PLAYERS> PlayerManager::list;

PlayerManager::PlayerManager(Team* team)
	: spawn_timer(PLAYER_SPAWN_DELAY),
	team(team),
	credits(CREDITS_INITIAL),
	ability(Team::abilities_enabled ? Ability::Sensor : Ability::None),
	ability_level{ 1, 0, 0 },
	ability_cooldown(),
	entity(),
	spawn()
{
}

void PlayerManager::ability_switch(Ability a)
{
	if (ability_cooldown == 0.0f)
	{
		if (a == Ability::None)
		{
			// switch to next available ability
			s32 i = (s32)ability;
			while (true)
			{
				i = (i + 1) % (s32)Ability::count;
				if (ability_level[i] > 0)
				{
					ability = (Ability)i;
					break;
				}
			}
		}
		else
		{
			vi_assert(ability_level[(s32)a] > 0);
			ability = a;
		}
	}
}

void PlayerManager::ability_upgrade(Ability a)
{
	u8& level = ability_level[(s32)a];
	const AbilityInfo& info = AbilityInfo::list[(s32)a];
	vi_assert(level < info.max_level);
	u16 cost = ability_upgrade_cost(a);
	vi_assert(credits >= cost);
	level += 1;
	credits -= cost;
	credits_flash_timer = CREDITS_FLASH_TIME;
	if (a == Ability::SkipCooldown)
	{
		vi_assert(entity.ref());
		if (level == 2)
			entity.ref()->get<PlayerCommon>()->cooldown_multiplier = 1.15f;
		else if (level == 3)
			entity.ref()->get<PlayerCommon>()->cooldown_multiplier = 1.3f;
	}
	else if (a == Ability::Sensor)
	{
		if (level == 2)
			team.ref()->sensor_time = SENSOR_TIME_2;
		else if (level == 3)
			team.ref()->sensor_explode = true;
	}
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
		return level < info.max_level;
	}
}

void PlayerManager::add_credits(u16 c)
{
	if (c != 0)
	{
		credits += c;
		credits_flash_timer = CREDITS_FLASH_TIME;
	}
}

b8 PlayerManager::at_spawn() const
{
	if (Game::data.mode == Game::Mode::Pvp)
		return entity.ref() && team.ref()->player_spawn.ref()->get<PlayerTrigger>()->is_triggered(entity.ref());
	else
		return false;
}

void PlayerManager::update(const Update& u)
{
	credits_flash_timer = vi_max(0.0f, credits_flash_timer - Game::real_time.delta);

	if (!entity.ref() && spawn_timer > 0.0f && team.ref()->player_spawn.ref())
	{
		spawn_timer -= u.time.delta;
		if (spawn_timer <= 0.0f)
		{
			spawn.fire();
			if (Game::data.mode == Game::Mode::Parkour)
				spawn_timer = PLAYER_SPAWN_DELAY; // reset timer so we can respawn next time
		}
	}

	if (ability_cooldown > 0.0f)
		ability_cooldown = vi_max(0.0f, ability_cooldown - u.time.delta);
}


}