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
		AbilityInfo::Type::Build,
		Asset::Mesh::icon_minion,
		20,
		false,
	},
	{
		AbilityInfo::Type::Shoot,
		Asset::Mesh::icon_bolter,
		8,
		true,
	},
	{
		AbilityInfo::Type::Other,
		Asset::Mesh::icon_active_armor,
		10,
		false,
	},
	{
		AbilityInfo::Type::Build,
		Asset::Mesh::icon_sensor,
		15,
		false,
	},
	{
		AbilityInfo::Type::Build,
		Asset::Mesh::icon_drone,
		20,
		false,
	},
	{
		AbilityInfo::Type::Build,
		Asset::Mesh::icon_force_field,
		40,
		false,
	},
	{
		AbilityInfo::Type::Build,
		Asset::Mesh::icon_rocket,
		10,
		false,
	},
	{
		AbilityInfo::Type::Shoot,
		Asset::Mesh::icon_sniper,
		10,
		false,
	},
	{
		AbilityInfo::Type::Shoot,
		Asset::Mesh::icon_grenade,
		20,
		false,
	},
};

UpgradeInfo UpgradeInfo::list[s32(Upgrade::count)] =
{
	{
		strings::minion,
		strings::description_minion,
		Asset::Mesh::icon_minion,
		40,
	},
	{
		strings::bolter,
		strings::description_bolter,
		Asset::Mesh::icon_bolter,
		40,
	},
	{
		strings::active_armor,
		strings::description_active_armor,
		Asset::Mesh::icon_active_armor,
		40,
	},
	{
		strings::sensor,
		strings::description_sensor,
		Asset::Mesh::icon_sensor,
		50,
	},
	{
		strings::decoy,
		strings::description_decoy,
		Asset::Mesh::icon_drone,
		50,
	},
	{
		strings::force_field,
		strings::description_force_field,
		Asset::Mesh::icon_force_field,
		120,
	},
	{
		strings::rocket,
		strings::description_rocket,
		Asset::Mesh::icon_rocket,
		120,
	},
	{
		strings::sniper,
		strings::description_sniper,
		Asset::Mesh::icon_sniper,
		120,
	},
	{
		strings::grenade,
		strings::description_grenade,
		Asset::Mesh::icon_grenade,
		120,
	},
};

Team::Team()
	: player_tracks(),
	player_spawn()
{
}

void Team::awake_all()
{
	game_over = false;
	game_over_real_time = 0.0f;
	if (Game::level.local) // if we're a client, the netcode manages this
		match_time = 0.0f;
	winner = nullptr;
	score_summary.length = 0;
	for (s32 i = 0; i < MAX_PLAYERS * MAX_PLAYERS; i++)
		PlayerManager::visibility[i] = nullptr;
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
#if SERVER
		// todo: map rotation
		Game::schedule_load_level(Game::level.id, Game::Mode::Pvp);
#else
		Game::schedule_load_level(Asset::Level::overworld, Game::Mode::Special);
#endif
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

s16 Team::kills() const
{
	s16 kills = 0;
	for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team.ref() == this)
			kills += i.item()->kills;
	}
	return kills;
}

Team* Team::with_most_kills()
{
	s16 highest_kills = 0;
	Team* result = nullptr;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		s16 kills = i.item()->kills();
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
void update_visibility_sensor(Entity* visibility[][MAX_PLAYERS], PlayerManager* player, Entity* player_entity)
{
	Quat player_rot;
	Vec3 player_pos;
	player_entity->get<Transform>()->absolute(&player_pos, &player_rot);
	player_pos += player_rot * Vec3(0, 0, -DRONE_RADIUS);
	Vec3 normal = player_rot * Vec3(0, 0, 1);
	for (auto sensor = Sensor::list.iterator(); !sensor.is_last(); sensor.next())
	{
		Entity** sensor_visibility = &visibility[player->id()][(s32)sensor.item()->team];
		if (!(*sensor_visibility))
		{
			Vec3 to_sensor = sensor.item()->get<Transform>()->absolute_pos() - player_pos;
			if (to_sensor.length_squared() < SENSOR_RANGE * SENSOR_RANGE
				&& to_sensor.dot(normal) > 0.0f)
				*sensor_visibility = player_entity;
		}
	}
}

void update_stealth_state(PlayerManager* player, AIAgent* a, Entity* visibility[][MAX_PLAYERS])
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
	if (!Sensor::can_see(a->team, player_pos, normal))
		stealth_enabled = false;
	else
	{
		// check if any enemy sensors can see us
		for (auto t = Team::list.iterator(); !t.is_last(); t.next())
		{
			if (t.item()->team() != a->team && visibility[player->id()][t.index] == a->entity())
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
	// determine which drones and decoys are seen by which teams
	// and update their stealth state
	Entity* visibility[MAX_PLAYERS][MAX_PLAYERS] = {};
	for (auto player = PlayerManager::list.iterator(); !player.is_last(); player.next())
	{
		Entity* decoy = player.item()->decoy();
		if (decoy)
		{
			update_visibility_sensor(visibility, player.item(), decoy);
			update_stealth_state(player.item(), decoy->get<AIAgent>(), visibility);
		}

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

		Entity* i_decoy = i.item()->decoy();

		for (auto j = PlayerManager::list.iterator(); !j.is_last(); j.next())
		{
			Team* j_team = j.item()->team.ref();

			if (i_team == j_team)
				continue;

			Entity* detected_entity = nullptr; // the entity i detected, if any

			// if j_decoy is at all visible, it will be detected first
			// i_decoy is also able to detect j_actual_entity and j_decoy

			Entity* j_decoy = j.item()->decoy();
			if (j_decoy && !j_decoy->get<AIAgent>()->stealth)
			{
				r32 distance;
				if ((visibility_check(i_entity, j_decoy, &distance)
					&& distance < i_range)
					|| (i_decoy
						&& visibility_check(i_decoy, j_decoy, &distance)
						&& distance < DRONE_MAX_DISTANCE))
					detected_entity = j_decoy;
			}

			if (!detected_entity)
			{
				Entity* j_actual_entity = j.item()->instance.ref();
				if (j_actual_entity && !j_actual_entity->get<AIAgent>()->stealth)
				{
					r32 distance;
					if ((visibility_check(i_entity, j_actual_entity, &distance)
						&& distance < i_range)
						|| (i_decoy
							&& visibility_check(i_decoy, j_actual_entity, &distance)
							&& distance < DRONE_MAX_DISTANCE))
						detected_entity = j_actual_entity;
				}
			}

			PlayerManager::visibility[PlayerManager::visibility_hash(i.item(), j.item())] = detected_entity;
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
				else if (detected_entity->has<Decoy>() || detected_entity->get<Drone>()->state() == Drone::State::Crawl)
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

b8 Team::net_msg(Net::StreamRead* p)
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
				AI::Team team = i.item()->team.ref()->team();
				team_add_score_summary_item(i.item(), i.item()->username);
				team_add_score_summary_item(i.item(), _(strings::kills), i.item()->kills);
				if (Game::session.type == SessionType::Story)
					team_add_score_summary_item(i.item(), _(strings::leftover_energy), i.item()->energy);
				if (PlayerManager::list.count() > 2)
					team_add_score_summary_item(i.item(), _(strings::deaths), i.item()->deaths);
			}
			break;
		}
		case TeamNet::Message::TransitionMode:
		{
			Game::Mode mode_old = Game::level.mode;
			serialize_enum(p, Game::Mode, Game::level.mode);
			game_over = false;
			transition_timer = TRANSITION_TIME * 0.5f;
			match_time = 0.0f;
			if (Game::level.mode == Game::Mode::Pvp)
				Game::level.post_pvp = true; // we have played (or are playing) a PvP match on this level
			for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
			{
				Camera* camera = i.item()->camera;
				if (camera)
				{
					Quat rot;
					Game::level.map_view.ref()->absolute(&camera->pos, &rot);
					camera->rot = Quat::look(rot * Vec3(0, -1, 0));
				}
				i.item()->get<PlayerManager>()->can_spawn = Game::level.mode == Game::Mode::Parkour;
			}
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
					for (auto i = Battery::list.iterator(); !i.is_last(); i.next())
						i.item()->set_team(team_winner); // these are synced over the network, so must do them only on the server
					for (auto i = Rocket::list.iterator(); !i.is_last(); i.next())
						World::remove_deferred(i.item()->entity());
				}
				// teams are not synced over the network, so set them on both client and server
				for (auto i = MinionCommon::list.iterator(); !i.is_last(); i.next())
					i.item()->team(team_winner);
				for (auto i = Grenade::list.iterator(); !i.is_last(); i.next())
					i.item()->set_owner(nullptr);
				for (auto i = Sensor::list.iterator(); !i.is_last(); i.next())
					i.item()->set_team(team_winner);
				TerminalEntity::open();

				Overworld::zone_done(Game::level.id);
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
		Team* team_with_most_kills = Game::level.type == GameType::Deathmatch ? with_most_kills() : nullptr;
		if (!Game::level.continue_match_after_death
			&& ((match_time > Game::level.time_limit && (Game::level.type != GameType::Rush || ControlPoint::count_capturing() == 0))
			|| (Game::level.has_feature(Game::FeatureLevel::All) && Team::teams_with_active_players() <= 1)
			|| (Game::level.type == GameType::Rush && list[1].control_point_count() == ControlPoint::list.count())
			|| (Game::level.type == GameType::Deathmatch && team_with_most_kills && team_with_most_kills->kills() >= Game::level.kill_limit)))
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
			else if (Game::level.type == GameType::Deathmatch)
				w = team_with_most_kills;
			else if (Game::level.type == GameType::Rush)
			{
				if (list[0].control_point_count() > 0)
					w = &list[0]; // defenders win
				else
					w = &list[1]; // attackers win
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
				Vec3 pos;
				Quat rot;
				i.item()->get<Transform>()->absolute(&pos, &rot);
				ParticleEffect::spawn(ParticleEffect::Type::Explosion, pos, rot);
				World::remove_deferred(i.item()->entity());
			}
			for (auto i = Decoy::list.iterator(); !i.is_last(); i.next())
			{
				Vec3 pos;
				Quat rot;
				i.item()->get<Transform>()->absolute(&pos, &rot);
				ParticleEffect::spawn(ParticleEffect::Type::Explosion, pos, rot);
				World::remove_deferred(i.item()->entity());
			}

			TeamNet::send_game_over(w);

			for (auto i = ControlPoint::list.iterator(); !i.is_last(); i.next())
				i.item()->set_team(w->team());

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
					if (Game::save.zones[Game::level.id] == ZoneState::Friendly) // player was defending
						Overworld::zone_change(Game::level.id, ZoneState::Hostile);
					else // player was attacking
						Overworld::zone_change(Game::level.id, ZoneState::Friendly);
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

	// rockets
	if (!game_over)
	{
		for (auto t = Team::list.iterator(); !t.is_last(); t.next())
		{
			Team* team = t.item();

			for (auto player = PlayerManager::list.iterator(); !player.is_last(); player.next())
			{
				AI::Team player_team = player.item()->team.ref()->team();
				if (team->team() == player_team)
					continue;

				// launch a rocket at this player if the conditions are right
				const Team::SensorTrack& track = team->player_tracks[player.index];
				for (auto rocket = Rocket::list.iterator(); !rocket.is_last(); rocket.next())
				{
					if (rocket.item()->team() == team->team() // it belongs to our team
						&& rocket.item()->get<Transform>()->parent.ref()) // it's waiting to be fired
					{
						Entity* detected_entity = nullptr; // we're tracking the player, or the owner is alive and can see the player
						if (track.tracking)
							detected_entity = track.entity.ref();
						else
						{
							Entity* e = PlayerManager::visibility[PlayerManager::visibility_hash(rocket.item()->owner.ref(), player.item())].ref();
							if (e && (!e->has<Drone>() || e->get<Drone>()->state() == Drone::State::Crawl)) // only launch rockets at drones that are crawling; this prevents rockets from launching and immediately losing their target
								detected_entity = e;
						}

						if (detected_entity && !Rocket::inbound(detected_entity))
						{
							Vec3 target_pos = detected_entity->get<Transform>()->absolute_pos();
							Vec3 rocket_pos = rocket.item()->get<Transform>()->absolute_pos();
							if ((rocket_pos - target_pos).length_squared() < ROCKET_RANGE * ROCKET_RANGE // it's in range
								&& ForceField::hash(team->team(), rocket_pos) == ForceField::hash(team->team(), target_pos)) // no force fields in the way
							{
								rocket.item()->launch(detected_entity);
								break;
							}
						}
					}
				}
			}
		}
	}
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
}

PlayerSpawnPosition PlayerManager::spawn_position() const
{
	PlayerSpawnPosition result;
	Quat rot;
	team.ref()->player_spawn.ref()->absolute(&result.pos, &rot);
	Vec3 dir = rot * Vec3(0, 1, 0);
	result.angle = atan2f(dir.x, dir.z);

	if (team.ref()->player_count() > 1)
		result.pos += Quat::euler(0, result.angle + (id() * PI * 0.5f), 0) * Vec3(0, 0, CONTROL_POINT_RADIUS * 0.5f); // spawn it around the edges
	return result;
}

b8 PlayerManager::has_upgrade(Upgrade u) const
{
	return upgrades & (1 << (s32)u);
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
	if (energy < info.spawn_cost)
		return false;

	if (ability == Ability::ActiveArmor && instance.ref()->get<Drone>()->invincible_timer > 0.0f)
		return false;

	return true;
}

Ref<Entity> PlayerManager::visibility[MAX_PLAYERS * MAX_PLAYERS];

s32 PlayerManager::visibility_hash(const PlayerManager* drone_a, const PlayerManager* drone_b)
{
	return drone_a->id() * MAX_PLAYERS + drone_b->id();
}

PlayerManager::PlayerManager(Team* team, const char* u)
	: spawn_timer(PLAYER_SPAWN_DELAY),
	score_accepted(Team::game_over),
	team(team),
	upgrades(0),
	abilities{ Ability::None, Ability::None, Ability::None },
	instance(),
	spawn(),
	can_spawn(Game::level.mode == Game::Mode::Parkour || Game::session.type != SessionType::Story || Game::save.zones[Game::level.id] == ZoneState::Friendly),
	current_upgrade(Upgrade::None),
	state_timer(),
	upgrade_completed(),
	control_point_capture_completed(),
	particle_accumulator(),
	respawns(Game::level.respawns),
	kills(),
	deaths()
{
	if (Game::level.has_feature(Game::FeatureLevel::Abilities))
	{
		energy = ENERGY_INITIAL;
		if (Game::session.type == SessionType::Story && Game::level.type == GameType::Rush && team->team() == 0)
			energy += s32(Team::match_time / ENERGY_INCREMENT_INTERVAL) * (ENERGY_DEFAULT_INCREMENT * s32(Battery::list.count() * 0.75f));
	}
	else
		energy = 0;
	energy_last = energy;
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

b8 PlayerManager::upgrade_start(Upgrade u)
{
	s16 cost = upgrade_cost(u);
	if (can_transition_state()
		&& upgrade_available(u)
		&& energy >= cost
		&& at_upgrade_point())
	{
		current_upgrade = u;
		state_timer = UPGRADE_TIME;
		add_energy(-cost);
		return true;
	}
	return false;
}

void PlayerManager::upgrade_cancel()
{
	if (state() == State::Upgrading)
	{
		current_upgrade = Upgrade::None;
		state_timer = 0.0f;
	}
}

void PlayerManager::upgrade_complete()
{
	Upgrade u = current_upgrade;
	current_upgrade = Upgrade::None;

	vi_assert(!has_upgrade(u));

	upgrades |= 1 << s32(u);

	if (s32(u) < s32(Ability::count))
	{
		// it's an ability
		abilities[ability_count()] = Ability(u);
	}

	upgrade_completed.fire(u);
}

b8 PlayerManager::capture_start()
{
	ControlPoint* control_point = at_control_point();
	if (can_transition_state() && control_point && control_point->can_be_captured_by(team.ref()->team()))
	{
		vi_assert(current_upgrade == Upgrade::None);
		instance.ref()->get<Drone>()->current_ability = Ability::None;
		state_timer = CAPTURE_TIME;
		return true;
	}
	return false;
}

void PlayerManager::capture_cancel()
{
	if (state() == State::Capturing)
	{
		control_point_capture_completed.fire(false);
		state_timer = 0.0f;
	}
}

// the capture is not actually complete; we've completed the process of *starting* to capture the point
void PlayerManager::capture_complete()
{
	if (!instance.ref())
		return;

	b8 success = false;
	ControlPoint* control_point = at_control_point();
	if (control_point && control_point->can_be_captured_by(team.ref()->team()))
	{
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
				control_point->capture_start(team.ref()->team()); // start capturing again
		}
		success = true;
	}

	control_point_capture_completed.fire(success);
}

s16 PlayerManager::upgrade_cost(Upgrade u) const
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
			if (!has_upgrade((Upgrade)i) && energy >= upgrade_cost((Upgrade)i))
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

// returns the difference actually applied (never goes below 0)
s32 PlayerManager::add_energy(s32 c)
{
	if (c != 0)
	{
		s16 old_energy = energy;
		energy = s16(vi_max(0, s32(energy) + c));
		return energy - old_energy;
	}
	return 0;
}

void PlayerManager::add_kills(s32 k)
{
	kills += k;
}

void PlayerManager::add_deaths(s32 d)
{
	deaths += d;
}

b8 PlayerManager::at_upgrade_point() const
{
	return team.ref()->player_spawn.ref()->get<PlayerTrigger>()->is_triggered(instance.ref());
}

ControlPoint* PlayerManager::at_control_point() const
{
	Entity* e = instance.ref();
	if (e && (!e->has<Drone>() || e->get<Drone>()->state() == Drone::State::Crawl))
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
		if (current_upgrade == Upgrade::None)
			return State::Capturing;
		else
			return State::Upgrading;
	}
}

b8 PlayerManager::can_transition_state() const
{
	if (!Game::level.has_feature(Game::FeatureLevel::Abilities))
		return false;

	Entity* e = instance.ref();
	if (!e)
		return false;

	State s = state();
	if (s != State::Default)
		return false;

	return e->get<Drone>()->state() == Drone::State::Crawl;
}

s16 PlayerManager::increment() const
{
	return ENERGY_DEFAULT_INCREMENT
		+ Battery::count(1 << team.ref()->team()) * ENERGY_ENERGY_PICKUP;
}

void PlayerManager::update_all(const Update& u)
{
	if (Game::level.local)
	{
		if (Game::level.mode == Game::Mode::Pvp
			&& Game::level.has_feature(Game::FeatureLevel::Batterys))
		{
			for (auto i = list.iterator(); !i.is_last(); i.next())
			{
				r32 interval_per_point = ENERGY_INCREMENT_INTERVAL / i.item()->increment();
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

void PlayerManager::update_server(const Update& u)
{
	if (Game::level.mode == Game::Mode::Pvp)
	{
		if (!instance.ref()
			&& can_spawn
			&& spawn_timer > 0.0f
			&& team.ref()->player_spawn.ref()
			&& respawns != 0
			&& !Team::game_over
			&& !Game::level.continue_match_after_death)
		{
			spawn_timer = vi_max(0.0f, spawn_timer - u.time.delta);
			if (spawn_timer == 0.0f)
			{
				if (respawns != -1 && Game::level.has_feature(Game::FeatureLevel::All)) // infinite respawns in the tutorial
					respawns--;
				if (respawns != 0)
					spawn_timer = PLAYER_SPAWN_DELAY;
				spawn.fire(spawn_position());
			}
		}
	}
	else if (Game::level.mode == Game::Mode::Parkour)
	{
		if (!instance.ref()
			&& !Team::game_over
			&& !Game::level.continue_match_after_death)
		{
			spawn.fire(spawn_position());
		}
	}

	State s = state();

	if (state_timer > 0.0f)
	{
		// something is in progress
		if (!instance.ref()) // we got killed; cancel whatever we were doing
		{
			switch (s)
			{
				case State::Capturing:
				{
					capture_cancel();
					break;
				}
				case State::Upgrading:
				{
					upgrade_cancel();
					break;
				}
				default:
				{
					vi_assert(false);
					break;
				}
			}
		}
		else
		{
			// still alive
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

Entity* PlayerManager::decoy() const
{
	for (auto i = Decoy::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->owner.ref() == this)
			return i.item()->entity();
	}
	return nullptr;
}

namespace PlayerManagerNet
{
	enum class Message
	{
		CanSpawn,
		ScoreAccept,
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
}

void PlayerManager::set_can_spawn()
{
	PlayerManagerNet::send(this, PlayerManagerNet::Message::CanSpawn);
}

void PlayerManager::score_accept()
{
	PlayerManagerNet::send(this, PlayerManagerNet::Message::ScoreAccept);
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
			case PlayerManagerNet::Message::ScoreAccept:
			{
				m->score_accepted = true;
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


}