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

namespace VI
{


const Vec4 Team::colors[(s32)AI::Team::count] =
{
	Vec4(1.0f, 0.9f, 0.4f, 1),
	Vec4(0.8f, 0.3f, 0.3f, 1),
};

const Vec4 Team::ui_colors[(s32)AI::Team::count] =
{
	Vec4(1.0f, 0.9f, 0.4f, 1),
	Vec4(1.0f, 0.5f, 0.5f, 1),
};

StaticArray<Team, (s32)AI::Team::count> Team::list;

Team::Team()
	: victory_timer(5.0f)
{
}

void Team::awake()
{
	if (player_spawn.ref())
		AI::obstacle_add(player_spawn.ref()->absolute_pos(), PLAYER_SPAWN_RADIUS, 4.0f);
}

b8 Team::game_over()
{
	if (NoclipControl::list.count() > 0)
		return false;

	s32 teams_with_players = 0;
	for (s32 i = 0; i < Team::list.length; i++)
	{
		if (Team::list[i].has_player())
			teams_with_players++;
	}
	return teams_with_players == 1;
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
	if (Game::data.mode == Game::Mode::Multiplayer && has_player() && game_over())
	{
		// we win
		victory_timer -= u.time.delta;
		if (victory_timer < 0.0f)
			Menu::transition(Game::data.next_level);
	}
}

AbilitySlot::Info AbilitySlot::info[] =
{
	//{ Asset::Mesh::icon_stun, strings::stun, 5.0f, { 20, 40, 80 } },
	{ Asset::Mesh::icon_sensor, strings::sensor, 30.0f, { 20, 40, 80 } },
	{ Asset::Mesh::icon_stealth, strings::stealth, 30.0f, { 20, 40, 80 } },
	//{ AssetNull, strings::turret, 60.0f, { 20, 40, 80 } },
	//{ AssetNull, strings::gun, 5.0f, { 20, 40, 80 } },
};

b8 AbilitySlot::can_upgrade() const
{
	return level < ABILITY_LEVELS;
}

u16 AbilitySlot::upgrade_cost() const
{
	if (!can_upgrade())
		return UINT16_MAX;
	if (ability == Ability::None)
	{
		// return cheapest possible upgrade
		u16 cheapest_upgrade = UINT16_MAX;
		for (s32 i = 0; i < (s32)Ability::count; i++)
		{
			if (info[i].upgrade_cost[0] < cheapest_upgrade)
				cheapest_upgrade = info[i].upgrade_cost[0];
		}
		return cheapest_upgrade;
	}
	else
		return info[(s32)ability].upgrade_cost[level];
}

b8 AbilitySlot::use(Entity* awk)
{
	if (cooldown == 0.0f)
	{
		cooldown = AbilitySlot::info[(s32)ability].cooldown;

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
					abs_pos += abs_rot * Vec3(0, 0, AWK_RADIUS * -0.5f); // make it nearly flush with the wall
					World::create<SensorEntity>(awk->get<Transform>()->parent.ref(), awk->get<AIAgent>()->team, abs_pos, abs_rot);
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
	abilities{ { Ability::Sensor, 1 } }
{
}

void PlayerManager::upgrade(Ability ability)
{
	b8 upgraded = false;
	for (s32 i = 0; i < ABILITY_COUNT; i++)
	{
		if (abilities[i].can_upgrade())
		{
			if (abilities[i].ability == ability)
			{
				credits -= abilities[i].upgrade_cost();
				abilities[i].level++;
				upgraded = true;
				break;
			}
			else if (abilities[i].ability == Ability::None)
			{
				abilities[i].ability = ability;
				credits -= abilities[i].upgrade_cost();
				abilities[i].level = 1;
				upgraded = true;
				break;
			}
		}
	}
	vi_assert(upgraded); // check for invalid upgrade request
}

void PlayerManager::add_credits(u16 c)
{
	credits += c;
}

b8 PlayerManager::upgrade_available() const
{
	for (s32 i = 0; i < ABILITY_COUNT; i++)
	{
		if (abilities[i].can_upgrade() && credits >= abilities[i].upgrade_cost())
			return true;
	}
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

	for (s32 i = 0; i < ABILITY_COUNT; i++)
	{
		if (abilities[i].cooldown > 0.0f)
			abilities[i].cooldown = fmax(0.0f, abilities[i].cooldown - u.time.delta);
	}
}


}