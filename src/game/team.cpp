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
#include "common.h"

namespace VI
{


namespace TeamNet
{
	b8 update_counts(Team*);
}

const Vec4 Team::color_friend = Vec4(0.15f, 0.45f, 0.7f, MATERIAL_NO_OVERRIDE);
const Vec4 Team::color_enemy = Vec4(1.0f, 0.3f, 0.4f, MATERIAL_NO_OVERRIDE);

const Vec4 ui_color_friend_pvp = Vec4(0.35f, 0.85f, 1.0f, 1);
const Vec4 ui_color_friend_normal = Vec4(0.0f / 255.0f, 232.0f / 255.0f, 202.0f / 255.0f, 1);
const Vec4& Team::ui_color_friend()
{
	return Overworld::pvp_colors() ? ui_color_friend_pvp : ui_color_friend_normal;
}
const Vec4 ui_color_enemy_pvp = Vec4(1.0f, 0.4f, 0.4f, 1);
const Vec4 ui_color_enemy_normal = Vec4(255.0f / 255.0f, 115.0f / 255.0f, 200.0f / 255.0f, 1);
const Vec4& Team::ui_color_enemy()
{
	return Overworld::pvp_colors() ? ui_color_enemy_pvp : ui_color_enemy_normal;
}

r32 Team::control_point_timer;
r32 Team::game_over_real_time;
Ref<Team> Team::winner;
StaticArray<Team::ScoreSummaryItem, MAX_PLAYERS * PLAYER_SCORE_SUMMARY_ITEMS> Team::score_summary;
r32 Team::transition_timer;
r32 Team::match_time;
r32 Team::core_module_delay;
Team::MatchState Team::match_state;

static const AssetID team_select_names[MAX_TEAMS] =
{
	strings::team_select_a,
	strings::team_select_b,
	strings::team_select_c,
	strings::team_select_d,
};

AssetID Team::name_selector(AI::Team t)
{
	if (Game::session.config.game_type == GameType::Assault)
	{
		vi_assert(t < 2);
		return t == 0 ? strings::defend : strings::attack;
	}
	else
	{
		return team_select_names[t];
	}
}

static const AssetID team_long_names[MAX_TEAMS] =
{
	strings::team_a,
	strings::team_b,
	strings::team_c,
	strings::team_d,
};

AssetID Team::name_long(AI::Team t)
{
	if (Game::session.config.game_type == GameType::Assault)
	{
		vi_assert(t < 2);
		return t == 0 ? strings::defend : strings::attack;
	}
	else
		return team_long_names[t];
}

AbilityInfo AbilityInfo::list[s32(Ability::count) + 1] =
{
	{
		0.275f, // movement cooldown
		0.3f, // switch cooldown
		0.0f, // use cooldown
		0.7f, // recoil velocity
		AK::EVENTS::PLAY_EQUIP_BOLTER,
		Asset::Mesh::icon_bolter,
		Type::Shoot,
	},
	{
		2.75f, // cooldown
		0.0f, // switch cooldown
		0.0f, // use cooldown
		0.0f, // recoil velocity
		AK::EVENTS::PLAY_DRONE_ACTIVE_ARMOR,
		Asset::Mesh::icon_active_armor,
		Type::Other,
	},
	{
		DRONE_COOLDOWN_MAX, // movement cooldown
		0.3f, // switch cooldown
		6.0f, // use cooldown
		0.5f, // recoil velocity
		AK::EVENTS::PLAY_EQUIP_BUILD,
		Asset::Mesh::icon_rectifier,
		Type::Build,
	},
	{
		0.0f, // cooldown
		0.0f, // switch cooldown
		0.0f, // use cooldown
		0.0f, // recoil velocity
		AK_InvalidID,
		Asset::Mesh::icon_minion,
		Type::Passive,
	},
	{
		2.5f, // movement cooldown
		0.4f, // switch cooldown
		0.0f, // use cooldown
		1.7f, // recoil velocity
		AK::EVENTS::PLAY_EQUIP_SHOTGUN,
		Asset::Mesh::icon_shotgun,
		Type::Shoot,
	},
	{
		2.5f, // movement cooldown
		0.5f, // switch cooldown
		0.0f, // use cooldown
		1.5f, // recoil velocity
		AK::EVENTS::PLAY_EQUIP_SNIPER,
		Asset::Mesh::icon_sniper,
		Type::Shoot,
	},
	{
		DRONE_COOLDOWN_MAX, // movement cooldown
		0.5f, // switch cooldown
		30.0f, // use cooldown
		0.5f, // recoil velocity
		AK::EVENTS::PLAY_EQUIP_BUILD,
		Asset::Mesh::icon_force_field,
		Type::Build,
	},
	{
		DRONE_COOLDOWN_MAX, // movement cooldown
		0.3f, // switch cooldown
		6.0f, // use cooldown
		1.0f, // recoil velocity
		AK::EVENTS::PLAY_EQUIP_GRENADE,
		Asset::Mesh::icon_grenade,
		Type::Shoot,
	},
	{ // Ability::None
		1.2f + (DRONE_MAX_DISTANCE / DRONE_FLY_SPEED), // movement cooldown
		0.0f, // switch cooldown
		0.0f, // use cooldown
		0.0f, // recoil velocity
		AK::EVENTS::PLAY_EQUIP_NONE,
		Asset::Mesh::icon_chevron,
		Type::Shoot,
	},
};

UpgradeInfo UpgradeInfo::list[s32(Upgrade::count)] =
{
	{
		strings::bolter,
		strings::description_bolter,
		Asset::Mesh::icon_bolter,
		100,
		Type::Ability,
	},
	{
		strings::active_armor,
		strings::description_active_armor,
		Asset::Mesh::icon_active_armor,
		100,
		Type::Ability,
	},
	{
		strings::rectifier,
		strings::description_rectifier,
		Asset::Mesh::icon_rectifier,
		250,
		Type::Ability,
	},
	{
		strings::minion_boost,
		strings::description_minion_boost,
		Asset::Mesh::icon_minion,
		250,
		Type::Ability,
	},
	{
		strings::shotgun,
		strings::description_shotgun,
		Asset::Mesh::icon_shotgun,
		350,
		Type::Ability,
	},
	{
		strings::sniper,
		strings::description_sniper,
		Asset::Mesh::icon_sniper,
		350,
		Type::Ability,
	},
	{
		strings::force_field,
		strings::description_force_field,
		Asset::Mesh::icon_force_field,
		500,
		Type::Ability,
	},
	{
		strings::grenade,
		strings::description_grenade,
		Asset::Mesh::icon_grenade,
		500,
		Type::Ability,
	},
};

void Team::awake_all()
{
	game_over_real_time = 0.0f;
	if (Game::level.local) // if we're a client, the netcode manages this
	{
		match_state = Game::level.mode == Game::Mode::Pvp ? MatchState::Waiting : MatchState::Active;
		match_time = 0.0f;
		core_module_delay = CORE_MODULE_DELAY;
	}

	winner = nullptr;
	score_summary.length = 0;
	for (s32 i = 0; i < MAX_PLAYERS * MAX_PLAYERS; i++)
		PlayerManager::visibility[i] = { nullptr };
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

Team* Team::with_least_players(s32* player_count)
{
	Team* result = nullptr;
	s32 least_players = MAX_PLAYERS + 1;
	for (auto i = Team::list.iterator(); !i.is_last(); i.next())
	{
		s32 player_count = i.item()->player_count();
		if (player_count < least_players)
		{
			least_players = player_count;
			result = i.item();
		}
	}
	if (player_count)
	{
		if (result)
			*player_count = least_players;
		else
			*player_count = 0;
	}
	return result;
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
		if (i.item()->team.ref() == this)
			return true;
	}

	return false;
}

void Team::transition_next()
{
	vi_assert(Game::level.local);
	if (Game::session.type == SessionType::Story)
	{
#if SERVER
		Game::unload_level(); // disconnect any connected players
#else
		Game::schedule_load_level(Game::save.zone_current, Game::Mode::Parkour);
#endif
	}
	else
	{
		// multiplayer
#if SERVER
		Net::Server::transition_level();
#endif
		if (Game::level.config_scheduled_apply)
		{
			Game::session.config = Game::level.config_scheduled;
			Game::level.config_scheduled_apply = false;
		}

		if (Game::level.multiplayer_level_scheduled == AssetNull)
			Game::level.multiplayer_level_schedule();
		Game::schedule_load_level(Game::level.multiplayer_level_scheduled, Game::Mode::Pvp);
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

	RectifierTrack* track = &player_tracks[player->id()];
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
	TeamNet::update_counts(this);
}

r32 Team::minion_spawn_rate() const
{
	r32 rate = Game::session.config.game_type == GameType::Assault ? 1.0f : 0.0f;
	for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->has_ability(Ability::MinionBoost))
		{
			if (rate == 0.0f)
				rate = 1.0f;
			else
				rate *= 1.2f;
		}
	}
	return vi_min(2.0f, rate);
}

s16 Team::increment() const
{
	s16 increment = ENERGY_DEFAULT_INCREMENT;

	// batteries (that may or may not be rectifiers)
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

Team* Team::with_most_flags()
{
	s16 highest_flags = 0;
	Team* result = nullptr;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		s16 flags = i.item()->flags_captured;
		if (flags == highest_flags)
			result = nullptr;
		else if (flags > highest_flags)
		{
			highest_flags = flags;
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
		Physics::raycast(&ray_callback, CollisionAudio);
		if (!ray_callback.hasHit())
		{
			*distance = sqrtf(dist_sq);
			return true;
		}
	}

	return false;
}

// determine which rectifiers can see the given player
void update_visibility_rectifier(Entity* visibility[][MAX_TEAMS], PlayerManager* player, Entity* player_entity)
{
	Quat player_rot;
	Vec3 player_pos;
	player_entity->get<Transform>()->absolute(&player_pos, &player_rot);
	player_pos += player_rot * Vec3(0, 0, -DRONE_RADIUS);
	Vec3 normal = player_rot * Vec3(0, 0, 1);
	for (auto i = Rectifier::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team != AI::TeamNone)
		{
			Entity** entry = &visibility[player->id()][s32(i.item()->team)];
			if (!(*entry))
			{
				Vec3 to_rectifier = i.item()->get<Transform>()->absolute_pos() - player_pos;
				if (to_rectifier.length_squared() < RECTIFIER_RANGE * RECTIFIER_RANGE
					&& to_rectifier.dot(normal) > 0.0f)
					*entry = player_entity;
			}
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

	// if we are within range of their own rectifiers
	// and not detected by enemy rectifiers
	// then we should be stealthed
	b8 stealth_enabled = true;
	UpgradeStation* upgrade_station = UpgradeStation::drone_inside(a->get<Drone>());
	if (upgrade_station && upgrade_station->timer == 0.0f) // always stealthed inside upgrade stations (but not while transitioning)
		stealth_enabled = true;
	else if (Game::time.total - a->get<Drone>()->last_ability_fired < ABILITY_UNSTEALTH_TIME)
		stealth_enabled = false;
	else if (!Rectifier::can_see(1 << a->team, player_pos, normal))
		stealth_enabled = false;
	else
	{
		// check if any enemy rectifiers can see us
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
	// update stealth states of rectifiers
	for (auto i = Rectifier::list.iterator(); !i.is_last(); i.next())
	{
		if (!i.item()->has<Battery>())
		{
			Vec3 pos;
			Quat rot;
			i.item()->get<Transform>()->absolute(&pos, &rot);
			Vec3 normal = rot * Vec3(0, 0, 1);
			i.item()->set_stealth(!Rectifier::can_see(~(1 << i.item()->team), pos - normal * RECTIFIER_RADIUS, normal));
		}
	}

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
				update_visibility_rectifier(visibility, player.item(), player_entity);
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

			PlayerManager::Visibility detected = { nullptr };

			Entity* j_actual_entity = j.item()->instance.ref();
			if (j_actual_entity && !j_actual_entity->get<AIAgent>()->stealth)
			{
				// i_entity detecting j_actual_entity
				r32 distance;
				if ((visibility_check(i_entity, j_actual_entity, &distance)
					&& distance < i_range))
					detected.entity = j_actual_entity;
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
			Team::RectifierTrack* track = &team->player_tracks[player.index];
			if (detected_entity)
			{
				// team's rectifiers are picking up the Drone
				if (track->entity.ref() == detected_entity)
				{
					if (track->tracking)
						track->timer = RECTIFIER_LINGER_TIME; // this is how much time we'll continue to track them after we can no longer detect them
					else
					{
						// tracking but not yet alerted
						track->timer += u.time.delta;
						if (track->timer >= RECTIFIER_TRACK_TIME)
							team->track(player.item(), detected_entity);
					}
				}
				else if (detected_entity->get<Drone>()->state() == Drone::State::Crawl)
				{
					// not tracking yet; insert new track entry
					// (only start tracking if the Drone is attached to a wall; don't start tracking if Drone is mid-air)

					new (track) Team::RectifierTrack();
					track->entity = detected_entity;
				}
			}
			else
			{
				// team's rectifiers don't see the Drone
				// done tracking
				if (track->tracking && track->entity.ref() && track->timer > 0.0f) // track still remains active for RECTIFIER_LINGER_TIME seconds
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
		MatchState,
		UpdateCounts,
		MapSchedule,
		CoreVulnerable,
		Spot,
		count,
	};

	b8 send_match_state(Team::MatchState s, Team* w = nullptr)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::Team);
		{
			Message type = Message::MatchState;
			serialize_enum(p, Message, type);
		}
		serialize_enum(p, Team::MatchState, s);
		if (s == Team::MatchState::Done)
		{
			Ref<Team> ref = w;
			serialize_ref(p, ref);
		}
		Net::msg_finalize(p);
		return true;
	}

	b8 update_counts(Team* t)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::Team);
		{
			Message type = Message::UpdateCounts;
			serialize_enum(p, Message, type);
		}
		{
			Ref<Team> ref = t;
			serialize_ref(p, ref);
		}
		serialize_s16(p, t->kills);
		serialize_s16(p, t->flags_captured);
		Net::msg_finalize(p);
		return true;
	}

	b8 map_schedule(AssetID id)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::Team);
		{
			Message type = Message::MapSchedule;
			serialize_enum(p, Message, type);
		}
		serialize_s16(p, id);
		Net::msg_finalize(p);
		return true;
	}

	b8 core_vulnerable()
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::Team);
		{
			Message type = Message::CoreVulnerable;
			serialize_enum(p, Message, type);
		}
		Net::msg_finalize(p);
		return true;
	}

	b8 spot(Team* team, Target* t)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::Team);
		{
			Message type = Message::Spot;
			serialize_enum(p, Message, type);
		}
		{
			Ref<Team> ref = team;
			serialize_ref(p, ref);
		}
		{
			Ref<Target> ref = t;
			serialize_ref(p, ref);
		}
		Net::msg_finalize(p);
		return true;
	}
}

void Team::match_start()
{
	vi_assert(Game::level.local && (match_state == MatchState::Waiting || match_state == MatchState::Active || match_state == MatchState::TeamSelect));
	if (match_state != MatchState::Active)
	{
		TeamNet::send_match_state(MatchState::Active);
#if SERVER
		Net::Server::sync_time();
#endif
	}
}

void Team::match_team_select()
{
	vi_assert(Game::level.local && (match_state == MatchState::Waiting || match_state == MatchState::TeamSelect));
	if (match_state != MatchState::TeamSelect)
	{
		TeamNet::send_match_state(MatchState::TeamSelect);
#if SERVER
		Net::Server::sync_time();
#endif
	}
}

void Team::match_waiting()
{
	vi_assert(Game::level.local && (match_state == MatchState::Waiting || match_state == MatchState::TeamSelect));
	if (match_state != MatchState::Waiting)
	{
		TeamNet::send_match_state(MatchState::Waiting);
#if SERVER
		Net::Server::sync_time();
#endif
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
		case TeamNet::Message::MatchState:
		{
			serialize_enum(p, MatchState, match_state);
			if (match_state == MatchState::Done)
			{
				serialize_ref(p, winner);
				game_over_real_time = Game::real_time.total;

				score_summary.length = 0;
				for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
				{
					i.item()->score_accepted = false;
					AI::Team team = i.item()->team.ref()->team();
					team_add_score_summary_item(i.item(), i.item()->username);
					if (Game::session.type == SessionType::Story)
					{
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
			}
			match_time = 0.0f;
			break;
		}
		case TeamNet::Message::UpdateCounts:
		{
			Ref<Team> t;
			serialize_ref(p, t);
			s16 kills;
			serialize_s16(p, kills);
			s16 flags_captured;
			serialize_s16(p, flags_captured);
			if (!Game::level.local || src == Net::MessageSource::Loopback)
			{
				t.ref()->kills = kills;
				t.ref()->flags_captured = flags_captured;
			}
			break;
		}
		case TeamNet::Message::MapSchedule:
		{
			serialize_s16(p, Game::level.multiplayer_level_scheduled);
			break;
		}
		case TeamNet::Message::CoreVulnerable:
		{
			core_module_delay = 0.0f;
			Game::level.core_force_field.ref()->flags &= ~ForceField::FlagInvincible;

			// let everyone know what happened
			char buffer[UI_TEXT_MAX];
			strcpy(buffer, _(strings::core_vulnerable));

			for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
			{
				// it's a good thing if you're not on the defending team
				b8 good = i.item()->get<PlayerManager>()->team.ref()->team() != 0;
				i.item()->msg(buffer, good ? PlayerHuman::FlagMessageGood : PlayerHuman::FlagNone);
			}

			break;
		}
		case TeamNet::Message::Spot:
		{
			Ref<Team> team;
			serialize_ref(p, team);
			Ref<Target> target;
			serialize_ref(p, target);
			if (Game::level.local == (src == Net::MessageSource::Loopback))
				team.ref()->spot_target = target;
			break;
		}
		default:
			vi_assert(false);
			break;
	}

	return true;
}

void team_stats(s32* team_counts, s32 team_count, AI::Team* smallest_team, AI::Team* largest_team)
{
	s32 smallest_count = MAX_PLAYERS;
	s32 largest_count = 0;
	*smallest_team = 0;
	*largest_team = 0;
	for (s32 i = 0; i < team_count; i++)
	{
		if (team_counts[i] < smallest_count)
		{
			*smallest_team = i;
			smallest_count = team_counts[i];
		}
		if (team_counts[i] > largest_count)
		{
			*largest_team = i;
			largest_count = team_counts[i];
		}
	}
}

namespace PlayerManagerNet
{
	b8 team_switch(PlayerManager*, AI::Team);
	b8 can_spawn(PlayerManager*, b8);
}

enum class BalanceMode : s8
{
	All,
	OnlyBots,
	count,
};

void team_balance(BalanceMode mode)
{
	s32 team_counts[MAX_TEAMS] = {};
	for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
	{
		AI::Team team = i.item()->team_scheduled == AI::TeamNone ? i.item()->team.ref()->team() : i.item()->team_scheduled;
		team_counts[team]++;
	}

	AI::Team largest_team;
	AI::Team smallest_team;
	team_stats(team_counts, Team::list.count(), &smallest_team, &largest_team);
	while (team_counts[largest_team] > team_counts[smallest_team] + (mode == BalanceMode::OnlyBots ? 1 : 2))
	{
		// move a player from the largest team to the smallest
		PlayerManager* victim = nullptr;
		for (auto i = PlayerManager::list.iterator_end(); !i.is_first(); i.prev())
		{
			AI::Team team = i.item()->team_scheduled == AI::TeamNone ? i.item()->team.ref()->team() : i.item()->team_scheduled;
			if (team == largest_team)
			{
				if ((mode == BalanceMode::All && !victim) || !i.item()->has<PlayerHuman>()) // bots get moved first
					victim = i.item();
				break;
			}
		}
		if (victim)
		{
			team_counts[largest_team]--;
			team_counts[smallest_team]++;
			victim->team_scheduled = smallest_team;
			team_stats(team_counts, Team::list.count(), &smallest_team, &largest_team);
		}
		else if (mode == BalanceMode::All) // we should be able to balance teams if we're allowed to move humans
			vi_assert(false);
		else
			break; // impossible to balance by only moving bots
	}

	// apply team changes
	for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team_scheduled != AI::TeamNone)
			PlayerManagerNet::team_switch(i.item(), i.item()->team_scheduled);
		if (!i.item()->can_spawn)
			PlayerManagerNet::can_spawn(i.item(), true);
	}
}

void Team::update_all_server(const Update& u)
{
	if (match_state == MatchState::Waiting)
	{
		// check whether we need to transition to TeamSelect
		if (Game::session.type == SessionType::Story
			|| PlayerHuman::list.count() >= vi_max(s32(Game::session.config.min_players), Game::session.config.fill_bots ? 1 : 2))
			match_team_select();
	}
	else if (match_state == MatchState::TeamSelect)
	{
		// check whether we need to transition back to Waiting
		if (Game::session.type == SessionType::Multiplayer
			&& PlayerHuman::list.count() < vi_max(s32(Game::session.config.min_players), Game::session.config.fill_bots ? 1 : 2))
			match_waiting();
	}

	if (match_state == MatchState::TeamSelect
		&& Game::level.mode == Game::Mode::Pvp)
	{
		if ((match_time > TEAM_SELECT_TIME)
			|| !Game::level.has_feature(Game::FeatureLevel::All) // tutorial
			|| (Game::session.config.game_type == GameType::Deathmatch
				&& Game::session.config.max_players == Game::session.config.team_count // FFA
				&& teams_with_active_players() > 1))
		{
			// force match to start
			team_balance(BalanceMode::All);
			match_start();
		}
		else
		{
			// check if we can start
			b8 can_spawn = true;
			for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
			{
				if (!i.item()->can_spawn)
				{
					can_spawn = false;
					break;
				}
			}

			if (can_spawn)
			{
				team_balance(BalanceMode::OnlyBots); // move bots around (not humans) to try and balance teams

				if (teams_with_active_players() > 1)
				{
					s32 least_players = MAX_PLAYERS;
					s32 most_players = 0;
					for (auto i = list.iterator(); !i.is_last(); i.next())
					{
						s32 players = i.item()->player_count();
						least_players = vi_min(least_players, players);
						most_players = vi_max(most_players, players);
					}
					if (most_players <= least_players + 2)
						match_start();
				}
			}
		}
	}

	if (Game::level.mode != Game::Mode::Pvp)
		return;

	// fill bots
	if (Game::session.config.fill_bots
		&& (match_state == MatchState::Waiting || match_state == MatchState::TeamSelect || match_state == MatchState::Active)
		&& PlayerHuman::list.count() > 0)
	{
		while (PlayerManager::list.count() < vi_min(s32(Game::session.config.max_players), Game::session.config.fill_bots + 1))
		{
			Entity* e = World::create<ContainerEntity>();
			char username[MAX_USERNAME + 1] = {};
			snprintf(username, MAX_USERNAME, "Bot %03d", mersenne::rand() % 1000);
			Team* team = with_least_players();
			vi_assert(team);
			AI::Config config = PlayerAI::generate_config(team->team(), 0.0f);
			PlayerManager* manager = e->add<PlayerManager>(team, username);
			PlayerAI* player = PlayerAI::list.add();
			new (player) PlayerAI(manager, config);
			Net::finalize(e);
		}
	}

	if (match_state == MatchState::Active)
	{
		Team* team_with_most_kills = Game::session.config.game_type == GameType::Deathmatch ? with_most_kills() : nullptr;
		Team* team_with_most_flags = Game::session.config.game_type == GameType::CaptureTheFlag ? with_most_flags() : nullptr;
		if (!Game::level.noclip
			&& ((match_time > Game::session.config.time_limit() && Game::level.has_feature(Game::FeatureLevel::All)) // no time limit in tutorial
			|| (Game::level.has_feature(Game::FeatureLevel::All) && teams_with_active_players() <= 1 && Game::level.ai_config.length == 0)
			|| (Game::session.config.game_type == GameType::Assault && CoreModule::count(1 << 0) == 0)
			|| (Game::session.config.game_type == GameType::Deathmatch && team_with_most_kills && team_with_most_kills->kills >= Game::session.config.kill_limit)
			|| (Game::session.config.game_type == GameType::CaptureTheFlag && team_with_most_flags && team_with_most_flags->flags_captured >= Game::session.config.flag_limit)))
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
			else if (Game::session.config.game_type == GameType::CaptureTheFlag)
				w = team_with_most_flags;

			// remove player entities
			for (auto i = PlayerCommon::list.iterator(); !i.is_last(); i.next())
			{
				Vec3 pos;
				Quat rot;
				i.item()->get<Transform>()->absolute(&pos, &rot);
				ParticleEffect::spawn(ParticleEffect::Type::DroneExplosion, pos, rot);
				World::remove_deferred(i.item()->entity());
			}

			TeamNet::send_match_state(MatchState::Done, w);

			if (Game::session.type == SessionType::Story)
			{
				// we're in story mode, give the player whatever stuff they have leftover
				if (PlayerHuman::list.count() > 0)
				{
					PlayerManager* player = PlayerHuman::list.iterator().item()->get<PlayerManager>();
					Overworld::resource_change(Resource::Energy, player->energy);
				}

				if (w == &list[1]) // attackers won; the zone is going to change owners
				{
					if (Game::save.zones[Game::level.id] == ZoneState::PvpFriendly) // player was defending
						Overworld::zone_change(Game::level.id, ZoneState::PvpHostile);
					else // player was attacking
						Overworld::zone_change(Game::level.id, ZoneState::PvpFriendly);
				}
			}
		}
		else
		{
			// game is still going

			if (Game::session.config.game_type == GameType::Assault && Turret::list.count() == 0)
			{
				if (core_module_delay > 0.0f)
				{
					core_module_delay = vi_max(0.0f, core_module_delay - u.time.delta);
					if (core_module_delay == 0.0f)
						TeamNet::core_vulnerable();
				}
			}

			for (auto i = list.iterator(); !i.is_last(); i.next())
			{
				Target* spot = i.item()->spot_target.ref();
				if (spot && spot->has<Flag>())
				{
					// check if flag is being carried by an enemy; if so, remove the spot
					// we don't want people to be able to track their flag's location
					Transform* parent = spot->get<Transform>()->parent.ref();
					if (parent && parent->get<AIAgent>()->team != spot->get<Flag>()->team)
						TeamNet::spot(i.item(), nullptr);
				}
			}
		}
	}

	if (match_state == MatchState::Done && Game::scheduled_load_level == AssetNull)
	{
		// wait for all local players to accept scores
		b8 score_accepted = true;
		if (Game::real_time.total - game_over_real_time < SCORE_SUMMARY_ACCEPT_TIME) // automatically move on after 45 seconds
		{
			for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
			{
				if (!i.item()->get<PlayerManager>()->score_accepted)
				{
					score_accepted = false;
					break;
				}
			}
		}

		if (score_accepted)
			transition_next(); // time to get out of here
	}

	update_visibility(u);
}

void Team::draw_ui(const RenderParams& params)
{
	if (transition_timer > 0.0f)
		Menu::draw_letterbox(params, transition_timer, TRANSITION_TIME);
}

void Team::update_all(const Update& u)
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

s16 Team::initial_energy() const
{
	return Game::session.config.ruleset.start_energy;
}

SpawnPoint* Team::default_spawn_point() const
{
	for (auto i = SpawnPoint::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team == team() && !i.item()->battery())
			return i.item();
	}
	return nullptr;
}

PlayerManager::Visibility PlayerManager::visibility[MAX_PLAYERS * MAX_PLAYERS];

PlayerManager::PlayerManager(Team* team, const char* u)
	: spawn_timer((Game::session.type == SessionType::Story && team->team() == 1) ? 0 : Game::session.config.ruleset.spawn_delay), // defenders in story mode get to spawn instantly
	score_accepted(Team::match_state == Team::MatchState::Done),
	team(team),
	upgrades(Game::session.config.ruleset.upgrades_default),
	abilities{ Ability::None, Ability::None },
	instance(),
	spawn(),
	can_spawn(Game::session.type == SessionType::Story || Team::match_state == Team::MatchState::Active),
	current_upgrade(Upgrade::None),
	state_timer(),
	upgrade_completed(),
	energy(team->initial_energy()),
	kills(),
	deaths(),
	flags_captured(),
	ability_cooldown(),
	ability_flash_time(),
	current_upgrade_ability_slot(),
	team_scheduled(AI::TeamNone),
	is_admin()
{
	{
		const StaticArray<Ability, MAX_ABILITIES>& start_abilities = Game::session.config.ruleset.start_abilities;
		for (s32 i = 0; i < start_abilities.length; i++)
		{
			Ability ability = start_abilities[i];
			vi_assert(UpgradeInfo::list[s32(ability)].type == UpgradeInfo::Type::Ability);
			upgrades |= 1 << s32(ability);
			abilities[i] = ability;
		}
	}

	if (Game::level.has_feature(Game::FeatureLevel::Abilities)
		&& (Game::session.config.ruleset.upgrades_allow | Game::session.config.ruleset.upgrades_default)
		&& Game::session.type == SessionType::Story
		&& Game::session.config.game_type == GameType::Assault
		&& team->team() == 0)
		energy += s32(Team::match_time / ENERGY_INCREMENT_INTERVAL) * (ENERGY_DEFAULT_INCREMENT * s32(Battery::list.count() * 0.75f));

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

b8 PlayerManager::has_ability(Ability a) const
{
	for (s32 i = 0; i < MAX_ABILITIES; i++)
	{
		if (abilities[i] == a)
			return true;
	}
	return false;
}

b8 PlayerManager::ability_valid(Ability ability) const
{
	if (ability != Ability::None)
	{
		if (!Game::level.has_feature(Game::FeatureLevel::Abilities))
			return false;

		if (!has_ability(ability))
			return false;
	}

	if (state() != State::Default)
		return false;

	Entity* e = instance.ref();
	if (!e)
		return false;

	if (ability == Ability::ActiveArmor && e->get<Health>()->active_armor())
		return false;

	Drone* drone = e->get<Drone>();
	if (drone->state() != Drone::State::Crawl)
		return false;

	if (!drone->cooldown_can_shoot())
		return false;

	if (ability != Ability::None && drone->flag.ref())
		return false;

	{
		const AbilityInfo& info = AbilityInfo::list[s32(ability)];
		if (ability_cooldown[s32(ability)] > 0.0f)
			return false;
	}

	return true;
}

s32 PlayerManager::visibility_hash(const PlayerManager* drone_a, const PlayerManager* drone_b)
{
	return drone_a->id() * MAX_PLAYERS + drone_b->id();
}

namespace PlayerManagerNet
{
	b8 send(PlayerManager* m, PlayerManager::Message msg)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::PlayerManager);
		{
			Ref<PlayerManager> ref = m;
			serialize_ref(p, ref);
		}
		serialize_enum(p, PlayerManager::Message, msg);
		Net::msg_finalize(p);
		return true;
	}

	b8 can_spawn(PlayerManager* m, b8 value)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::PlayerManager);
		{
			Ref<PlayerManager> ref = m;
			serialize_ref(p, ref);
		}
		{
			PlayerManager::Message msg = PlayerManager::Message::CanSpawn;
			serialize_enum(p, PlayerManager::Message, msg);
		}
		serialize_bool(p, value);
		Net::msg_finalize(p);
		return true;
	}

	b8 team_schedule(PlayerManager* m, AI::Team t)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::PlayerManager);
		{
			Ref<PlayerManager> ref = m;
			serialize_ref(p, ref);
		}
		{
			PlayerManager::Message msg = PlayerManager::Message::TeamSchedule;
			serialize_enum(p, PlayerManager::Message, msg);
		}
		serialize_s8(p, t);
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
			PlayerManager::Message msg = PlayerManager::Message::UpdateCounts;
			serialize_enum(p, PlayerManager::Message, msg);
		}
		serialize_s16(p, m->kills);
		serialize_s16(p, m->deaths);
		serialize_s16(p, m->flags_captured);
		Net::msg_finalize(p);
		return true;
	}

	b8 team_switch(PlayerManager* m, AI::Team t)
	{
		vi_assert(Game::level.local);

		if (Game::session.config.game_type == GameType::Assault && t != m->team.ref()->team())
		{
			const Team& team = Team::list[s32(t)];
			m->energy = team.initial_energy();
		}

		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::PlayerManager);
		{
			Ref<PlayerManager> ref = m;
			serialize_ref(p, ref);
		}
		{
			PlayerManager::Message msg = PlayerManager::Message::TeamSwitch;
			serialize_enum(p, PlayerManager::Message, msg);
		}
		serialize_s8(p, t);

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
			PlayerManager::Message msg = PlayerManager::Message::UpgradeCompleted;
			serialize_enum(p, PlayerManager::Message, msg);
		}
		serialize_int(p, s32, index, 0, MAX_ABILITIES - 1);
		serialize_enum(p, Upgrade, u);
		Net::msg_finalize(p);
		return true;
	}

	b8 make_other_admin(PlayerManager* existing_admin, PlayerManager* m, b8 value)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::PlayerManager);
		{
			Ref<PlayerManager> ref = existing_admin;
			serialize_ref(p, ref);
		}
		{
			PlayerManager::Message msg = PlayerManager::Message::MakeOtherAdmin;
			serialize_enum(p, PlayerManager::Message, msg);
		}
		{
			Ref<PlayerManager> ref = m;
			serialize_ref(p, ref);
		}
		serialize_bool(p, value);
		Net::msg_finalize(p);
		return true;
	}

	b8 ban(PlayerManager* existing_admin, PlayerManager* m)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::PlayerManager);
		{
			Ref<PlayerManager> ref = existing_admin;
			serialize_ref(p, ref);
		}
		{
			PlayerManager::Message msg = PlayerManager::Message::Ban;
			serialize_enum(p, PlayerManager::Message, msg);
		}
		{
			Ref<PlayerManager> ref = m;
			serialize_ref(p, ref);
		}
		Net::msg_finalize(p);
		return true;
	}

	b8 make_admin(PlayerManager* m, b8 value)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::PlayerManager);
		{
			Ref<PlayerManager> ref = m;
			serialize_ref(p, ref);
		}
		{
			PlayerManager::Message msg = PlayerManager::Message::MakeAdmin;
			serialize_enum(p, PlayerManager::Message, msg);
		}
		serialize_bool(p, value);
		Net::msg_finalize(p);
		return true;
	}

	b8 kick(PlayerManager* kicker, PlayerManager* kickee)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::PlayerManager);
		{
			Ref<PlayerManager> ref = kicker;
			serialize_ref(p, ref);
		}
		{
			PlayerManager::Message msg = PlayerManager::Message::Kick;
			serialize_enum(p, PlayerManager::Message, msg);
		}
		{
			Ref<PlayerManager> ref = kickee;
			serialize_ref(p, ref);
		}
		Net::msg_finalize(p);
		return true;
	}

	b8 leave(PlayerManager* m)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::PlayerManager);
		{
			Ref<PlayerManager> ref = m;
			serialize_ref(p, ref);
		}
		{
			PlayerManager::Message msg = PlayerManager::Message::Leave;
			serialize_enum(p, PlayerManager::Message, msg);
		}
		Net::msg_finalize(p);
		return true;
	}

	b8 map_schedule(PlayerManager* m, AssetID map)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::PlayerManager);
		{
			Ref<PlayerManager> ref = m;
			serialize_ref(p, ref);
		}
		{
			PlayerManager::Message msg = PlayerManager::Message::MapSchedule;
			serialize_enum(p, PlayerManager::Message, msg);
		}
		serialize_s16(p, map);
		Net::msg_finalize(p);
		return true;
	}

	b8 map_skip(PlayerManager* m, AssetID map)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::PlayerManager);
		{
			Ref<PlayerManager> ref = m;
			serialize_ref(p, ref);
		}
		{
			PlayerManager::Message msg = PlayerManager::Message::MapSkip;
			serialize_enum(p, PlayerManager::Message, msg);
		}
		serialize_s16(p, map);
		Net::msg_finalize(p);
		return true;
	}

	b8 chat(PlayerManager* m, const char* text, AI::TeamMask mask)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::PlayerManager);
		{
			Ref<PlayerManager> ref = m;
			serialize_ref(p, ref);
		}
		{
			PlayerManager::Message msg = PlayerManager::Message::Chat;
			serialize_enum(p, PlayerManager::Message, msg);
		}

		serialize_s8(p, mask);

		{
			s32 text_length = s32(strlen(text));
			serialize_int(p, s32, text_length, 1, MAX_CHAT);
			char* hack = const_cast<char*>(text);
			serialize_bytes(p, (u8*)hack, text_length);
		}

		Net::msg_finalize(p);
		return true;
	}

	b8 spot(PlayerManager* m, Target* t)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::PlayerManager);
		{
			Ref<PlayerManager> ref = m;
			serialize_ref(p, ref);
		}
		{
			PlayerManager::Message msg = PlayerManager::Message::Spot;
			serialize_enum(p, PlayerManager::Message, msg);
		}
		{
			Ref<Target> ref = t;
			serialize_ref(p, ref);
		}
		Net::msg_finalize(p);
		return true;
	}

	b8 ability_cooldown_ready(PlayerManager* m, Ability a)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::PlayerManager);
		{
			Ref<PlayerManager> ref = m;
			serialize_ref(p, ref);
		}
		{
			PlayerManager::Message msg = PlayerManager::Message::AbilityCooldownReady;
			serialize_enum(p, PlayerManager::Message, msg);
		}
		serialize_enum(p, Ability, a);
		Net::msg_finalize(p);
		return true;
	}
}

void internal_spawn_go(PlayerManager* m, SpawnPoint* point)
{
	vi_assert(Game::level.local && point);
	m->spawn_timer = Game::session.config.ruleset.spawn_delay;
	m->spawn.fire(point->spawn_position());
}

void remove_player_from_local_mask(PlayerManager* m)
{
	if (m->has<PlayerHuman>() && m->get<PlayerHuman>()->local())
		Game::session.local_player_mask &= ~(1 << m->get<PlayerHuman>()->gamepad);
}

b8 PlayerManager::net_msg(Net::StreamRead* p, PlayerManager* m, Message msg, Net::MessageSource src)
{
	using Stream = Net::StreamRead;

	if (src == Net::MessageSource::Invalid)
		net_error();

	switch (msg)
	{
		case Message::CanSpawn:
		{
			b8 value;
			serialize_bool(p, value);

			if (!m)
				return true;

			if (!Game::level.local || src == Net::MessageSource::Loopback)
				m->can_spawn = value;

			if (Game::level.local
				&& src == Net::MessageSource::Remote
				&& (value || Team::match_state == Team::MatchState::Waiting || Team::match_state == Team::MatchState::TeamSelect)) // once the match starts, can_spawn can not go false
			{
				if (value && m->team_scheduled != AI::TeamNone)
				{
					b8 valid;
					if (Team::match_state == Team::MatchState::Waiting || Team::match_state == Team::MatchState::TeamSelect)
						valid = true;
					else if (m->team_scheduled == m->team.ref()->team())
						valid = true;
					else
					{
						// make sure teams will still be even
						s32 least_players;
						Team::with_least_players(&least_players);
						if (least_players == m->team.ref()->player_count())
							least_players--; // this team would also be losing a player
						valid = Team::list[m->team_scheduled].player_count() + 1 <= least_players + 2;
					}

					if (valid) // actually switch teams
					{
						PlayerManagerNet::team_switch(m, m->team_scheduled);
						if (!m->can_spawn)
							PlayerManagerNet::can_spawn(m, value); // repeat to all clients
						if (Team::match_state == Team::MatchState::Active)
							team_balance(BalanceMode::OnlyBots);
					}
					else
						PlayerManagerNet::team_switch(m, m->team.ref()->team()); // keep their same team (clears team_scheduled)
				}
				else
					PlayerManagerNet::can_spawn(m, value); // repeat to all clients
			}
			break;
		}
		case Message::TeamSchedule:
		{
			AI::Team t;
			serialize_s8(p, t);

			if (!m)
				return true;

			if (Game::session.type == SessionType::Multiplayer
				&& Team::match_state != Team::MatchState::Done
				&& (Game::level.local || src == Net::MessageSource::Remote)
				&& (t == AI::TeamNone || (t >= 0 && t < Team::list.count())))
			{
				if (Game::level.local && src == Net::MessageSource::Remote) // repeat it to all clients
					PlayerManagerNet::team_schedule(m, t);
				else
					m->team_scheduled = t;
			}
			break;
		}
		case Message::TeamSwitch:
		{
			AI::Team t;
			serialize_s8(p, t);

			if (!m)
				return true;

			if (!Game::level.local || src == Net::MessageSource::Loopback)
			{
				if (Game::level.local
					&& m->team.ref()->team() != t
					&& m->instance.ref())
					m->instance.ref()->get<Health>()->kill(nullptr);
				m->team = &Team::list[t];
				m->team_scheduled = AI::TeamNone;
				if (m->has<PlayerHuman>())
					m->get<PlayerHuman>()->team_set(t);
				m->clear_ownership();
			}
			break;
		}
		case Message::ScoreAccept:
		{
			if (!m)
				return true;
			m->score_accepted = true;
			break;
		}
		case Message::UpgradeCompleted:
		{
			s32 index;
			serialize_int(p, s32, index, 0, MAX_ABILITIES - 1);
			Upgrade u;
			serialize_enum(p, Upgrade, u);

			if (!m)
				return true;

			if (!Game::level.local || src == Net::MessageSource::Loopback)
			{
				m->current_upgrade = Upgrade::None;
				m->state_timer = 0.0f;
				if (UpgradeInfo::list[s32(u)].type == UpgradeInfo::Type::Ability)
				{
					m->upgrades |= 1 << s32(u);
					if (m->abilities[index] != Ability::None)
					{
						// if the drone currently has the old ability equipped, update it to the new ability
						Entity* instance = m->instance.ref();
						if (instance && instance->get<Drone>()->current_ability == m->abilities[index])
							instance->get<Drone>()->current_ability = Ability(u);
					}
					m->abilities[index] = Ability(u);
					m->ability_flash_time[index] = Game::real_time.total;
				}
				else
					vi_assert(false);
				m->upgrade_completed.fire(u);
			}
			break;
		}
		case Message::UpdateCounts:
		{
			s16 kills;
			s16 deaths;
			s16 flags_captured;
			serialize_s16(p, kills);
			serialize_s16(p, deaths);
			serialize_s16(p, flags_captured);

			if (!m)
				return true;
		
			if (!Game::level.local || src == Net::MessageSource::Loopback) // server does not accept these messages from clients
			{
				m->kills = kills;
				m->deaths = deaths;
				m->flags_captured = flags_captured;
			}
			break;
		}
		case Message::MakeOtherAdmin:
		{
			Ref<PlayerManager> target;
			serialize_ref(p, target);
			b8 value;
			serialize_bool(p, value);

			if (!m || !target.ref() || !m->is_admin || !target.ref()->has<PlayerHuman>())
				return true;

			if (Game::level.local)
				target.ref()->make_admin(value);
			break;
		}
		case Message::MakeAdmin:
		{
			b8 value;
			serialize_bool(p, value);

			if (!m || !m->has<PlayerHuman>())
				return true;

			if (!Game::level.local
				|| src == Net::MessageSource::Loopback)
			{
				m->is_admin = value;
#if SERVER
				Net::Server::admin_set(m->get<PlayerHuman>(), value);
				Net::master_user_role_set(Game::session.config.id, m->get<PlayerHuman>()->master_id, value ? Net::Master::Role::Admin : Net::Master::Role::Allowed);
#endif
			}
			break;
		}
		case Message::Ban:
		{
			Ref<PlayerManager> target;
			serialize_ref(p, target);

			if (!m || !target.ref() || !m->is_admin || !target.ref()->has<PlayerHuman>())
				return true;

			if (Game::level.local)
			{
#if SERVER
				Net::master_user_role_set(Game::session.config.id, target.ref()->get<PlayerHuman>()->master_id, Net::Master::Role::Banned);
#endif
				PlayerManagerNet::kick(m, target.ref()); // repeat for other clients and ourselves
			}
			break;
		}
		case Message::Kick:
		{
			Ref<PlayerManager> kickee;
			serialize_ref(p, kickee);

			if (!m)
				return true;

			if (!m->is_admin || m == kickee.ref())
				net_error();

			if (Game::level.local)
			{
				if (src == Net::MessageSource::Remote)
				{
					if (kickee.ref() && kickee.ref()->has<PlayerHuman>())
						PlayerManagerNet::kick(m, kickee.ref()); // repeat for other clients and ourselves
				}
				else // loopback
					kickee.ref()->kick();
			}

			if (Game::level.local == (src == Net::MessageSource::Loopback))
			{
				// display notification
				char buffer[UI_TEXT_MAX];
				snprintf(buffer, UI_TEXT_MAX, _(strings::player_kicked), kickee.ref()->username);
				PlayerHuman::log_add(buffer, kickee.ref()->team.ref()->team(), AI::TeamAll);
				remove_player_from_local_mask(m);
			}
			break;
		}
		case Message::Leave:
		{
			if (!m)
				return true;

			remove_player_from_local_mask(m);

			if (Game::level.local)
				m->kick();
				
			break;
		}
		case Message::MapSchedule:
		{
			AssetID map;
			serialize_s16(p, map);

			if (!m)
				return true;

			if (!m->is_admin || (map != AssetNull && Overworld::zone_max_teams(map) < Game::session.config.team_count))
				net_error();

			if (Game::level.local)
				TeamNet::map_schedule(map);

			break;
		}
		case Message::MapSkip:
		{
			AssetID map;
			serialize_s16(p, map);

			if (!m)
				return true;

			if (!m->is_admin || Overworld::zone_max_teams(map) < Game::session.config.team_count)
				net_error();

			if (Game::level.local)
			{
				Game::level.multiplayer_level_scheduled = map;
				Team::transition_next();
			}

			break;
		}
		case Message::Chat:
		{
			AI::TeamMask mask;
			serialize_s8(p, mask);

			s32 text_length;
			serialize_int(p, s32, text_length, 1, MAX_CHAT);
			char text[MAX_CHAT + 1];
			serialize_bytes(p, (u8*)text, text_length);
			text[text_length] = '\0';

			if (!m)
				return true;

			if (Game::level.local)
			{
				if (src == Net::MessageSource::Remote)
					m->chat(text, mask); // repeat to everyone
				else // loopback message
					PlayerHuman::chat_add(text, m, mask);
			}
			else if (src == Net::MessageSource::Remote)
				PlayerHuman::chat_add(text, m, mask);

			break;
		}
		case Message::Spot:
		{
			Ref<Target> target;
			serialize_ref(p, target);

			if (!m)
				return true;

			m->team.ref()->spot_target = target;
			break;
		}
		case Message::AbilityCooldownReady:
		{
			Ability a;
			serialize_enum(p, Ability, a);

			if (!m)
				return true;

			if (m->is_local())
				Audio::post_global(AK::EVENTS::PLAY_DRONE_CHARGE_RESTORE);

			m->ability_cooldown[s32(a)] = 0.0f;

			// flash ability
			for (s32 i = 0; i < MAX_ABILITIES; i++)
			{
				if (m->abilities[i] == a)
				{
					m->ability_flash_time[i] = Game::real_time.total;
					break;
				}
			}

			break;
		}
		default:
			vi_assert(false);
			break;
	}
	return true;
}

void PlayerManager::chat(const char* msg, AI::TeamMask mask)
{
	if (strlen(msg) > 0)
		PlayerManagerNet::chat(this, msg, mask);
}

void PlayerManager::spot(Target* t)
{
	PlayerManagerNet::spot(this, t);
}

void PlayerManager::leave()
{
	if (has<PlayerHuman>() && get<PlayerHuman>()->local() && (PlayerHuman::count_local() == 1 || get<PlayerHuman>()->gamepad == 0))
	{
		// we're the only player left, or we're player 1; just exit
		if (Game::session.type == SessionType::Story)
			Menu::title();
		else
			Menu::title_multiplayer();
	}
	else // other people still playing
		PlayerManagerNet::leave(this);
}

b8 PlayerManager::upgrade_start(Upgrade u, s8 ability_slot)
{
	const UpgradeInfo& info = UpgradeInfo::list[s32(u)];
	s16 cost = upgrade_cost(u);
	if (can_transition_state()
		&& upgrade_available(u)
		&& UpgradeStation::drone_inside(instance.ref()->get<Drone>()))
	{
		if (Game::level.local)
		{
			current_upgrade = u;
			current_upgrade_ability_slot = ability_slot;

			r32 rtt;
			r32 interpolation_delay;
			if (has<PlayerHuman>() && !get<PlayerHuman>()->local())
			{
				rtt = Net::rtt(get<PlayerHuman>());
				interpolation_delay = Net::interpolation_delay(get<PlayerHuman>());
			}
			else
			{
				rtt = 0.0f;
				interpolation_delay = 0.0f;
			}
			state_timer = UPGRADE_TIME - vi_min(NET_MAX_RTT_COMPENSATION, rtt) - interpolation_delay;

			if (!has_upgrade(u)) // doesn't cost anything the second time
				add_energy(-cost);
		}
		else // client-side prediction
		{
			state_timer = UPGRADE_TIME;
			current_upgrade = u;
			current_upgrade_ability_slot = ability_slot;
		}
		return true;
	}
	else
		return false;
}

void PlayerManager::upgrade_complete()
{
	PlayerManagerNet::upgrade_completed(this, current_upgrade_ability_slot, current_upgrade);
}

s16 PlayerManager::upgrade_cost(Upgrade u) const
{
	vi_assert(u != Upgrade::None);
	return UpgradeInfo::list[s32(u)].cost;
}

Upgrade PlayerManager::upgrade_highest_owned_or_available() const
{
	s16 highest_cost = 0;
	Upgrade highest_upgrade = Upgrade::None;
	for (s32 i = 0; i < s32(Upgrade::count); i++)
	{
		s16 cost = upgrade_cost(Upgrade(i));
		if (cost > highest_cost && !has_upgrade(Upgrade(i)) && upgrade_available(Upgrade(i)))
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
		// any upgrade available?
		for (s32 i = 0; i < s32(Upgrade::count); i++)
		{
			if (!has_upgrade(Upgrade(i))
				&& energy >= upgrade_cost(Upgrade(i))
				&& upgrade_available(Upgrade(i)))
				return true;
		}
		return false;
	}
	else
	{
		// make sure that the upgrade is allowed and we have enough money for it
		const UpgradeInfo& info = UpgradeInfo::list[s32(u)];
		return (info.type != UpgradeInfo::Type::Ability || !has_ability(Ability(u))) // can't do an ability upgrade if we already have the ability equipped
			&& ((Game::session.config.ruleset.upgrades_allow | Game::session.config.ruleset.upgrades_default) & (1 << s32(u)))
			&& (has_upgrade(u) || energy >= upgrade_cost(u));
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

void PlayerManager::captured_flag()
{
	vi_assert(Game::level.local);
	flags_captured++;
	PlayerManagerNet::update_counts(this);
	team.ref()->flags_captured++;
	TeamNet::update_counts(team.ref());
}

void PlayerManager::add_deaths(s32 d)
{
	vi_assert(Game::level.local);
	if (Game::level.has_feature(Game::FeatureLevel::All)) // don't count deaths in tutorial
	{
		deaths += d;
		PlayerManagerNet::update_counts(this);
	}
}

void PlayerManager::ability_cooldown_apply(Ability a)
{
	const AbilityInfo& info = AbilityInfo::list[s32(a)];
	if (info.cooldown_use > 0.0f)
	{
		vi_assert(ability_cooldown[s32(a)] == 0.0f);
		r32 c;
#if SERVER
		if (has<PlayerHuman>())
			c = info.cooldown_use - Net::rtt(get<PlayerHuman>());
		else
#endif
			c = info.cooldown_use;

		ability_cooldown[s32(a)] = c;
	}
}

void PlayerManager::make_admin(b8 value)
{
	vi_assert(Game::level.local);
	PlayerManagerNet::make_admin(this, value);
}

void PlayerManager::make_admin(PlayerManager* other, b8 value)
{
	vi_assert(is_admin);
	PlayerManagerNet::make_other_admin(this, other, value);
}

void PlayerManager::kick(PlayerManager* kickee)
{
	vi_assert(is_admin && kickee != this);
	PlayerManagerNet::kick(this, kickee);
}

void PlayerManager::kick()
{
	vi_assert(Game::level.local);

	{
		Entity* i = instance.ref();
		if (i)
			World::remove_deferred(i);
	}
	World::remove_deferred(entity());

#if SERVER
	if (has<PlayerHuman>())
	{
		ID client_id = Net::Server::client_id(get<PlayerHuman>());
		b8 client_has_other_players = false;
		for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item() != get<PlayerHuman>() && Net::Server::client_id(i.item()) == client_id)
			{
				client_has_other_players = true;
				break;
			}
		}

		if (!client_has_other_players)
			Net::Server::client_force_disconnect(client_id, Net::DisconnectReason::Kicked);
	}
#endif
}

void PlayerManager::ban(PlayerManager* other)
{
	vi_assert(is_admin);
	PlayerManagerNet::ban(this, other);
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
	if (!Game::level.has_feature(Game::FeatureLevel::Abilities) || !(Game::session.config.ruleset.upgrades_allow | Game::session.config.ruleset.upgrades_default))
		return false;

	Entity* e = instance.ref();
	if (!e)
		return false;

	if (state() != State::Default)
		return false;

	Drone* drone = e->get<Drone>();
	return drone->state() == Drone::State::Crawl && !drone->flag.ref();
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
					// give points to players based on how many batteries they own
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
		else
			i.item()->update_client_only(u);
	}
}

PlayerManager* PlayerManager::owner(const Entity* e)
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

void PlayerManager::clear_ownership()
{
	for (auto i = Minion::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->owner.ref() == this)
			i.item()->owner = nullptr;
	}

	for (auto i = Bolt::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->player.ref() == this)
			i.item()->player = nullptr;
	}

	for (auto i = Grenade::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->owner.ref() == this)
			i.item()->set_owner(nullptr);
	}
}

void killer_name(Entity* killer, PlayerManager* killer_player, PlayerManager* player, char* result)
{
	result[0] = '\0';

	Entity* logged_killer = killer;
	if (killer->has<Bolt>())
		logged_killer = killer->get<Bolt>()->owner.ref();

	if (logged_killer)
	{
		if (logged_killer->has<Minion>())
			strncpy(result, _(strings::minion), MAX_USERNAME);
		else if (logged_killer->has<Turret>())
			snprintf(result, MAX_USERNAME + 1, _(strings::turret_name), _(logged_killer->get<Turret>()->name()));
	}

	if (result[0] == '\0')
	{
		if (killer_player)
			strncpy(result, killer_player->username, MAX_USERNAME);
		else
			strncpy(result, _(strings::minion), MAX_USERNAME);
	}
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
				if (Game::level.local)
				{
					if (killer_player)
						killer_player->add_kills(1);
					if (killer_team != AI::TeamNone)
						Team::list[killer_team].add_kills(1);
					player->add_deaths(1);
				}

				{
					// log message
					char killer_str[MAX_USERNAME + 1];
					killer_name(killer, killer_player, player, killer_str);

					PlayerHuman::log_add(killer_str, killer_team, AI::TeamAll, player->username, team);
				}

				if (player && killer_player && killer_player->has<PlayerHuman>())
					killer_player->get<PlayerHuman>()->kill_popup(player);

				reward = ENERGY_DRONE_DESTROY;
			}
			else if (e->has<Minion>())
				reward = ENERGY_MINION_KILL;
			else if (e->has<Grenade>())
				reward = ENERGY_GRENADE_DESTROY;
			else if (e->has<ForceField>())
				reward = ENERGY_FORCE_FIELD_DESTROY;
			else if (e->has<Rectifier>())
				reward = ENERGY_RECTIFIER_DESTROY;
			else if (e->has<Turret>())
			{
				reward = ENERGY_TURRET_DESTROY;
				reward_share = true;

				// log message
				char killer_str[MAX_USERNAME + 1];
				killer_name(killer, killer_player, player, killer_str);

				char name[MAX_USERNAME + 1];
				snprintf(name, MAX_USERNAME, _(strings::turret_name), _(e->get<Turret>()->name()));

				PlayerHuman::log_add(killer_str, killer_team, AI::TeamAll, name, team);
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
		&& Team::match_state == Team::MatchState::Active
		&& !Game::level.noclip)
	{
		if (Game::level.mode == Game::Mode::Pvp)
		{
			if (spawn_timer > 0.0f)
			{
				spawn_timer = vi_max(0.0f, spawn_timer - u.time.delta);
				if (spawn_timer == 0.0f)
					internal_spawn_go(this, team.ref()->default_spawn_point());
			}
		}
		else if (Game::level.mode == Game::Mode::Parkour)
			internal_spawn_go(this, team.ref()->default_spawn_point());
	}

	for (s32 i = 0; i < s32(Ability::count); i++)
	{
		if (ability_cooldown[i] > 0.0f)
		{
			ability_cooldown[i] = vi_max(0.0f, ability_cooldown[i] - u.time.delta);
			if (ability_cooldown[i] == 0.0f) // let clients know
				PlayerManagerNet::ability_cooldown_ready(this, Ability(i));
		}
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
					upgrade_complete();
					break;
				default:
					vi_assert(false);
					break;
			}
		}
	}
}

void PlayerManager::update_client_only(const Update& u)
{
	state_timer = vi_max(0.0f, state_timer - u.time.delta);
	for (s32 i = 0; i < s32(Ability::count); i++)
	{
		if (ability_cooldown[i] > 0.0f)
			ability_cooldown[i] = vi_max(0.01f, ability_cooldown[i] - u.time.delta); // can't set to zero until the server says so
	}
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

void PlayerManager::set_can_spawn(b8 value)
{
	vi_assert(value || Team::match_state == Team::MatchState::Waiting || Team::match_state == Team::MatchState::TeamSelect); // once match starts, can_spawn can't go false
	PlayerManagerNet::can_spawn(this, value);
}

void PlayerManager::team_schedule(AI::Team t)
{
	PlayerManagerNet::team_schedule(this, t);
}

void PlayerManager::map_schedule(AssetID map)
{
	PlayerManagerNet::map_schedule(this, map);
}

void PlayerManager::map_skip(AssetID map)
{
	PlayerManagerNet::map_skip(this, map);
}

void PlayerManager::score_accept()
{
	PlayerManagerNet::send(this, PlayerManager::Message::ScoreAccept);
}


}
