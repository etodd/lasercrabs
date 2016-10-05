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
#include "cora.h"
#include "render/particles.h"

#define CREDITS_FLASH_TIME 0.5f
#define RESPAWNS 5

namespace VI
{


	const Vec4 Team::color_friend = Vec4(0.15f, 0.45f, 0.7f, MATERIAL_NO_OVERRIDE);
	const Vec4 Team::color_enemy = Vec4(1.0f, 0.3f, 0.4f, MATERIAL_NO_OVERRIDE);

	const Vec4 Team::ui_color_friend = Vec4(0.35f, 0.85f, 1.0f, 1);
	const Vec4 Team::ui_color_enemy = Vec4(1.0f, 0.4f, 0.4f, 1);

	StaticArray<Team, MAX_PLAYERS> Team::list;
	r32 Team::control_point_timer;
	r32 Team::game_over_real_time;
	b8 Team::game_over;
	Ref<Team> Team::winner;

	AbilityInfo AbilityInfo::list[(s32)Ability::count] =
	{
		{
			Asset::Mesh::icon_sensor,
			15,
		},
		{
			Asset::Mesh::icon_minion,
			15,
		},
		{
			Asset::Mesh::icon_teleporter,
			10,
		},
		{
			Asset::Mesh::icon_rocket,
			10,
		},
		{
			Asset::Mesh::icon_containment_field,
			20,
		},
		{
			Asset::Mesh::icon_sniper,
			10,
		},
	};

	UpgradeInfo UpgradeInfo::list[(s32)Upgrade::count] =
	{
		{
			strings::sensor,
			strings::description_sensor,
			Asset::Mesh::icon_sensor,
			50,
		},
		{
			strings::minion,
			strings::description_minion,
			Asset::Mesh::icon_minion,
			50,
		},
		{
			strings::teleporter,
			strings::description_teleporter,
			Asset::Mesh::icon_teleporter,
			50,
		},
		{
			strings::rocket,
			strings::description_rocket,
			Asset::Mesh::icon_rocket,
			120,
		},
		{
			strings::containment_field,
			strings::description_containment_field,
			Asset::Mesh::icon_containment_field,
			120,
		},
		{
			strings::sniper,
			strings::description_sniper,
			Asset::Mesh::icon_sniper,
			120,
		},
	};

	Team::Team()
		: player_tracks(),
		player_track_history(),
		player_spawn()
	{
	}

	void Team::awake_all()
	{
		game_over = false;
		game_over_real_time = 0.0f;
		winner = nullptr;
		PlayerManager::timer = CONTROL_POINT_INTERVAL;
		Game::session.last_match = Game::MatchResult::Forfeit;

		for (s32 i = 0; i < Team::list.length; i++)
		{
			Team* team = &Team::list[i];
			for (auto j = PlayerManager::list.iterator(); !j.is_last(); j.next())
			{
				if (j.item()->team.ref() != team)
					extract_history(j.item(), &team->player_track_history[j.index]);
			}
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

	b8 Team::has_player() const
	{
		for (auto j = PlayerManager::list.iterator(); !j.is_last(); j.next())
		{
			if (j.item()->team.ref() == this
				&& (j.item()->respawns > 1 || j.item()->entity.ref()))
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
			history->shield = health->shield;
			history->shield_max = health->shield_max;
			history->pos = manager->entity.ref()->get<Transform>()->absolute_pos();
		}
		else
		{
			// initial health
			history->hp = 0;
			history->hp_max = AWK_HEALTH;
			history->shield = AWK_SHIELD;
			history->shield_max = AWK_SHIELD;
			history->pos = manager->team.ref()->player_spawn.ref()->absolute_pos();
		}
	}

	void Team::transition_next(Game::MatchResult result)
	{
		Game::session.last_match = result;
		if (Game::session.level == Asset::Level::Safe_Zone && result == Game::MatchResult::Loss)
			Game::schedule_load_level(Game::session.level, Game::Mode::Pvp); // retry tutorial automatically
		else
			Terminal::show();
	}

	s16 Team::containment_field_mask(AI::Team t)
	{
		return 1 << (8 + t);
	}

	void Team::track(PlayerManager* player)
	{
		// enemy player has been detected by `tracked_by`
		vi_assert(player->entity.ref());
		vi_assert(player->team.ref() != this);

		SensorTrack* track = &player_tracks[player->id()];
		track->tracking = true; // got em
		track->entity = player->entity;
		track->timer = SENSOR_TIMEOUT;
	}

	s32 Team::control_point_count() const
	{
		s32 count = 0;
		for (auto i = ControlPoint::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->team == team())
				count++;
		}
		return count;
	}

	void Team::update_all(const Update& u)
	{
		if (Game::session.mode != Game::Mode::Pvp || !Game::session.local)
			return;

		if (!game_over)
		{
			if (!Game::level.continue_match_after_death
			&& ((Game::time.total > GAME_TIME_LIMIT && (Game::session.type != Game::Type::Rush || ControlPoint::count_capturing() == 0))
				|| (PlayerManager::list.count() > 1 && teams_with_players() <= 1)
				|| (Game::session.type == Game::Type::Rush && Team::list[1].control_point_count() > 0)))
			{
				game_over = true;
				game_over_real_time = Game::real_time.total;
				vi_debug("Game over. Time: %f\n", Game::time.total);

				// remove in-flight projectiles
				{
					for (auto i = Projectile::list.iterator(); !i.is_last(); i.next())
						World::remove_deferred(i.item()->entity());

					for (auto i = Rocket::list.iterator(); !i.is_last(); i.next())
					{
						if (!i.item()->get<Transform>()->parent.ref()) // it's in flight
							World::remove_deferred(i.item()->entity());
					}
				}

				// determine the winner, if any
				Team* result = nullptr;
				s32 teams_with_players = 0;
				for (s32 i = 0; i < Team::list.length; i++)
				{
					if (Team::list[i].has_player())
					{
						result = &Team::list[i];
						teams_with_players++;
					}
				}

				if (teams_with_players == 1)
				{
					winner = result;
					for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
					{
						s32 total = 0;

						total += i.item()->credits;
						i.item()->credits_summary.add({ strings::leftover_energy, i.item()->credits });

						if (i.item()->is_local() && !Game::session.multiplayer)
						{
							// we're in story mode and this is a local player; increase their energy
							Game::save.resources[(s32)Game::Resource::Energy] += total;
						}
					}
				}
				else if (Game::session.type == Game::Type::Rush)
				{
					if (Team::list[1].control_point_count() > 0)
						winner = &Team::list[1]; // attackers win
					else if (Game::time.total > GAME_TIME_LIMIT)
						winner = &Team::list[0]; // defenders win
					else
						winner = nullptr; // draw
				}
				else
					winner = nullptr;
			}
		}

		if (game_over)
		{
			// wait for all local players to accept scores
			b8 score_accepted = true;
			for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
			{
				if (i.item()->is_local() && !i.item()->score_accepted)
					score_accepted = false;
			}

			if (score_accepted)
			{
				// time to get out of here

				if (winner.ref())
				{
					// somebody won
					if (Game::session.multiplayer)
						transition_next(Game::MatchResult::None);
					else
					{
						// if we're in story mode, only advance if the local team won
						b8 won = false;
						for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
						{
							if (i.item()->team.ref() == winner.ref() && i.item()->is_local())
							{
								transition_next(Game::MatchResult::Victory);
								won = true;
								break;
							}
						}
						if (!won)
							transition_next(Game::MatchResult::Loss);
					}
				}
				else
					transition_next(Game::MatchResult::Draw);
			}
		}

		// determine which Awks are seen by which teams
		Sensor* visibility[MAX_PLAYERS][MAX_PLAYERS] = {};
		for (auto player = PlayerManager::list.iterator(); !player.is_last(); player.next())
		{
			Entity* player_entity = player.item()->entity.ref();

			if (player_entity && player_entity->get<Awk>()->state() == Awk::State::Crawl)
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
				Vec3 diff;
				{
					Vec3 start = i_entity->get<Awk>()->center_lerped();
					Vec3 end = j_entity->get<Awk>()->center_lerped();
					diff = end - start;

					if (btVector3(diff).fuzzyZero())
						visible = true;
					else
					{
						btCollisionWorld::ClosestRayResultCallback ray_callback(start, end);
						Physics::raycast(&ray_callback, btBroadphaseProxy::StaticFilter | CollisionInaccessible);
						visible = !ray_callback.hasHit();
					}
				}

				r32 distance = diff.length_squared();
				r32 i_range = i_entity->get<Awk>()->range();
				b8 i_can_see_j = visible && !j_entity->get<AIAgent>()->stealth && distance < i_range * i_range;
				r32 j_range = j_entity->get<Awk>()->range();
				b8 j_can_see_i = visible && !i_entity->get<AIAgent>()->stealth && distance < j_range * j_range;
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
								team->track(player.item());
						}
					}
					else if (player_entity->get<Awk>()->state() == Awk::State::Crawl)
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
				if (player_entity && !game_over && !Rocket::inbound(player_entity))
				{
					Vec3 player_pos = player_entity->get<Transform>()->absolute_pos();
					for (auto rocket = Rocket::list.iterator(); !rocket.is_last(); rocket.next())
					{
						if (rocket.item()->team == team->team() // it belongs to our team
							&& rocket.item()->get<Transform>()->parent.ref() // it's waiting to be fired
							&& (track->tracking || (rocket.item()->owner.ref() && PlayerCommon::visibility.get(PlayerCommon::visibility_hash(rocket.item()->owner.ref()->get<PlayerCommon>(), player_entity->get<PlayerCommon>()))))) // we're tracking the player, or the owner is alive and can see the player
						{
							Vec3 rocket_pos = rocket.item()->get<Transform>()->absolute_pos();
							if ((rocket_pos - player_pos).length_squared() < ROCKET_RANGE * ROCKET_RANGE // it's in range
								&& ContainmentField::hash(team->team(), rocket_pos) == ContainmentField::hash(team->team(), player_pos)) // no containment fields in the way
							{
								rocket.item()->launch(player_entity);
								break;
							}
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

	b8 PlayerManager::ability_valid(Ability ability) const
	{
		if (ability == Ability::None)
			return false;

		if (!Game::level.has_feature(Game::FeatureLevel::Abilities))
			return false;

		if (!can_transition_state())
			return false;

		if (!has_upgrade((Upgrade)ability))
			return false;

		const AbilityInfo& info = AbilityInfo::list[(s32)ability];
		if (credits < info.spawn_cost)
			return false;

		return true;
	}

	PinArray<PlayerManager, MAX_PLAYERS> PlayerManager::list;

	PlayerManager::PlayerManager(Team* team)
		: spawn_timer(PLAYER_SPAWN_DELAY),
		score_accepted(),
		team(team),
		credits(Game::level.has_feature(Game::FeatureLevel::Abilities) ? CREDITS_INITIAL : 0),
		upgrades(0),
		abilities{ Ability::None, Ability::None, Ability::None },
		entity(),
		spawn(),
		current_upgrade(Upgrade::None),
		state_timer(),
		upgrade_completed(),
		control_point_capture_completed(),
		credits_summary(),
		particle_accumulator(),
		respawns(RESPAWNS)
	{
	}

	b8 PlayerManager::upgrade_start(Upgrade u)
	{
		u16 cost = upgrade_cost(u);
		if (can_transition_state()
			&& upgrade_available(u)
			&& credits >= cost
			&& at_upgrade_point())
		{
			current_upgrade = u;
			state_timer = UPGRADE_TIME;
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

		upgrade_completed.fire(u);
	}

	b8 PlayerManager::capture_start()
	{
		ControlPoint* control_point = at_control_point();
		if (can_transition_state() && control_point && !friendly_control_point(control_point))
		{
			vi_assert(current_upgrade == Upgrade::None);
			entity.ref()->get<Awk>()->current_ability = Ability::None;
			state_timer = CAPTURE_TIME;
			return true;
		}
		return false;
	}

	// the capture is not actually complete; we've completed the process of *starting* to capture the point
	void PlayerManager::capture_complete()
	{
		if (!entity.ref())
			return;

		b8 success = false;
		ControlPoint* control_point = at_control_point();
		if (control_point && !friendly_control_point(control_point))
		{
			add_credits(CREDITS_CAPTURE_CONTROL_POINT);
			if (control_point->team_next == AI::TeamNone)
			{
				// no capture in progress; start capturing
				control_point->capture_start(team.ref()->team());
			}
			else
			{
				// capture already in progress; cancel if necessary
				vi_assert(control_point->team_next != team.ref()->team());
				control_point->capture_cancel();
				if (control_point->team != team.ref()->team())
				{
					// start capturing again
					control_point->capture_start(team.ref()->team());
				}
			}
			success = true;
		}

		control_point_capture_completed.fire(success);
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

	b8 PlayerManager::at_upgrade_point() const
	{
		return team.ref()->player_spawn.ref()->get<PlayerTrigger>()->is_triggered(entity.ref());
	}

	ControlPoint* PlayerManager::at_control_point() const
	{
		Entity* e = entity.ref();
		if (e && e->get<Awk>()->state() == Awk::State::Crawl)
		{
			for (auto i = ControlPoint::list.iterator(); !i.is_last(); i.next())
			{
				if (i.item()->get<PlayerTrigger>()->is_triggered(e))
					return i.item();
			}
		}
		return nullptr;
	}

	b8 PlayerManager::friendly_control_point(const ControlPoint* p) const
	{
		return p && p->owned_by(team.ref()->team());
	}

	PlayerManager::State PlayerManager::state() const
	{
		if (state_timer == 0.0f)
			return State::Default;
		else
		{
			if (current_upgrade != Upgrade::None)
				return State::Upgrading;
			else
				return State::Capturing;
		}
	}

	b8 PlayerManager::can_transition_state() const
	{
		if (!Game::level.has_feature(Game::FeatureLevel::Abilities))
			return false;

		Entity* e = entity.ref();
		if (!e)
			return false;

		State s = state();
		if (s != State::Default)
			return false;

		Awk* awk = e->get<Awk>();
		return awk->state() == Awk::State::Crawl;
	}

	u16 PlayerManager::increment() const
	{
		if (!entity.ref())
			return 0;

		return ControlPoint::count(1 << team.ref()->team()) * CREDITS_CONTROL_POINT
			+ EnergyPickup::count(1 << team.ref()->team()) * CREDITS_ENERGY_PICKUP;
	}

	r32 PlayerManager::timer = CONTROL_POINT_INTERVAL;
	void PlayerManager::update_all(const Update& u)
	{
		if (Game::session.mode == Game::Mode::Pvp
			&& Game::session.local
			&& Game::level.has_feature(Game::FeatureLevel::EnergyPickups)
			&& u.time.total > GAME_BUY_PERIOD)
		{
			timer -= u.time.delta;
			if (timer < 0.0f)
			{
				// give points to players based on how many control points they own
				for (auto i = list.iterator(); !i.is_last(); i.next())
					i.item()->add_credits(i.item()->increment());

				timer += CONTROL_POINT_INTERVAL;
			}
		}

		for (auto i = list.iterator(); !i.is_last(); i.next())
			i.item()->update(u);
	}

	void PlayerManager::update(const Update& u)
	{
		credits_flash_timer = vi_max(0.0f, credits_flash_timer - Game::real_time.delta);

		if (!entity.ref()
			&& spawn_timer > 0.0f
			&& team.ref()->player_spawn.ref()
			&& respawns > 0
			&& !Game::level.continue_match_after_death)
		{
			spawn_timer -= u.time.delta;
			if (spawn_timer <= 0.0f)
			{
				respawns--;
				if (respawns > 0)
					spawn_timer = PLAYER_SPAWN_DELAY;
				spawn.fire();
			}
		}

		State s = state();

		if (state_timer > 0.0f)
		{
			state_timer = vi_max(0.0f, state_timer - u.time.delta);
			if (state_timer == 0.0f)
			{
				switch (s)
				{
					case State::Capturing:
					{
						capture_complete();
						break;
					}
					case State::Upgrading:
					{
						upgrade_complete();
						break;
					}
					default:
					{
						vi_assert(false);
						break;
					}
				}
			}
		}
	}

	b8 PlayerManager::is_local() const
	{
		for (auto j = LocalPlayer::list.iterator(); !j.is_last(); j.next())
		{
			if (j.item()->manager.ref() == this)
				return true;
		}
		return false;
	}

}
