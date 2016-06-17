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
#include "mersenne/mersenne-twister.h"

#define PLAYER_SPAWN_DELAY 3.0f

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
	{
		Asset::Mesh::icon_sensor,
		2.5f,
		10,
	},
	{
		Asset::Mesh::icon_rocket,
		1.25f,
		5,
	},
	{
		Asset::Mesh::icon_minion,
		1.5f,
		7,
	},
	{
		Asset::Mesh::icon_containment_field,
		0.75f,
		20,
	},
};

UpgradeInfo UpgradeInfo::list[] =
{
	{
		strings::sensor,
		strings::description_sensor,
		Asset::Mesh::icon_sensor,
		40,
	},
	{
		strings::rocket,
		strings::description_rocket,
		Asset::Mesh::icon_rocket,
		80,
	},
	{
		strings::minion,
		strings::description_minion,
		Asset::Mesh::icon_minion,
		50,
	},
	{
		strings::containment_field,
		strings::description_containment_field,
		Asset::Mesh::icon_containment_field,
		150,
	},
	{
		strings::health_steal,
		strings::description_health_steal,
		AssetNull,
		150,
	},
	{
		strings::health_buff,
		strings::description_health_buff,
		AssetNull,
		200,
	},
};

#define GAME_OVER_TIME 5.0f

Team::Team()
	: victory_timer(GAME_OVER_TIME),
	player_tracks(),
	player_track_history(),
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
	if (Game::state.mode != Game::Mode::Pvp)
		return false;

	if (NoclipControl::list.count() > 0)
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

r32 Team::game_over_time()
{
	if (is_draw())
		return Game::time.total - GAME_TIME_LIMIT;
	else
	{
		r32 result = GAME_OVER_TIME;
		for (s32 i = 0; i < Team::list.length; i++)
			result = vi_min(result, Team::list[i].victory_timer);
		return GAME_OVER_TIME - result;
	}
}

b8 Team::has_player() const
{
	for (auto j = PlayerManager::list.iterator(); !j.is_last(); j.next())
	{
		if (j.item()->team.ref() == this && j.item()->entity.ref())
			return true;
	}
	return false;
}

b8 Team::is_local() const
{
	for (auto j = LocalPlayer::list.iterator(); !j.is_last(); j.next())
	{
		if (j.item()->manager.ref()->team.ref() == this)
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
	{
		if (Game::save.level_index < 2) // tutorial levels; just retry
			Menu::transition(Game::levels[Game::save.level_index], Game::Mode::Pvp);
		else
			Menu::transition(Game::levels[Game::save.level_index], Game::Mode::Parkour);
	}
}

void level_next()
{
	Game::Mode next_mode;
	AssetID next_level;

	if (Game::state.local_multiplayer)
	{
		// we're in local multiplayer mode
		next_mode = Game::Mode::Pvp;

		if (Game::level.lock_teams || Game::save.round == (s32)AI::Team::count - 1)
		{
			Game::save.level_index++; // advance to next level
			Game::save.round = 0;
		}
		else
			Game::save.round = (Game::save.round + 1) % (s32)AI::Team::count;
		next_level = Game::levels[Game::save.level_index];
	}
	else
	{
		// campaign mode; advance to next level
		next_mode = Game::Mode::Parkour;
		if (Game::level.lock_teams
			|| Game::save.level_index < 3 // advance past tutorials and first level after only one round
			|| Game::save.round == (s32)AI::Team::count - 1)
		{
			// advance to next level
			Game::save.level_index++;
			Game::save.round = 0;
			next_level = Game::levels[Game::save.level_index];
		}
		else
		{
			// play another round before advancing
			Game::save.round = (Game::save.round + 1) % (s32)AI::Team::count;
			next_level = Game::levels[Game::save.level_index];
		}
	}

	if (next_level == AssetNull) // done; go to main menu
		Menu::transition(Asset::Level::title, Game::Mode::Special);
	else
		Menu::transition(next_level, next_mode);
}

void Team::track(PlayerManager* player, PlayerManager* tracked_by)
{
	// enemy player has been detected by `tracked_by`
	vi_assert(player->entity.ref());
	vi_assert(player->team.ref() != this);

	if (tracked_by)
	{
		vi_assert(tracked_by->team.ref() == this);

		SensorTrack* track = &player_tracks[player->id()];

		Entity* player_entity = tracked_by->entity.ref();
		if (player_entity && player_entity->has<LocalPlayerControl>())
			player_entity->get<LocalPlayerControl>()->player.ref()->msg(_(strings::enemy_detected), true);
		s32 diff = player->add_credits(-CREDITS_DETECT);
		tracked_by->add_credits(-diff);

		track->tracking = true; // got em
		track->entity = player->entity;
		track->timer = SENSOR_TIME;
	}
}

void Team::update_all(const Update& u)
{
	if (Game::state.mode != Game::Mode::Pvp)
		return;

	b8 is_game_over = game_over();
	if (is_game_over && is_draw())
	{
		if (Game::time.total > GAME_TIME_LIMIT + GAME_OVER_TIME)
			level_retry(); // it's a draw; try again
	}

	// determine which Awks are seen by which teams
	Sensor* visibility[MAX_PLAYERS][(s32)AI::Team::count] = {};
	for (auto player = PlayerManager::list.iterator(); !player.is_last(); player.next())
	{
		Entity* player_entity = player.item()->entity.ref();

		if (player_entity && player_entity->has<Awk>() && player_entity->get<Transform>()->parent.ref())
		{
			// we're on a wall and can thus be detected
			AI::Team player_team = player.item()->team.ref()->team();
			Quat player_rot;
			Vec3 player_pos;
			player_entity->get<Transform>()->absolute(&player_pos, &player_rot);
			player_pos += player_rot * Vec3(0, 0, -AWK_RADIUS);
			for (auto sensor = Sensor::list.iterator(); !sensor.is_last(); sensor.next())
			{
				Sensor** sensor_visibility = &visibility[player.index][(s32)sensor.item()->team];
				if (!(*sensor_visibility))
				{
					Vec3 to_sensor = sensor.item()->get<Transform>()->absolute_pos() - player_pos;
					if (to_sensor.length_squared() < SENSOR_RANGE * SENSOR_RANGE
						&& to_sensor.dot(player_rot * Vec3(0, 0, 1)) > 0.0f)
						*sensor_visibility = sensor.item();
				}
			}
		}
	}

	// update stealth state
	for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
	{
		Entity* i_entity = i.item()->entity.ref();
		if (!i_entity)
			continue;

		AI::Team team = i.item()->team.ref()->team();

		// if we are within range of their own sensors
		// and not detected by enemy sensors
		// then we should be stealthed
		b8 stealth_enabled = true;
		if (!visibility[i.index][(s32)team])
			stealth_enabled = false;
		else
		{
			// check if any enemy sensors can see us
			for (s32 t = 0; t < Team::list.length; t++)
			{
				if ((AI::Team)t != team
					&& (visibility[i.index][t] || Team::list[t].player_tracks[i.index].tracking))
				{
					stealth_enabled = false;
					break;
				}
			}
		}
		i_entity->get<Awk>()->stealth(stealth_enabled);
	}

	// update player visibility
	for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
	{
		Entity* i_entity = i.item()->entity.ref();
		if (!i_entity)
			continue;

		AI::Team team = i.item()->team.ref()->team();

		auto j = i;
		j.next();
		for (; !j.is_last(); j.next())
		{
			Entity* j_entity = j.item()->entity.ref();
			if (!j_entity)
				continue;

			if (team == j.item()->team.ref()->team())
				continue;

			b8 visible;
			{
				Vec3 start = i_entity->get<Awk>()->center();
				Vec3 end = j_entity->get<Awk>()->center();
				Vec3 diff = end - start;

				if (btVector3(diff).fuzzyZero())
					visible = true;
				else
				{
					if (diff.length_squared() > AWK_MAX_DISTANCE * AWK_MAX_DISTANCE)
						visible = false;
					else
					{
						btCollisionWorld::ClosestRayResultCallback ray_callback(start, end);
						Physics::raycast(&ray_callback, btBroadphaseProxy::StaticFilter | CollisionInaccessible);
						visible = !ray_callback.hasHit();
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

	for (s32 team_id = 0; team_id < list.length; team_id++)
	{
		Team* team = &list[team_id];

		if (team->has_player() && is_game_over)
		{
			// we win
			team->victory_timer -= u.time.delta;
			if (team->victory_timer < 0.0f)
			{
				// done letting the player gloat
				b8 advance = false;
				if (Game::state.local_multiplayer)
					advance = true;
				else
					advance = team->is_local(); // if we're in campaign mode, only advance if the local team won

				if (advance)
					level_next();
				else
					level_retry();
			}
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
			if (sensor)
			{
				// team's sensors are picking up the Awk

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
							team->track(player.item(), sensor->owner.ref());
					}
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

			// launch a rocket at this player if the conditions are right
			if (player_entity && !Rocket::inbound(player_entity))
			{
				Vec3 player_pos = player_entity->get<Transform>()->absolute_pos();
				for (auto rocket = Rocket::list.iterator(); !rocket.is_last(); rocket.next())
				{
					if (rocket.item()->team == team->team() // it belongs to our team
						&& rocket.item()->get<Transform>()->parent.ref() // it's waiting to be fired
						&& (rocket.item()->get<Transform>()->absolute_pos() - player_pos).length_squared() < AWK_MAX_DISTANCE * 2.0f * AWK_MAX_DISTANCE * 2.0f // it's in range
						&& (track->tracking || (rocket.item()->owner.ref() && PlayerCommon::visibility.get(PlayerCommon::visibility_hash(rocket.item()->owner.ref()->get<PlayerCommon>(), player_entity->get<PlayerCommon>()))))) // we're tracking the player, or the owner is alive and can see the player
					{
						rocket.item()->launch(player_entity);
						break;
					}
				}
			}
		}
	}
}

b8 PlayerManager::has_upgrade(Upgrade u) const
{
	return upgrades & (1 << (u32)u);
}

b8 PlayerManager::ability_spawn_start(Ability ability)
{
	if (ability == Ability::None)
		return false;

	if (!Game::level.has_feature(Game::FeatureLevel::Abilities))
		return false;

	Entity* awk = entity.ref();
	if (!awk)
		return false;

	if (!has_upgrade((Upgrade)ability))
		return false;

	// need to be sitting on some kind of surface
	if (!awk->get<Transform>()->parent.ref())
		return false;

	const AbilityInfo& info = AbilityInfo::list[(s32)ability];
	if (credits < info.spawn_cost)
		return false;

	Ability old = current_spawn_ability;

	current_spawn_ability = ability;
	spawn_ability_timer = info.spawn_time;

	if (old != Ability::None)
		ability_spawn_canceled.fire(old);

	return true;
}

void PlayerManager::ability_spawn_stop(Ability ability)
{
	if (current_spawn_ability == ability)
	{
		current_spawn_ability = Ability::None;
		spawn_ability_timer = 0.0f;
		ability_spawn_canceled.fire(ability);
	}
}

void PlayerManager::ability_spawn_complete()
{
	Ability ability = current_spawn_ability;
	current_spawn_ability = Ability::None;

	Entity* awk = entity.ref();
	if (!awk)
	{
		ability_spawn_canceled.fire(ability);
		return;
	}

	u16 cost = AbilityInfo::list[(s32)ability].spawn_cost;
	if (credits < cost)
	{
		ability_spawn_canceled.fire(ability);
		return;
	}

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

				Entity* sensor = World::create<SensorEntity>(this, abs_pos + abs_rot * Vec3(0, 0, -AWK_RADIUS + (rope_segment_length * 2.0f) - rope_radius + SENSOR_RADIUS), abs_rot);

				Audio::post_global_event(AK::EVENTS::PLAY_SENSOR_SPAWN, abs_pos);

				// attach it to the wall
				Rope* rope = Rope::start(awk->get<Transform>()->parent.ref()->get<RigidBody>(), abs_pos + abs_rot * Vec3(0, 0, -AWK_RADIUS), abs_rot * Vec3(0, 0, 1), abs_rot);
				rope->end(abs_pos + abs_rot * Vec3(0, 0, -AWK_RADIUS + (rope_segment_length * 2.0f)), abs_rot * Vec3(0, 0, -1), sensor->get<RigidBody>());

				ability_spawned.fire(ability);
			}
			else
				ability_spawn_canceled.fire(ability);
			break;
		}
		case Ability::Rocket:
		{
			if (awk->get<Transform>()->parent.ref())
			{
				add_credits(-cost);

				// spawn a rocket pod
				Quat rot = awk->get<Transform>()->absolute_rot();
				Vec3 pos = awk->get<Transform>()->absolute_pos() + rot * Vec3(0, 0, -AWK_RADIUS);
				Transform* parent = awk->get<Transform>()->parent.ref();
				World::create<RocketEntity>(awk, parent, pos, rot, team.ref()->team());

				// rocket base
				Entity* base = World::alloc<Prop>(Asset::Mesh::rocket_base);
				base->get<Transform>()->absolute(pos, rot);
				base->get<Transform>()->reparent(parent);
				base->get<View>()->team = (u8)team.ref()->team();

				ability_spawned.fire(ability);
			}
			else
				ability_spawn_canceled.fire(ability);
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
				World::create<Minion>(pos, Quat::euler(0, awk->get<PlayerCommon>()->angle_horizontal, 0), team.ref()->team(), this);

				Audio::post_global_event(AK::EVENTS::PLAY_MINION_SPAWN, pos);

				ability_spawned.fire(ability);
			}
			else
				ability_spawn_canceled.fire(ability);
			break;
		}
		case Ability::ContainmentField:
		{
			if (awk->get<Transform>()->parent.ref())
			{
				add_credits(-cost);

				// spawn a containment field
				Vec3 pos;
				Quat rot;
				awk->get<Transform>()->absolute(&pos, &rot);
				pos += rot * Vec3(0, 0, CONTAINMENT_FIELD_BASE_OFFSET);
				World::create<ContainmentFieldEntity>(awk->get<Transform>()->parent.ref(), pos, rot, this);

				ability_spawned.fire(ability);
			}
			else
				ability_spawn_canceled.fire(ability);
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}
}

PinArray<PlayerManager, MAX_PLAYERS> PlayerManager::list;

PlayerManager::PlayerManager(Team* team)
	: spawn_timer(PLAYER_SPAWN_DELAY),
	team(team),
	credits(Game::level.has_feature(Game::FeatureLevel::Abilities) ? CREDITS_INITIAL : 0),
	upgrades(0),
	abilities{ Ability::None, Ability::None, Ability::None },
	entity(),
	spawn(),
	current_spawn_ability(Ability::None),
	current_upgrade(Upgrade::None),
	upgrade_timer(),
	ability_spawned(),
	ability_spawn_canceled(),
	upgrade_completed()
{
}

b8 PlayerManager::upgrade_start(Upgrade u)
{
	u16 cost = upgrade_cost(u);
	if (upgrade_available(u) && credits >= cost)
	{
		current_upgrade = u;
		upgrade_timer = UPGRADE_TIME;
		add_credits(-cost);
		return true;
	}
	return false;
}

void PlayerManager::upgrade_complete()
{
	Upgrade u = current_upgrade;
	current_upgrade = Upgrade::None;

	vi_assert(!has_upgrade(u));

	if (!entity.ref())
		return;

	upgrades |= 1 << (u32)u;

	if ((s32)u < (s32)Ability::count)
	{
		// it's an ability
		abilities[ability_count()] = (Ability)u;
	}

	if (u == Upgrade::HealthBuff)
	{
		entity.ref()->get<Health>()->hp_max = AWK_HEALTH + 1;
		entity.ref()->get<Health>()->added.fire(); // trigger UI animation
	}

	upgrade_completed.fire(u);
}

u16 PlayerManager::upgrade_cost(Upgrade u) const
{
	vi_assert(u != Upgrade::None);
	const UpgradeInfo& info = UpgradeInfo::list[(s32)u];
	return info.cost;
}

b8 PlayerManager::upgrade_available(Upgrade u) const
{
	if (u == Upgrade::None)
	{
		for (s32 i = 0; i < (s32)Upgrade::count; i++)
		{
			if (!has_upgrade((Upgrade)i) && credits >= upgrade_cost((Upgrade)i))
			{
				if (i >= (s32)Ability::count || ability_count() < MAX_ABILITIES)
					return true; // either it's not an ability, or it is an ability and we have enough room for it
			}
		}
		return false;
	}
	else
	{
		// make sure that either it's not an ability, or it is an ability and we have enough room for it
		return !has_upgrade(u) && ((s32)u >= (s32)Ability::count || ability_count() < MAX_ABILITIES);
	}
}

s32 PlayerManager::ability_count() const
{
	s32 count = 0;
	for (s32 i = 0; i < MAX_ABILITIES; i++)
	{
		if (abilities[i] != Ability::None)
			count++;
	}
	return count;
}

// returns the difference actually applied (never goes below 0)
s32 PlayerManager::add_credits(s32 c)
{
	if (c != 0)
	{
		s32 old_credits = credits;
		credits = (u16)vi_max(0, (s32)credits + c);
		credits_flash_timer = CREDITS_FLASH_TIME;
		return credits - old_credits;
	}
	return 0;
}

b8 PlayerManager::at_spawn() const
{
	if (Game::state.mode == Game::Mode::Pvp)
		return entity.ref() && entity.ref()->get<Transform>()->parent.ref() && team.ref()->player_spawn.ref()->get<PlayerTrigger>()->is_triggered(entity.ref());
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
			if (Game::state.mode == Game::Mode::Parkour)
				spawn_timer = PLAYER_SPAWN_DELAY; // reset timer so we can respawn next time
		}
	}

	if (upgrade_timer > 0.0f)
	{
		upgrade_timer = vi_max(0.0f, upgrade_timer - u.time.delta);
		if (upgrade_timer == 0.0f && current_upgrade != Upgrade::None)
			upgrade_complete();
	}

	if (spawn_ability_timer > 0.0f)
	{
		spawn_ability_timer = vi_max(0.0f, spawn_ability_timer - u.time.delta);

		if (spawn_ability_timer == 0.0f && current_spawn_ability != Ability::None)
			ability_spawn_complete();
	}
}


}
