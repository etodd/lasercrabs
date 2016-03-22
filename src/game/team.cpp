#include "team.h"
#include "game.h"
#include "data/components.h"
#include "entities.h"
#include "data/animator.h"
#include "asset/animation.h"
#include "asset/mesh.h"
#include "strings.h"
#include "awk.h"

#if DEBUG
#define PLAYER_SPAWN_DELAY 1.0f
#else
#define PLAYER_SPAWN_DELAY 5.0f
#endif

#define CREDITS_INITIAL 0

#define SENSOR_TIME_1 2.0f
#define SENSOR_TIME_2 1.0f

namespace VI
{


const Vec4 Team::colors[(s32)AI::Team::count] =
{
	Vec4(0.05f, 0.35f, 0.5f, 1),
	Vec4(0.6f, 0.15f, 0.05f, 1),
};

const Vec4 Team::ui_colors[(s32)AI::Team::count] =
{
	Vec4(0.3f, 0.8f, 1.0f, 1),
	Vec4(1.0f, 0.5f, 0.5f, 1),
};

StaticArray<Team, (s32)AI::Team::count> Team::list;

Team::Team()
	: victory_timer(5.0f),
	sensor_time(SENSOR_TIME_1),
	sensor_explode()
{
}

void Team::awake()
{
}

b8 Team::game_over()
{
	if (NoclipControl::list.count() > 0)
		return false;

	if (Game::time.total <= PLAYER_SPAWN_DELAY)
		return false;

	s32 teams_with_players = 0;
	for (s32 i = 0; i < Team::list.length; i++)
	{
		if (Team::list[i].has_player())
			teams_with_players++;
	}
	return teams_with_players < 2;
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

void Team::update(const Update& u)
{
	if (Game::data.mode != Game::Mode::Multiplayer)
		return;

	// determine which Awks are seen by which teams
	Sensor* visibility[MAX_PLAYERS][(s32)AI::Team::count] = {};
	for (auto player = PlayerCommon::list.iterator(); !player.is_last(); player.next())
	{
		Entity* player_entity = player.item()->entity();
		AI::Team player_team = player_entity->get<AIAgent>()->team;
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
							Vec3 to_sensor = sensor.item()->get<Transform>()->absolute_pos() - player_pos;
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

	for (s32 team_id = 0; team_id < list.length; team_id++)
	{
		Team* team = &list[team_id];

		if (team->has_player() && team->game_over())
		{
			// we win
			team->victory_timer -= u.time.delta;
			if (team->victory_timer < 0.0f)
				Menu::transition(Game::data.next_level);
		}

		// update tracking timers

		for (auto player = PlayerCommon::list.iterator(); !player.is_last(); player.next())
		{
			AI::Team player_team = player.item()->get<AIAgent>()->team;
			if (team->team() == player_team)
				continue;

			Entity* player_entity = player.item()->entity();

			Sensor* sensor = visibility[player.index][team->id()];
			if (sensor)
			{
				// team's sensors are picking up the Awk
				// if this team is already tracking the Awk, increment the timer
				// if not, add the Awk to the tracking list
				b8 already_tracking = false;
				for (s32 k = 0; k < MAX_PLAYERS; k++)
				{
					Team::SensorTrack* track = &team->player_tracks[k];
					if (track->entity.ref() == player_entity)
					{
						// already tracking
						if (track->visible) // already alerted
							track->timer = SENSOR_TIMEOUT;
						else
						{
							// tracking but not yet alerted
							track->timer += u.time.delta;
							if (track->timer >= team->sensor_time)
							{
								if (sensor->player_manager.ref())
									sensor->player_manager.ref()->add_credits(10);
								if (team->sensor_explode)
								{
									// todo: explode sensor
								}
								track->visible = true; // got em
							}
						}

						already_tracking = true;
						break;
					}
				}

				if (!already_tracking && player.item()->get<Transform>()->parent.ref())
				{
					// insert new track entry
					// (only start tracking if the Awk is attached to a wall; don't start tracking if Awk is mid-air)

					Team::SensorTrack* track = &team->player_tracks[player.index];
					new (track) Team::SensorTrack();
					track->entity = player_entity;
				}
			}
			else
			{
				// team's sensors don't see the Awk
				// remove the Awk's tracks, if any
				for (s32 k = 0; k < MAX_PLAYERS; k++)
				{
					Team::SensorTrack* track = &team->player_tracks[k];
					if (track->entity.ref() == player_entity)
					{
						if (track->visible && track->timer > 0.0f) // track remains active for SENSOR_TIMEOUT seconds
							track->timer -= u.time.delta;
						else
						{
							// erase track
							track->entity = nullptr;
							track->visible = false;
						}
						break;
					}
				}
			}
		}
	}
}

AbilityInfo AbilityInfo::list[] =
{
	{ Asset::Mesh::icon_sensor, strings::sensor, 20.0f, 3, { 20, 40, 80 } },
	{ Asset::Mesh::icon_stealth, strings::stealth, 30.0f, 3, { 20, 40, 80 } },
	{ Asset::Mesh::icon_skip_cooldown, strings::skip_cooldown, 30.0f, 3, { 20, 40, 80 } },
};

b8 PlayerManager::ability_use()
{
	Entity* awk = entity.ref();
	if (awk && ability_cooldown == 0.0f)
	{
		r32 cooldown_reset = AbilityInfo::list[(s32)ability].cooldown;
		
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
					abs_pos += abs_rot * Vec3(0, 0, rope_segment_length + 0.25f);
					World::create<SensorEntity>(awk->get<Transform>()->parent.ref(), this, abs_pos, abs_rot);

					Rope::spawn(abs_pos + abs_rot * Vec3(0, 0, -0.25f), abs_rot * Vec3(0, 0, -1), 1.0f); // attach it to the wall
					
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
					Vec3 dir = awk->get<LocalPlayerControl>()->look_dir();
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
	ability(Ability::Sensor),
	ability_level{ 1, 0, 0 },
	ability_cooldown(),
	entity(),
	spawn()
{
}

void PlayerManager::ability_switch(Ability a)
{
	vi_assert(ability_level[(s32)a] > 0);
	ability = a;
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
	credits += c;
}

b8 PlayerManager::at_spawn() const
{
	if (Game::data.mode == Game::Mode::Multiplayer)
		return entity.ref() && team.ref()->player_spawn.ref()->get<PlayerTrigger>()->is_triggered(entity.ref());
	else
		return false;
}

void PlayerManager::update(const Update& u)
{
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
		ability_cooldown = fmax(0.0f, ability_cooldown - u.time.delta);
}


}