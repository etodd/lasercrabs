#include "team.h"
#include "game.h"
#include "data/components.h"
#include "entities.h"
#include "data/animator.h"
#include "asset/animation.h"
#include "asset/mesh.h"
#include "strings.h"
#include "drone.h"
#include "minion.h"
#include "bullet/src/BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "audio.h"
#include "asset/Wwise_IDs.h"
#include "asset/level.h"
#include "walker.h"
#include "mersenne/mersenne-twister.h"
#include "render/particles.h"
#include "net.h"
#include "ai_player.h"
#include "overworld.h"
#include "player.h"

#define ENERGY_FLASH_TIME 0.5f

namespace VI
{


namespace TeamNet
{
	b8 update_kills(Team*);
}

const Vec4 Team::color_friend = Vec4(0.15f, 0.45f, 0.7f, MATERIAL_NO_OVERRIDE);
const Vec4 Team::color_enemy = Vec4(1.0f, 0.3f, 0.4f, MATERIAL_NO_OVERRIDE);

const Vec4 Team::ui_color_friend = Vec4(0.35f, 0.85f, 1.0f, 1);
const Vec4 Team::ui_color_enemy = Vec4(1.0f, 0.4f, 0.4f, 1);

r32 Team::control_point_timer;
r32 Team::game_over_real_time;
b8 Team::game_over;
Ref<Team> Team::winner;
Game::Mode Team::transition_mode_scheduled = Game::Mode::None;
StaticArray<Team::ScoreSummaryItem, MAX_PLAYERS * PLAYER_SCORE_SUMMARY_ITEMS> Team::score_summary;
r32 Team::transition_timer;
r32 Team::match_time;

AbilityInfo AbilityInfo::list[s32(Ability::count)] =
{
	{
		Asset::Mesh::icon_bolter,
		0,
		AbilityInfo::Type::Shoot,
		true,
	},
	{
		Asset::Mesh::icon_active_armor,
		0,
		AbilityInfo::Type::Other,
		false,
	},
	{
		Asset::Mesh::icon_minion,
		25,
		AbilityInfo::Type::Build,
		false,
	},
	{
		Asset::Mesh::icon_sensor,
		25,
		AbilityInfo::Type::Build,
		false,
	},
	{
		Asset::Mesh::icon_force_field,
		100,
		AbilityInfo::Type::Build,
		false,
	},
	{
		Asset::Mesh::icon_sniper,
		0,
		AbilityInfo::Type::Shoot,
		false,
	},
	{
		Asset::Mesh::icon_grenade,
		25,
		AbilityInfo::Type::Shoot,
		false,
	},
};

UpgradeInfo UpgradeInfo::list[s32(Upgrade::count)] =
{
	{
		strings::bolter,
		strings::description_bolter,
		Asset::Mesh::icon_bolter,
		100,
	},
	{
		strings::active_armor,
		strings::description_active_armor,
		Asset::Mesh::icon_active_armor,
		100,
	},
	{
		strings::minion,
		strings::description_minion,
		Asset::Mesh::icon_minion,
		100,
	},
	{
		strings::sensor,
		strings::description_sensor,
		Asset::Mesh::icon_sensor,
		100,
	},
	{
		strings::force_field,
		strings::description_force_field,
		Asset::Mesh::icon_force_field,
		200,
	},
	{
		strings::sniper,
		strings::description_sniper,
		Asset::Mesh::icon_sniper,
		300,
	},
	{
		strings::grenade,
		strings::description_grenade,
		Asset::Mesh::icon_grenade,
		300,
	},
};

void Team::awake_all()
{
	game_over = false;
	game_over_real_time = 0.0f;
	if (Game::level.local) // if we're a client, the netcode manages this
	{
		if (Game::session.type == SessionType::Story
			&& Game::level.mode == Game::Mode::Pvp
			&& Game::save.zones[Game::level.id] == ZoneState::PvpFriendly)
			match_time = 20.0f + mersenne::randf_cc() * (ZONE_UNDER_ATTACK_THRESHOLD * 1.5f); // player is defending; AI attacker has been here for some time
		else
			match_time = 0.0f;
	}
	winner = nullptr;
	score_summary.length = 0;
	for (s32 i = 0; i < MAX_PLAYERS * MAX_PLAYERS; i++)
		PlayerManager::visibility[i] = { nullptr, PlayerManager::Visibility::Type::Direct };
}

s32 Team::teams_with_active_players()
{
	s32 t = 0;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->has_active_player())
			t++;
	}
	return t;
}

b8 Team::has_active_player() const
{
	for (s32 i = 0; i < Game::level.ai_config.length; i++)
	{
		if (Game::level.ai_config[i].team == team())
			return true;
	}

	for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team.ref() == this
			&& (i.item()->respawns != 0 || i.item()->instance.ref()))
			return true;
	}

	return false;
}

void Team::transition_next()
{
	vi_assert(Game::level.local);
	if (Game::session.type == SessionType::Story)
	{
		if (Game::level.id == Game::save.zone_current)
			transition_mode(Game::Mode::Parkour);
		else
		{
			// we're playing a different zone than the one the player is currently on
			// that must mean that we need to restore the position in that zone
			vi_assert(Game::save.zone_current_restore);
			Game::schedule_load_level(Game::save.zone_current, Game::Mode::Parkour);
		}
	}
	else
	{
		// todo: map rotation
	}
}

s16 Team::force_field_mask(AI::Team t)
{
	return 1 << (8 + t);
}

void Team::track(PlayerManager* player, Entity* e)
{
	// enemy player has been detected by `tracked_by`
	vi_assert(player->team.ref() != this);

	SensorTrack* track = &player_tracks[player->id()];
	track->tracking = true; // got em
	track->entity = e;
}

s32 Team::player_count() const
{
	s32 count = 0;
	for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team.ref() == this)
			count++;
	}
	return count;
}

void Team::add_kills(s32 k)
{
	vi_assert(Game::level.local);
	kills += k;
	TeamNet::update_kills(this);
}

s16 Team::increment() const
{
	s16 increment = ENERGY_DEFAULT_INCREMENT;
	for (auto i = Battery::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team == team())
			increment += i.item()->increment();
	}
	return increment;
}

Team* Team::with_most_kills()
{
	s16 highest_kills = 0;
	Team* result = nullptr;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		s16 kills = i.item()->kills;
		if (kills == highest_kills)
			result = nullptr;
		else if (kills > highest_kills)
		{
			highest_kills = kills;
			result = i.item();
		}
	}
	return result;
}

b8 visibility_check(Entity* i, Entity* j, r32* distance)
{
	Vec3 start = i->get<Transform>()->absolute_pos();
	Vec3 end = j->get<Transform>()->absolute_pos();
	Vec3 diff = end - start;

	r32 dist_sq = diff.length_squared();
	if (dist_sq == 0.0f)
	{
		*distance = 0.0f;
		return true;
	}
	else
	{
		btCollisionWorld::ClosestRayResultCallback ray_callback(start, end);
		Physics::raycast(&ray_callback, CollisionStatic | CollisionInaccessible | CollisionParkour | CollisionElectric);
		if (!ray_callback.hasHit())
		{
			*distance = sqrtf(dist_sq);
			return true;
		}
	}

	return false;
}

// determine which sensors can see the given player
void update_visibility_sensor(Entity* visibility[][MAX_TEAMS], PlayerManager* player, Entity* player_entity)
{
	Quat player_rot;
	Vec3 player_pos;
	player_entity->get<Transform>()->absolute(&player_pos, &player_rot);
	player_pos += player_rot * Vec3(0, 0, -DRONE_RADIUS);
	Vec3 normal = player_rot * Vec3(0, 0, 1);
	for (auto sensor = Sensor::list.iterator(); !sensor.is_last(); sensor.next())
	{
		Entity** sensor_visibility = &visibility[player->id()][s32(sensor.item()->team)];
		if (!(*sensor_visibility))
		{
			Vec3 to_sensor = sensor.item()->get<Transform>()->absolute_pos() - player_pos;
			if (to_sensor.length_squared() < SENSOR_RANGE * SENSOR_RANGE
				&& to_sensor.dot(normal) > 0.0f)
				*sensor_visibility = player_entity;
		}
	}
}

void update_stealth_state(PlayerManager* player, AIAgent* a, Entity* visibility[][MAX_TEAMS])
{
	Quat player_rot;
	Vec3 player_pos;
	a->get<Transform>()->absolute(&player_pos, &player_rot);
	player_pos += player_rot * Vec3(0, 0, -DRONE_RADIUS);
	Vec3 normal = player_rot * Vec3(0, 0, 1);

	// if we are within range of their own sensors
	// and not detected by enemy sensors
	// then we should be stealthed
	b8 stealth_enabled = true;
	UpgradeStation* upgrade_station = UpgradeStation::drone_inside(a->get<Drone>());
	if (upgrade_station && upgrade_station->timer == 0.0f) // always stealthed inside upgrade stations (but not while transitioning)
		stealth_enabled = true;
	else if (!Sensor::can_see(a->team, player_pos, normal))
		stealth_enabled = false;
	else
	{
		// check if any enemy sensors can see us
		for (auto t = Team::list.iterator(); !t.is_last(); t.next())
		{
			if (t.item()->team() != a->team && t.item()->player_tracks[player->id()].entity.ref() == a->entity())
			{
				stealth_enabled = false;
				break;
			}
		}
	}
	Drone::stealth(a->entity(), stealth_enabled);
}

void update_visibility(const Update& u)
{
	// determine which drones are seen by which teams
	// and update their stealth state
	Entity* visibility[MAX_PLAYERS][MAX_TEAMS] = {};
	for (auto player = PlayerManager::list.iterator(); !player.is_last(); player.next())
	{
		Entity* player_entity = player.item()->instance.ref();
		if (player_entity && player_entity->has<Drone>())
		{
			if (player_entity->get<Drone>()->state() == Drone::State::Crawl) // we're on a wall and can thus be detected
			{
				update_visibility_sensor(visibility, player.item(), player_entity);
				update_stealth_state(player.item(), player_entity->get<AIAgent>(), visibility);
			}
			else
				Drone::stealth(player_entity, false); // always visible while flying or dashing
		}
	}

	// update player visibility
	for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
	{
		Entity* i_entity = i.item()->instance.ref();

		if (!i_entity || !i_entity->has<Drone>())
			continue;

		Team* i_team = i.item()->team.ref();

		r32 i_range = i_entity->get<Drone>()->range();

		for (auto j = PlayerManager::list.iterator(); !j.is_last(); j.next())
		{
			Team* j_team = j.item()->team.ref();

			if (i_team == j_team)
				continue;

			PlayerManager::Visibility detected = { nullptr, PlayerManager::Visibility::Type::Direct };

			Entity* j_actual_entity = j.item()->instance.ref();
			if (j_actual_entity && !j_actual_entity->get<AIAgent>()->stealth)
			{
				// i_entity detecting j_actual_entity
				r32 distance;
				if ((visibility_check(i_entity, j_actual_entity, &distance)
					&& distance < i_range))
				{
					detected.entity = j_actual_entity;
					detected.type = PlayerManager::Visibility::Type::Direct;
				}

				if (!detected.entity.ref())
				{
					// i turrets detecting j_actual_entity
					for (auto t = Turret::list.iterator(); !t.is_last(); t.next())
					{
						if (t.item()->team == i_team->team() && t.item()->target.ref() == j_actual_entity)
						{
							detected.entity = j_actual_entity;
							detected.type = PlayerManager::Visibility::Type::Indirect;
							break;
						}
					}

					// i minions detecting j_actual_entity
					if (!detected.entity.ref())
					{
						for (auto m = Minion::list.iterator(); !m.is_last(); m.next())
						{
							if (m.item()->get<AIAgent>()->team == i_team->team() && m.item()->goal.entity.ref() == j_actual_entity)
							{
								detected.entity = j_actual_entity;
								detected.type = PlayerManager::Visibility::Type::Indirect;
								break;
							}
						}
					}
				}
			}

			PlayerManager::visibility[PlayerManager::visibility_hash(i.item(), j.item())] = detected;
		}
	}

	for (auto t = Team::list.iterator(); !t.is_last(); t.next())
	{
		Team* team = t.item();

		// update tracking timers.

		for (auto player = PlayerManager::list.iterator(); !player.is_last(); player.next())
		{
			AI::Team player_team = player.item()->team.ref()->team();
			if (team->team() == player_team)
				continue;

			Entity* detected_entity = visibility[player.index][team->id()];
			Team::SensorTrack* track = &team->player_tracks[player.index];
			if (detected_entity)
			{
				// team's sensors are picking up the Drone
				if (track->entity.ref() == detected_entity)
				{
					if (track->tracking)
						track->timer = SENSOR_LINGER_TIME; // this is how much time we'll continue to track them after we can no longer detect them
					else
					{
						// tracking but not yet alerted
						track->timer += u.time.delta;
						if (track->timer >= SENSOR_TRACK_TIME)
							team->track(player.item(), detected_entity);
					}
				}
				else if (detected_entity->get<Drone>()->state() == Drone::State::Crawl)
				{
					// not tracking yet; insert new track entry
					// (only start tracking if the Drone is attached to a wall; don't start tracking if Drone is mid-air)

					new (track) Team::SensorTrack();
					track->entity = detected_entity;
				}
			}
			else
			{
				// team's sensors don't see the Drone
				// done tracking
				if (track->tracking && track->entity.ref() && track->timer > 0.0f) // track still remains active for SENSOR_LINGER_TIME seconds
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

namespace TeamNet
{
	enum Message
	{
		GameOver,
		TransitionMode,
		UpdateKills,
		count,
	};

	b8 send_game_over(Team* w)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::Team);
		{
			Message type = Message::GameOver;
			serialize_enum(p, Message, type);
		}
		{
			Ref<Team> ref = w;
			serialize_ref(p, ref);
		}
		Net::msg_finalize(p);
		return true;
	}

	b8 send_transition_mode(Game::Mode m)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::Team);
		{
			Message type = Message::TransitionMode;
			serialize_enum(p, Message, type);
		}
		serialize_enum(p, Game::Mode, m);
		Net::msg_finalize(p);
		return true;
	}

	b8 update_kills(Team* t)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::Team);
		{
			Message type = Message::UpdateKills;
			serialize_enum(p, Message, type);
		}
		{
			Ref<Team> ref = t;
			serialize_ref(p, ref);
		}
		serialize_s16(p, t->kills);
		Net::msg_finalize(p);
		return true;
	}
}

void team_add_score_summary_item(PlayerManager* player, const char* label, s32 amount = -1)
{
	Team::ScoreSummaryItem* item = Team::score_summary.add();
	item->amount = amount;
	item->player = player;
	item->team = player->team.ref()->team();
	strncpy(item->label, label, 512);
	item->label[511] = '\0';
}

b8 Team::net_msg(Net::StreamRead* p, Net::MessageSource src)
{
	using Stream = Net::StreamRead;

	TeamNet::Message type;
	serialize_enum(p, TeamNet::Message, type);

	switch (type)
	{
		case TeamNet::Message::GameOver:
		{
			serialize_ref(p, winner);
			game_over = true;
			game_over_real_time = Game::real_time.total;

			score_summary.length = 0;
			for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
			{
				i.item()->score_accepted = false;
				AI::Team team = i.item()->team.ref()->team();
				team_add_score_summary_item(i.item(), i.item()->username);
				if (Game::session.type == SessionType::Story)
				{
					team_add_score_summary_item(i.item(), _(strings::drone_surplus), i.item()->respawns);
					team_add_score_summary_item(i.item(), _(strings::energy_surplus), i.item()->energy);
					if (i.item()->has<PlayerHuman>() && i.item()->team.equals(winner))
					{
						s16 rewards[s32(Resource::count)];
						Overworld::zone_rewards(Game::level.id, rewards);
						for (s32 j = 0; j < s32(Resource::count); j++)
						{
							if (rewards[j] > 0)
								team_add_score_summary_item(i.item(), _(Overworld::resource_info[j].description), rewards[j]);
						}
					}
				}
				else
				{
					team_add_score_summary_item(i.item(), _(strings::kills), i.item()->kills);
					team_add_score_summary_item(i.item(), _(strings::deaths), i.item()->deaths);
				}
			}
			break;
		}
		case TeamNet::Message::TransitionMode:
		{
			Game::Mode mode_old = Game::level.mode;
			serialize_enum(p, Game::Mode, Game::level.mode);
			game_over = false;
			transition_timer = TRANSITION_TIME * 0.5f;
			Audio::post_global_event(AK::EVENTS::PLAY_TRANSITION_IN);
			match_time = 0.0f;
			if (Game::level.mode == Game::Mode::Pvp)
				Game::level.post_pvp = true; // we have played (or are playing) a PvP match on this level
			for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
				i.item()->game_mode_transitioning();
			if (Game::level.local)
			{
				for (auto i = PlayerCommon::list.iterator(); !i.is_last(); i.next())
					World::remove(i.item()->entity());
				for (auto i = PlayerAI::list.iterator(); !i.is_last(); i.next())
					World::remove(i.item()->manager.ref()->entity());
				PlayerAI::list.clear();
			}
			else
			{
				// some physics entities change between dynamic/kinematic depending on whether they are controlled by the server or not
				// which changes depending on the game mode
				// so rebuild the entities that changed to make sure they're set up right
				Bitmask<MAX_ENTITIES> transform_filter_before;
				for (auto i = RigidBody::list.iterator(); !i.is_last(); i.next())
					transform_filter_before.set(i.index, Game::net_transform_filter(i.item()->entity(), mode_old));
				for (auto i = RigidBody::list.iterator(); !i.is_last(); i.next())
				{
					if (Game::net_transform_filter(i.item()->entity(), Game::level.mode) != transform_filter_before.get(i.index))
						i.item()->rebuild();
				}
			}

			if (Game::level.post_pvp && Game::level.mode == Game::Mode::Parkour)
			{
				// exiting pvp mode
				vi_assert(Game::session.type == SessionType::Story);
				AI::Team team_winner = winner.ref()->team();
				// winner takes all
				if (Game::level.local)
				{
					// this stuff is synced over the network, so must do it only on the server
					for (auto i = Battery::list.iterator(); !i.is_last(); i.next())
						i.item()->set_team(team_winner);
					for (auto i = Minion::list.iterator(); !i.is_last(); i.next())
						World::remove_deferred(i.item()->entity());
					for (auto i = Grenade::list.iterator(); !i.is_last(); i.next())
						World::remove_deferred(i.item()->entity());
				}
				// teams are not synced over the network, so set them on both client and server
				for (auto i = Turret::list.iterator(); !i.is_last(); i.next())
					i.item()->set_team(team_winner);
				for (auto i = ForceField::list.iterator(); !i.is_last(); i.next())
					i.item()->set_team(team_winner);
				for (auto i = CoreModule::list.iterator(); !i.is_last(); i.next())
					i.item()->set_team(team_winner);
				for (auto i = Sensor::list.iterator(); !i.is_last(); i.next())
					i.item()->set_team(team_winner);
				TerminalEntity::open();

				Overworld::zone_done(Game::level.id);
			}
			break;
		}
		case TeamNet::Message::UpdateKills:
		{
			Ref<Team> t;
			serialize_ref(p, t);
			s16 kills;
			serialize_s16(p, kills);
			if (!Game::level.local || src == Net::MessageSource::Loopback)
				t.ref()->kills = kills;
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

void Team::update_all_server(const Update& u)
{
	if (transition_mode_scheduled != Game::Mode::None)
	{
		Game::Mode m = transition_mode_scheduled;
		transition_mode_scheduled = Game::Mode::None;
		TeamNet::send_transition_mode(m);
	}

	if (Game::level.mode != Game::Mode::Pvp)
		return;

	if (!game_over)
	{
		Team* team_with_most_kills = Game::session.config.game_type == GameType::Deathmatch ? with_most_kills() : nullptr;
		if (!Game::level.continue_match_after_death
			&& (match_time > Game::session.config.time_limit()
			|| (Game::level.has_feature(Game::FeatureLevel::All) && Team::teams_with_active_players() <= 1)
			|| (Game::session.config.game_type == GameType::Assault && CoreModule::count(1 << 0) == 0)
			|| (Game::session.config.game_type == GameType::Deathmatch && team_with_most_kills && team_with_most_kills->kills >= Game::session.config.kill_limit)))
		{
			// determine the winner, if any
			Team* w = nullptr;
			Team* team_with_player = nullptr;
			s32 teams_with_players = 0;
			for (auto i = list.iterator(); !i.is_last(); i.next())
			{
				if (i.item()->has_active_player())
				{
					team_with_player = i.item();
					teams_with_players++;
				}
			}

			if (teams_with_players == 1)
				w = team_with_player;
			else if (Game::session.config.game_type == GameType::Deathmatch)
				w = team_with_most_kills;
			else if (Game::session.config.game_type == GameType::Assault)
			{
				if (CoreModule::count(1 << 0) == 0)
					w = &list[1]; // attackers win
				else
					w = &list[0]; // defenders win
			}

			// remove entities
			for (auto i = PlayerCommon::list.iterator(); !i.is_last(); i.next())
			{
				Vec3 pos;
				Quat rot;
				i.item()->get<Transform>()->absolute(&pos, &rot);
				ParticleEffect::spawn(ParticleEffect::Type::Explosion, pos, rot);
				World::remove_deferred(i.item()->entity());
			}

			for (auto i = ForceField::list.iterator(); !i.is_last(); i.next())
			{
				if (!(i.item()->flags & ForceField::FlagPermanent))
				{
					Vec3 pos;
					Quat rot;
					i.item()->get<Transform>()->absolute(&pos, &rot);
					ParticleEffect::spawn(ParticleEffect::Type::Explosion, pos, rot);
					World::remove_deferred(i.item()->entity());
				}
			}

			TeamNet::send_game_over(w);

			if (Game::session.type == SessionType::Story)
			{
				// we're in story mode, give the player whatever stuff they have leftover
				if (PlayerHuman::list.count() > 0)
				{
					PlayerManager* player = PlayerHuman::list.iterator().item()->get<PlayerManager>();
					Overworld::resource_change(Resource::Energy, player->energy);
					Overworld::resource_change(Resource::Drones, vi_max(s16(0), player->respawns));
				}

				if (w == &Team::list[1]) // attackers won; the zone is going to change owners
				{
					if (Game::save.zones[Game::level.id] == ZoneState::PvpFriendly) // player was defending
						Overworld::zone_change(Game::level.id, ZoneState::PvpHostile);
					else // player was attacking
						Overworld::zone_change(Game::level.id, ZoneState::PvpFriendly);
				}
			}
		}
	}

	if (game_over && Game::scheduled_load_level == AssetNull && transition_mode_scheduled == Game::Mode::None)
	{
		// wait for all local players to accept scores
		b8 score_accepted = true;
		for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
		{
			if (!i.item()->get<PlayerManager>()->score_accepted)
			{
				score_accepted = false;
				break;
			}
		}

		if (score_accepted)
			transition_next(); // time to get out of here
	}

	update_visibility(u);
}

void Team::transition_mode(Game::Mode m)
{
	vi_assert(Game::level.local);
	transition_mode_scheduled = m;
}

void Team::draw_ui(const RenderParams& params)
{
	if (transition_timer > 0.0f)
		Menu::draw_letterbox(params, transition_timer, TRANSITION_TIME);
}

void Team::update(const Update& u)
{
	transition_timer = vi_max(0.0f, transition_timer - Game::real_time.delta);
	if (Game::level.local)
		update_all_server(u);
	else
		update_all_client_only(u);
}

void Team::update_all_client_only(const Update& u)
{
	if (Game::level.mode != Game::Mode::Pvp)
		return;

	update_visibility(u);

	if (game_over
		&& Game::scheduled_load_level == AssetNull
		&& Game::session.type == SessionType::Multiplayer)
	{
		// wait for everyone to accept scores, then quit to overworld
		b8 score_accepted = true;
		for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->is_local() && !i.item()->score_accepted)
			{
				score_accepted = false;
				break;
			}
		}
		if (score_accepted)
			Game::schedule_load_level(Asset::Level::overworld, Game::Mode::Special);
	}
}

PlayerManager::Visibility PlayerManager::visibility[MAX_PLAYERS * MAX_PLAYERS];

PlayerManager::PlayerManager(Team* team, const char* u)
	: spawn_timer((Game::session.type == SessionType::Story && team->team() == 1) ? 0 : SPAWN_DELAY), // defenders in story mode get to spawn instantly
	score_accepted(Team::game_over),
	team(team),
	upgrades(0),
	abilities{ Ability::None, Ability::None, Ability::None },
	instance(),
	spawn(),
	can_spawn(Game::level.mode == Game::Mode::Parkour || Game::session.type != SessionType::Story || Game::save.zones[Game::level.id] == ZoneState::PvpFriendly),
	current_upgrade(Upgrade::None),
	state_timer(),
	upgrade_completed(),
	respawns(Game::session.config.respawns),
	kills(),
	deaths(),
	ability_purchase_times()
{
	if (Game::level.has_feature(Game::FeatureLevel::Abilities) && Game::session.config.allow_abilities)
	{
		energy = ENERGY_INITIAL;
		if (Game::session.type == SessionType::Story && Game::session.config.game_type == GameType::Assault && team->team() == 0)
			energy += s32(Team::match_time / ENERGY_INCREMENT_INTERVAL) * (ENERGY_DEFAULT_INCREMENT * s32(Battery::list.count() * 0.75f));
	}
	else
		energy = 0;
	if (u)
		strncpy(username, u, MAX_USERNAME);
	else
		username[0] = '\0';
}

void PlayerManager::awake()
{
	if ((!Game::level.local || Game::session.type == SessionType::Story) && Game::level.mode == Game::Mode::Pvp)
	{
		char log[512];
		sprintf(log, _(strings::player_joined), username);
		PlayerHuman::log_add(log, team.ref()->team());
	}
}

PlayerManager::~PlayerManager()
{
	if ((!Game::level.local || Game::session.type == SessionType::Story) && Game::level.mode == Game::Mode::Pvp)
	{
		char log[512];
		sprintf(log, _(strings::player_left), username);
		PlayerHuman::log_add(log, team.ref()->team());
	}
}

b8 PlayerManager::has_upgrade(Upgrade u) const
{
	return upgrades & (1 << s32(u));
}

b8 PlayerManager::ability_valid(Ability ability) const
{
	if (ability == Ability::None)
		return false;

	if (!Game::level.has_feature(Game::FeatureLevel::Abilities))
		return false;

	if (!Game::session.config.allow_abilities)
		return false;

	if (!can_transition_state())
		return false;

	if (!has_upgrade(Upgrade(ability)))
		return false;

	const AbilityInfo& info = AbilityInfo::list[s32(ability)];
	if (energy < info.spawn_cost)
		return false;

	if (ability == Ability::ActiveArmor && instance.ref()->get<Health>()->invincible())
		return false;

	return true;
}

s32 PlayerManager::visibility_hash(const PlayerManager* drone_a, const PlayerManager* drone_b)
{
	return drone_a->id() * MAX_PLAYERS + drone_b->id();
}

namespace PlayerManagerNet
{
	enum class Message : s8
	{
		CanSpawn,
		ScoreAccept,
		SpawnSelect,
		UpgradeCompleted,
		UpdateCounts,
		SetInstance,
		count,
	};

	b8 send(PlayerManager* m, Message msg)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::PlayerManager);
		{
			Ref<PlayerManager> ref = m;
			serialize_ref(p, ref);
		}
		serialize_enum(p, Message, msg);
		Net::msg_finalize(p);
		return true;
	}

	b8 upgrade_completed(PlayerManager* m, s32 index, Upgrade u)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::PlayerManager);
		{
			Ref<PlayerManager> ref = m;
			serialize_ref(p, ref);
		}
		{
			Message msg = Message::UpgradeCompleted;
			serialize_enum(p, Message, msg);
		}
		serialize_int(p, s32, index, 0, MAX_ABILITIES - 1);
		serialize_enum(p, Upgrade, u);
		Net::msg_finalize(p);
		return true;
	}

	b8 spawn_select(PlayerManager* m, SpawnPoint* point)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::PlayerManager);
		{
			Ref<PlayerManager> ref = m;
			serialize_ref(p, ref);
		}
		{
			Message msg = Message::SpawnSelect;
			serialize_enum(p, Message, msg);
		}
		{
			Ref<SpawnPoint> ref = point;
			serialize_ref(p, ref);
		}
		Net::msg_finalize(p);
		return true;
	}

	b8 set_instance(PlayerManager* m, Entity* e)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::PlayerManager);
		{
			Ref<PlayerManager> ref = m;
			serialize_ref(p, ref);
		}
		{
			Message msg = Message::SetInstance;
			serialize_enum(p, Message, msg);
		}
		{
			Ref<Entity> ref = e;
			serialize_ref(p, ref);
		}
		Net::msg_finalize(p);
		return true;
	}

	b8 update_counts(PlayerManager* m)
	{
		using Stream = Net::StreamWrite;
		vi_assert(Game::level.local);
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::PlayerManager);
		{
			Ref<PlayerManager> ref = m;
			serialize_ref(p, ref);
		}
		{
			Message msg = Message::UpdateCounts;
			serialize_enum(p, Message, msg);
		}
		serialize_s16(p, m->kills);
		serialize_s16(p, m->deaths);
		serialize_s16(p, m->respawns);
		Net::msg_finalize(p);
		return true;
	}
}

void internal_spawn_go(PlayerManager* m, SpawnPoint* point)
{
	vi_assert(Game::level.local);
	if (m->respawns != -1 && Game::level.mode == Game::Mode::Pvp)
	{
		if (Game::level.has_feature(Game::FeatureLevel::All)
			|| m->respawns >= DEFAULT_ASSAULT_DRONES - 1) // in the tutorial, you can only lose two drones
		{
			m->respawns--;
			PlayerManagerNet::update_counts(m);
		}
	}
	if (m->respawns != 0)
		m->spawn_timer = SPAWN_DELAY;
	m->spawn.fire(point->spawn_position(m));
}

b8 PlayerManager::net_msg(Net::StreamRead* p, PlayerManager* m, Net::MessageSource src)
{
	using Stream = Net::StreamRead;
	PlayerManagerNet::Message msg;
	serialize_enum(p, PlayerManagerNet::Message, msg);
	if (src != Net::MessageSource::Invalid)
	{
		switch (msg)
		{
			case PlayerManagerNet::Message::CanSpawn:
			{
				m->can_spawn = true;
				break;
			}
			case PlayerManagerNet::Message::SpawnSelect:
			{
				Ref<SpawnPoint> ref;
				serialize_ref(p, ref);
				if (Game::level.local
					&& !Team::game_over
					&& m->spawn_timer == 0.0f
					&& m->respawns != 0
					&& !m->instance.ref()
					&& m->can_spawn
					&& ref.ref() && ref.ref()->team == m->team.ref()->team())
				{
#if SERVER
					if (m->has<PlayerHuman>())
					{
						AI::RecordedLife::Tag tag;
						tag.init(m);

						AI::RecordedLife::Action action;
						action.type = AI::RecordedLife::Action::TypeSpawn;
						Quat rot;
						ref.ref()->get<Transform>()->absolute(&action.pos, &rot);
						action.normal = rot * Vec3(0, 0, 1);
						AI::record_add(m->get<PlayerHuman>()->ai_record_id, tag, action);
					}
#endif
					internal_spawn_go(m, ref.ref());
				}
			}
			case PlayerManagerNet::Message::ScoreAccept:
			{
				m->score_accepted = true;
				break;
			}
			case PlayerManagerNet::Message::UpgradeCompleted:
			{
				s32 index;
				serialize_int(p, s32, index, 0, MAX_ABILITIES - 1);
				Upgrade u;
				serialize_enum(p, Upgrade, u);
				if (!Game::level.local || src == Net::MessageSource::Loopback)
				{
					m->upgrades |= 1 << s32(u);
					m->abilities[index] = Ability(u);
					m->ability_purchase_times[index] = Game::time.total;
					m->upgrade_completed.fire(u);
				}
				break;
			}
			case PlayerManagerNet::Message::UpdateCounts:
			{
				s16 kills;
				s16 deaths;
				s16 respawns;
				serialize_s16(p, kills);
				serialize_s16(p, deaths);
				serialize_s16(p, respawns);
				if (!Game::level.local || src == Net::MessageSource::Loopback) // server does not accept these messages from clients
				{
					m->kills = kills;
					m->deaths = deaths;
					m->respawns = respawns;
				}
				break;
			}
			case PlayerManagerNet::Message::SetInstance:
			{
				Ref<Entity> i;
				serialize_ref(p, i);
				if (!Game::level.local || src == Net::MessageSource::Loopback)
					m->instance = i;
				break;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}
	}
	return true;
}

b8 PlayerManager::upgrade_start(Upgrade u)
{
	s16 cost = upgrade_cost(u);
	if (can_transition_state()
		&& upgrade_available(u)
		&& energy >= cost
		&& UpgradeStation::drone_inside(instance.ref()->get<Drone>()))
	{
		current_upgrade = u;
		state_timer = UPGRADE_TIME;
		add_energy(-cost);
		return true;
	}
	return false;
}

void PlayerManager::upgrade_complete()
{
	Upgrade u = current_upgrade;
	current_upgrade = Upgrade::None;
	state_timer = 0.0f;

	vi_assert(!has_upgrade(u));

	PlayerManagerNet::upgrade_completed(this, ability_count(), u);
}

s16 PlayerManager::upgrade_cost(Upgrade u) const
{
	vi_assert(u != Upgrade::None);
	const UpgradeInfo& info = UpgradeInfo::list[s32(u)];
	return info.cost;
}

Upgrade PlayerManager::upgrade_highest_owned_or_available() const
{
	s16 highest_cost = 0;
	Upgrade highest_upgrade = Upgrade::None;
	for (s32 i = 0; i < s32(Upgrade::count); i++)
	{
		s16 cost = upgrade_cost(Upgrade(i));
		if (cost > highest_cost && energy >= cost && (upgrade_available(Upgrade(i)) || has_upgrade(Upgrade(i))))
		{
			highest_cost = cost;
			highest_upgrade = Upgrade(i);
		}
	}
	return highest_upgrade;
}

b8 PlayerManager::upgrade_available(Upgrade u) const
{
	if (u == Upgrade::None)
	{
		for (s32 i = 0; i < s32(Upgrade::count); i++)
		{
			if (!has_upgrade(Upgrade(i)) && energy >= upgrade_cost(Upgrade(i)))
			{
				if (i >= s32(Ability::count) || ability_count() < MAX_ABILITIES)
					return true; // either it's not an ability, or it is an ability and we have enough room for it
			}
		}
		return false;
	}
	else
	{
		// make sure that either it's not an ability, or it is an ability and we have enough room for it
		return !has_upgrade(u) && (s32(u) >= s32(Ability::count) || ability_count() < MAX_ABILITIES);
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

void PlayerManager::add_energy(s32 c)
{
	if (Game::level.local)
		energy = s16(vi_max(0, s32(energy) + c));
}

void PlayerManager::add_energy_and_notify(s32 c)
{
	add_energy(c);
	if (has<PlayerHuman>())
		get<PlayerHuman>()->energy_notify(c);
}

void PlayerManager::add_kills(s32 k)
{
	vi_assert(Game::level.local);
	kills += k;
	PlayerManagerNet::update_counts(this);
}

void PlayerManager::add_deaths(s32 d)
{
	vi_assert(Game::level.local);
	deaths += d;
	PlayerManagerNet::update_counts(this);
}

void PlayerManager::set_instance(Entity* e)
{
	PlayerManagerNet::set_instance(this, e);
}

PlayerManager::State PlayerManager::state() const
{
	if (current_upgrade == Upgrade::None)
		return State::Default;
	else
		return State::Upgrading;
}

b8 PlayerManager::can_transition_state() const
{
	if (!Game::level.has_feature(Game::FeatureLevel::Abilities) || !Game::session.config.allow_abilities)
		return false;

	Entity* e = instance.ref();
	if (!e)
		return false;

	State s = state();
	if (s != State::Default)
		return false;

	return e->get<Drone>()->state() == Drone::State::Crawl;
}

void PlayerManager::update_all(const Update& u)
{
	if (Game::level.local)
	{
		if (Game::level.mode == Game::Mode::Pvp
			&& Game::level.has_feature(Game::FeatureLevel::Batteries))
		{
			for (auto i = list.iterator(); !i.is_last(); i.next())
			{
				r32 interval_per_point = ENERGY_INCREMENT_INTERVAL / i.item()->team.ref()->increment();
				s32 index = s32((Team::match_time - u.time.delta) / interval_per_point);
				while (index < s32(Team::match_time / interval_per_point))
				{
					// give points to players based on how many control points they own
					i.item()->add_energy(1);
					index++;
				}
			}
		}
	}

	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (Game::level.local)
			i.item()->update_server(u);
		i.item()->update_client(u);
	}
}

PlayerManager* PlayerManager::owner(Entity* e)
{
	if (e->has<PlayerCommon>())
		return e->get<PlayerCommon>()->manager.ref();
	else if (e->has<Minion>())
		return e->get<Minion>()->owner.ref();
	else if (e->has<Bolt>())
		return e->get<Bolt>()->player.ref();
	else if (e->has<Grenade>())
		return e->get<Grenade>()->owner.ref();
	return nullptr;
}

void PlayerManager::entity_killed_by(Entity* e, Entity* killer)
{
	if (killer)
	{
		AI::Team team;
		AI::entity_info(e, AI::TeamNone, &team);

		AI::Team killer_team;
		AI::entity_info(killer, AI::TeamNone, &killer_team);

		if (killer_team != team)
		{
			PlayerManager* player = owner(e);
			PlayerManager* killer_player = owner(killer);

			s32 reward = 0;
			b8 reward_share = false; // do all players on the team get the reward?

			if (e->has<Drone>())
			{
				{
					char buffer[512];
					sprintf(buffer, _(strings::player_killed), player->username);
					PlayerHuman::log_add(buffer, e->get<AIAgent>()->team);
				}

				if (Game::level.local)
				{
					if (killer_player)
						killer_player->add_kills(1);
					Team::list[killer_team].add_kills(1);
					player->add_deaths(1);
				}

				reward = ENERGY_DRONE_DESTROY;
			}
			else if (e->has<Minion>())
				reward = ENERGY_MINION_KILL;
			else if (e->has<Grenade>())
				reward = ENERGY_GRENADE_DESTROY;
			else if (e->has<ForceField>())
				reward = ENERGY_FORCE_FIELD_DESTROY;
			else if (e->has<Sensor>())
				reward = ENERGY_SENSOR_DESTROY;
			else if (e->has<Turret>())
			{
				reward = ENERGY_TURRET_DESTROY;
				reward_share = true;
			}
			else if (e->has<CoreModule>())
			{
				reward = ENERGY_CORE_MODULE_DESTROY;
				reward_share = true;
			}
			else
				vi_assert(false);

			if (reward_share)
			{
				for (auto i = list.iterator(); !i.is_last(); i.next())
				{
					if (i.item()->team.ref()->team() == killer_team)
						i.item()->add_energy_and_notify(reward);
				}
			}
			else if (killer_player)
				killer_player->add_energy_and_notify(reward);
		}
	}
}

void PlayerManager::update_server(const Update& u)
{
	if (can_spawn
		&& !instance.ref()
		&& !Team::game_over
		&& !Game::level.continue_match_after_death)
	{
		if (Game::level.mode == Game::Mode::Pvp)
		{
			if (spawn_timer > 0.0f && respawns != 0)
			{
				spawn_timer = vi_max(0.0f, spawn_timer - u.time.delta);
				if (spawn_timer == 0.0f)
				{
					if (SpawnPoint::count(1 << s32(team.ref()->team())) == 1)
						internal_spawn_go(this, SpawnPoint::first(1 << s32(team.ref()->team())));
				}
			}
		}
		else if (Game::level.mode == Game::Mode::Parkour)
			internal_spawn_go(this, SpawnPoint::first(1 << s32(team.ref()->team())));
	}

	State s = state();

	if (state_timer > 0.0f)
	{
		// something is in progress
		state_timer = vi_max(0.0f, state_timer - u.time.delta);
		if (state_timer == 0.0f)
		{
			switch (s)
			{
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

void PlayerManager::update_client(const Update& u)
{
}

b8 PlayerManager::is_local() const
{
	for (auto j = PlayerHuman::list.iterator(); !j.is_last(); j.next())
	{
		if (j.item()->get<PlayerManager>() == this)
			return true;
	}
	return false;
}

void PlayerManager::set_can_spawn()
{
	PlayerManagerNet::send(this, PlayerManagerNet::Message::CanSpawn);
}

void PlayerManager::score_accept()
{
	PlayerManagerNet::send(this, PlayerManagerNet::Message::ScoreAccept);
}

void PlayerManager::spawn_select(SpawnPoint* point)
{
	PlayerManagerNet::spawn_select(this, point);
}


}
