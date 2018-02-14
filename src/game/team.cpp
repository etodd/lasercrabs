#include "strings.h"
#include "team.h"
#include "game.h"
#include "data/components.h"
#include "entities.h"
#include "data/animator.h"
#include "asset/animation.h"
#include "asset/mesh.h"
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

r32 Team::battery_spawn_delay;
r32 Team::game_over_real_time;
Ref<Team> Team::winner;
StaticArray<Team::ScoreSummaryItem, MAX_PLAYERS * PLAYER_SCORE_SUMMARY_ITEMS> Team::score_summary;
r32 Team::transition_timer;
r32 Team::match_time;
Team::MatchState Team::match_state;

#define PARKOUR_GAME_START_COUNTDOWN 10

b8 Team::parkour_game_start_impending()
{
	return Game::session.type == SessionType::Multiplayer
		&& Game::level.mode == Game::Mode::Parkour
		&& match_time > (60.0f * r32(Game::session.config.time_limit_parkour_ready)) - r32(PARKOUR_GAME_START_COUNTDOWN);
}

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
		0.265f, // movement cooldown
		0.3f, // switch cooldown
		0.0f, // use cooldown
		0.0f, // use cooldown threshold
		0.7f, // recoil velocity
		AK::EVENTS::PLAY_EQUIP_BOLTER,
		Asset::Mesh::icon_bolter,
		Type::Shoot,
	},
	{
		2.75f, // cooldown
		0.0f, // switch cooldown
		0.0f, // use cooldown
		0.0f, // use cooldown threshold
		0.0f, // recoil velocity
		AK::EVENTS::PLAY_DRONE_ACTIVE_ARMOR,
		Asset::Mesh::icon_active_armor,
		Type::Other,
	},
	{
		DRONE_COOLDOWN_MAX, // movement cooldown
		0.3f, // switch cooldown
		20.0f, // use cooldown
		10.0f, // use cooldown threshold
		0.5f, // recoil velocity
		AK::EVENTS::PLAY_EQUIP_BUILD,
		Asset::Mesh::icon_rectifier,
		Type::Build,
	},
	{
		DRONE_COOLDOWN_MAX, // movement cooldown
		0.5f, // switch cooldown
		40.0f, // use cooldown
		20.0f, // use cooldown threshold
		0.5f, // recoil velocity
		AK::EVENTS::PLAY_EQUIP_BUILD,
		Asset::Mesh::icon_minion,
		Type::Build,
	},
	{
		DRONE_COOLDOWN_MAX, // movement cooldown
		0.5f, // switch cooldown
		40.0f, // use cooldown
		20.0f, // use cooldown threshold
		0.5f, // recoil velocity
		AK::EVENTS::PLAY_EQUIP_BUILD,
		Asset::Mesh::icon_turret,
		Type::Build,
	},
	{
		2.5f, // movement cooldown
		0.4f, // switch cooldown
		0.0f, // use cooldown
		0.0f, // use cooldown threshold
		1.7f, // recoil velocity
		AK::EVENTS::PLAY_EQUIP_SHOTGUN,
		Asset::Mesh::icon_shotgun,
		Type::Shoot,
	},
	{
		2.35f, // movement cooldown
		0.5f, // switch cooldown
		0.0f, // use cooldown
		0.0f, // use cooldown threshold
		1.5f, // recoil velocity
		AK::EVENTS::PLAY_EQUIP_SNIPER,
		Asset::Mesh::icon_sniper,
		Type::Shoot,
	},
	{
		DRONE_COOLDOWN_MAX, // movement cooldown
		0.5f, // switch cooldown
		30.0f, // use cooldown
		15.0f, // use cooldown threshold
		0.5f, // recoil velocity
		AK::EVENTS::PLAY_EQUIP_BUILD,
		Asset::Mesh::icon_force_field,
		Type::Build,
	},
	{
		DRONE_COOLDOWN_MAX, // movement cooldown
		0.3f, // switch cooldown
		15.0f, // use cooldown
		7.5f, // use cooldown threshold
		1.0f, // recoil velocity
		AK::EVENTS::PLAY_EQUIP_GRENADE,
		Asset::Mesh::icon_grenade,
		Type::Shoot,
	},
	{ // Ability::None
		2.1f, // movement cooldown
		0.3f, // switch cooldown
		0.0f, // use cooldown
		0.0f, // use cooldown threshold
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
		300,
		Type::Ability,
	},
	{
		strings::active_armor,
		strings::description_active_armor,
		Asset::Mesh::icon_active_armor,
		300,
		Type::Ability,
	},
	{
		strings::rectifier,
		strings::description_rectifier,
		Asset::Mesh::icon_rectifier,
		400,
		Type::Ability,
	},
	{
		strings::minion_spawner,
		strings::description_minion_spawner,
		Asset::Mesh::icon_minion,
		400,
		Type::Ability,
	},
	{
		strings::turret,
		strings::description_turret,
		Asset::Mesh::icon_turret,
		400,
		Type::Ability,
	},
	{
		strings::shotgun,
		strings::description_shotgun,
		Asset::Mesh::icon_shotgun,
		600,
		Type::Ability,
	},
	{
		strings::sniper,
		strings::description_sniper,
		Asset::Mesh::icon_sniper,
		600,
		Type::Ability,
	},
	{
		strings::force_field,
		strings::description_force_field,
		Asset::Mesh::icon_force_field,
		800,
		Type::Ability,
	},
	{
		strings::grenade,
		strings::description_grenade,
		Asset::Mesh::icon_grenade,
		800,
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

		if (Game::level.mode == Game::Mode::Parkour)
		{
			// spawn all batteries now
			for (s32 i = 0; i < Game::level.battery_spawns.length; i++)
				battery_spawn();
		}
	}

	battery_spawn_delay = 0.0f;
	winner = nullptr;
	score_summary.length = 0;
	for (s32 i = 0; i < MAX_PLAYERS * MAX_PLAYERS; i++)
		PlayerManager::visibility[i].value = false;
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
		if (Game::level.multiplayer_level_scheduled == AssetNull)
			Game::level.multiplayer_level_schedule();

		Game::Mode mode;
		if (match_state == MatchState::Done)
			mode = Game::session.config.time_limit_parkour_ready == 0 ? Game::Mode::Pvp : Game::Mode::Parkour;
		else
			mode = Game::level.mode;
		
		Game::schedule_load_level(Game::level.multiplayer_level_scheduled, mode);
	}
}

void Team::battery_spawn()
{
	const Game::BatterySpawnPoint& p = Game::level.battery_spawns[Game::level.battery_spawn_index];

	AI::Team team = (Game::session.config.game_type == GameType::Assault && Game::level.mode == Game::Mode::Pvp) ? 0 : AI::TeamNone;

	Entity* entity = World::create<BatteryEntity>(p.pos, p.spawn_point.ref(), team);
	Net::finalize(entity);
	Rope::spawn(p.pos + Vec3(0, 1, 0), Vec3(0, 1, 0), 100.0f);

	Game::level.battery_spawn_index++;
	battery_spawn_delay = 0.0f;
}

s16 Team::force_field_mask(AI::Team t)
{
	return 1 << (8 + t);
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

Team* Team::with_most_energy_collected()
{
	s16 highest_energy_collected = 0;
	Team* result = nullptr;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		s16 energy_collected = i.item()->energy_collected;
		if (energy_collected == highest_energy_collected)
			result = nullptr;
		else if (energy_collected > highest_energy_collected)
		{
			highest_energy_collected = energy_collected;
			result = i.item();
		}
	}
	return result;
}

b8 visibility_check(Entity* i, Entity* j, r32 i_range)
{
	Vec3 start = i->get<Transform>()->absolute_pos();
	Vec3 end = j->get<Transform>()->absolute_pos();
	Vec3 diff = end - start;

	r32 dist_sq = diff.length_squared();
	if (btFuzzyZero(dist_sq))
		return true;
	else if (dist_sq < i_range * i_range)
	{
		btCollisionWorld::ClosestRayResultCallback ray_callback(start, end);
		Physics::raycast(&ray_callback, CollisionAudio);
		if (!ray_callback.hasHit())
			return true;
	}

	return false;
}

// determine which rectifiers can see the given player
void get_rectifier_visibility(b8 visibility[MAX_TEAMS], Entity* player_entity)
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
			b8* entry = &visibility[s32(i.item()->team)];
			if (!(*entry))
			{
				Vec3 to_rectifier = i.item()->get<Transform>()->absolute_pos() - player_pos;
				if (to_rectifier.length_squared() < RECTIFIER_RANGE * RECTIFIER_RANGE
					&& to_rectifier.dot(normal) > 0.0f)
					*entry = true;
			}
		}
	}
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
	for (auto player = PlayerManager::list.iterator(); !player.is_last(); player.next())
	{
		Entity* player_entity = player.item()->instance.ref();
		if (player_entity && player_entity->has<Drone>())
		{
			AI::Team team = player.item()->team.ref()->team();
			b8 stealthing = false;
			UpgradeStation* upgrade_station = UpgradeStation::drone_inside(player_entity->get<Drone>());
			if (upgrade_station && upgrade_station->timer == 0.0f) // always stealthed inside upgrade stations (but not while transitioning)
				stealthing = true;
			else if (Game::time.total - player_entity->get<Drone>()->last_ability_fired < ABILITY_UNSTEALTH_TIME)
				stealthing = false;
			else if (player_entity->get<Drone>()->state() == Drone::State::Crawl) // we're on a wall and can thus be detected
			{
				b8 rectifier_visibility[MAX_TEAMS] = {};
				get_rectifier_visibility(rectifier_visibility, player_entity);
				stealthing = rectifier_visibility[team]; // if player's own rectifiers can see them, they're stealthing
				if (stealthing)
				{
					// unless another team's rectifiers can see them too
					for (s32 i = 0; i < Team::list.count(); i++)
					{
						if (i != team && rectifier_visibility[i])
						{
							stealthing = false;
							break;
						}
					}
				}
			}
			else
				stealthing = false; // always visible while flying or dashing
			Drone::stealth(player_entity, stealthing);
		}
	}

	// update player visibility
	for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
	{
		Entity* i_entity = i.item()->instance.ref();

		if (!i_entity)
			continue;

		Team* i_team = i.item()->team.ref();

		r32 i_range = i_entity->has<Drone>() ? i_entity->get<Drone>()->range() : DRONE_MAX_DISTANCE;

		for (auto j = PlayerManager::list.iterator(); !j.is_last(); j.next())
		{
			Team* j_team = j.item()->team.ref();

			PlayerManager::Visibility* visibility = &PlayerManager::visibility[PlayerManager::visibility_hash(i.item(), j.item())];

			if (i_team == j_team)
				visibility->value = true;
			else
			{
				Entity* j_entity = j.item()->instance.ref();
				if (j_entity && j_entity->get<AIAgent>()->stealth < 1.0f)
					visibility->value = visibility_check(i_entity, j_entity, i_range);
				else
					visibility->value = false;
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
		serialize_s16(p, t->energy_collected);
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
					i.item()->flag(PlayerManager::FlagScoreAccepted, false);
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
						if (Game::session.config.game_type == GameType::CaptureTheFlag)
							team_add_score_summary_item(i.item(), _(strings::flags), i.item()->flags_captured);
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
			s16 energy_collected;
			serialize_s16(p, energy_collected);
			if (!Game::level.local || src == Net::MessageSource::Loopback)
			{
				t.ref()->kills = kills;
				t.ref()->flags_captured = flags_captured;
				t.ref()->energy_collected = energy_collected;
			}
			break;
		}
		case TeamNet::Message::MapSchedule:
		{
			serialize_s16(p, Game::level.multiplayer_level_scheduled);
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
		if (!i.item()->flag(PlayerManager::FlagCanSpawn))
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
			&& Game::scheduled_load_level == AssetNull
			&& PlayerHuman::list.count() < vi_max(s32(Game::session.config.min_players), Game::session.config.fill_bots ? 1 : 2))
		{
			if (Game::session.config.time_limit_parkour_ready > 0) // go back to parkour mode
			{
#if SERVER
				Net::Server::transition_level();
#endif
				Game::schedule_load_level(Game::level.id, Game::Mode::Parkour);
			}
			else // go back to Waiting state
				match_waiting();
		}
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
				if (!i.item()->flag(PlayerManager::FlagCanSpawn))
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

	update_visibility(u);

	// fill bots
	if ((match_state == MatchState::Waiting || match_state == MatchState::TeamSelect || match_state == MatchState::Active)
		&& Game::level.mode == Game::Mode::Pvp
		&& Game::session.config.fill_bots
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
		if (Game::level.mode == Game::Mode::Parkour && Game::session.type == SessionType::Multiplayer)
		{
			if (PlayerManager::list.count() >= Game::session.config.min_players && Game::scheduled_load_level == AssetNull)
			{
				r32 time_limit = 60.0f * r32(Game::session.config.time_limit_parkour_ready);
				if (PlayerManager::parkour_ready_count() == PlayerManager::list.count())
				{
					r32 t = time_limit - r32(PARKOUR_GAME_START_COUNTDOWN);
					if (Team::match_time < t)
					{
						Team::match_time = t;
#if SERVER
						Net::Server::sync_time();
#endif
					}
				}

				if (time_limit > 0.0f && Team::match_time > time_limit)
				{
					// start the match
#if SERVER
					Net::Server::transition_level();
#endif
					Game::schedule_load_level(Game::level.id, Game::Mode::Pvp);
				}
			}
		}
		else if (Game::level.mode == Game::Mode::Pvp)
		{
			Team* team_with_most_kills = Game::session.config.game_type == GameType::Deathmatch ? with_most_kills() : nullptr;
			Team* team_with_most_flags = Game::session.config.game_type == GameType::CaptureTheFlag ? with_most_flags() : nullptr;
			if (!Game::level.noclip
				&& ((match_time > Game::session.config.time_limit() && Game::level.has_feature(Game::FeatureLevel::All)) // no time limit in tutorial
					|| (Game::level.has_feature(Game::FeatureLevel::All) && teams_with_active_players() <= 1 && Game::level.ai_config.length == 0)
					|| (Game::session.config.game_type == GameType::Assault && Battery::list.count() == 0 && Game::level.battery_spawn_index >= Game::level.battery_spawns.length)
					|| (Game::session.config.game_type == GameType::Deathmatch && team_with_most_kills && team_with_most_kills->kills >= Game::session.config.kill_limit)
					|| (Game::session.config.game_type == GameType::CaptureTheFlag && team_with_most_flags && team_with_most_flags->flags_captured >= Game::session.config.flag_limit)))
			{
				// determine the winner, if any
				Team* w = nullptr; // winning team
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
					if (Battery::list.count() == 0)
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

				for (auto i = list.iterator(); !i.is_last(); i.next())
					TeamNet::update_counts(i.item());
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

				if (Battery::list.count() == 0
					&& Game::session.config.ruleset.enable_batteries
					&& Game::level.battery_spawn_index < Game::level.battery_spawns.length
					&& (Game::level.feature_level == Game::FeatureLevel::Batteries || Game::level.has_feature(Game::FeatureLevel::TutorialAll)))
				{
					battery_spawn_delay += u.time.delta;
					if (battery_spawn_delay > 10.0f)
					{
						s32 end = vi_min(s32(Game::level.battery_spawns.length), s32(Game::level.battery_spawn_index + Game::level.battery_spawn_group_size));
						for (s32 i = Game::level.battery_spawn_index; i < end; i++)
							battery_spawn();
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
	}

	if (Game::level.mode == Game::Mode::Pvp && match_state == MatchState::Done && Game::scheduled_load_level == AssetNull)
	{
		// wait for all local players to accept scores
		b8 score_accepted = true;
		if (Game::real_time.total - game_over_real_time < SCORE_SUMMARY_ACCEPT_TIME) // automatically move on after 45 seconds
		{
			for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
			{
				if (!i.item()->get<PlayerManager>()->flag(PlayerManager::FlagScoreAccepted))
				{
					score_accepted = false;
					break;
				}
			}
		}

		if (score_accepted)
			transition_next(); // time to get out of here
	}
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
	update_visibility(u);
}


SpawnPoint* Team::get_spawn_point() const
{
	if (Game::level.mode == Game::Mode::Parkour || Game::session.config.game_type == GameType::Deathmatch)
	{
		// random
		Array<SpawnPoint*> spawns_good;
		Array<SpawnPoint*> spawns_bad;
		for (auto i = SpawnPoint::list.iterator(); !i.is_last(); i.next())
		{
			AI::Team spawn_team = i.item()->team;
			if (spawn_team == AI::TeamNone || spawn_team == team())
			{
				Vec3 pos = i.item()->spawn_position().pos;

				b8 good = true;

				if (i.item()->battery())
					good = false;
				else
				{
					for (auto j = PlayerCommon::list.iterator(); !j.is_last(); j.next())
					{
						if (j.item()->get<AIAgent>()->team != team() && (j.item()->get<Transform>()->absolute_pos() - pos).length_squared() < 8.0f * 8.0f)
						{
							good = false;
							break;
						}
					}
				}

				if (good)
					spawns_good.add(i.item());
				else
					spawns_bad.add(i.item());
			}
		}

		if (spawns_good.length > 0)
			return spawns_good[mersenne::rand() % spawns_good.length];
		else
			return spawns_bad[mersenne::rand() % spawns_bad.length];
	}
	else
	{
		// only one spawn point
		for (auto i = SpawnPoint::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->team == team() && !i.item()->battery())
				return i.item();
		}
	}
	return nullptr;
}

PlayerManager::Visibility PlayerManager::visibility[MAX_PLAYERS * MAX_PLAYERS];

PlayerManager::PlayerManager(Team* team, const char* u)
	: spawn_timer(Game::session.config.ruleset.spawn_delay),
	flags
	(
		(Team::match_state == Team::MatchState::Done ? FlagScoreAccepted : 0)
		| (Game::session.type == SessionType::Story || Team::match_state == Team::MatchState::Active ? FlagCanSpawn : 0)
	),
	team(team),
	upgrades(Game::session.config.ruleset.upgrades_default),
	abilities{ Ability::None, Ability::None },
	instance(),
	spawn(),
	current_upgrade(Upgrade::None),
	state_timer(),
	upgrade_completed(),
	energy(Game::session.config.ruleset.start_energy),
	kills(),
	deaths(),
	flags_captured(),
	ability_cooldown(),
	ability_flash_time(),
	current_upgrade_ability_slot(),
	team_scheduled(AI::TeamNone)
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

	if (u)
		strncpy(username, u, MAX_USERNAME);
	else
		username[0] = '\0';
}

void PlayerManager::awake()
{
	if (Game::level.mode != Game::Mode::Special && !Game::level.local)
	{
		char log[512];
		sprintf(log, _(strings::player_joined), username);
		PlayerHuman::log_add(log, team.ref()->team());
	}

	if (Game::level.mode == Game::Mode::Parkour
		&& Game::session.type == SessionType::Multiplayer
		&& PlayerManager::list.count() > 1
		&& PlayerManager::list.count() == Game::session.config.min_players)
	{
		// we just now have enough players to start the game
		// so restart the countdown
		Team::match_time = 0.0f;
#if SERVER
		Net::Server::sync_time();
#endif
	}
}

PlayerManager::~PlayerManager()
{
	if (Game::level.mode != Game::Mode::Special)
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
		if (info.cooldown_use > 0.0f && ability_cooldown[s32(ability)] >= info.cooldown_use_threshold)
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

	b8 parkour_ready(PlayerManager* m, b8 value)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::PlayerManager);
		{
			Ref<PlayerManager> ref = m;
			serialize_ref(p, ref);
		}
		{
			PlayerManager::Message msg = PlayerManager::Message::ParkourReady;
			serialize_enum(p, PlayerManager::Message, msg);
		}
		serialize_bool(p, value);
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
		serialize_s16(p, m->energy_collected);
		Net::msg_finalize(p);
		return true;
	}

	b8 team_switch(PlayerManager* m, AI::Team t)
	{
		vi_assert(Game::level.local);

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
		serialize_r32(p, m->ability_cooldown[s32(a)]);
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

s32 PlayerManager::parkour_ready_count()
{
	s32 result = 0;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->flag(FlagParkourReady))
			result++;
	}
	return result;
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
				m->flag(FlagCanSpawn, value);

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
						if (!m->flag(FlagCanSpawn))
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
		case Message::ParkourReady:
		{
			b8 value;
			serialize_bool(p, value);

			if (!m)
				return true;

			if (Game::level.local && src == Net::MessageSource::Remote)
				PlayerManagerNet::parkour_ready(m, value); // repeat to clients and self
			else if (Game::level.local == (src == Net::MessageSource::Loopback))
				m->flag(FlagParkourReady, value);

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
			m->flag(FlagScoreAccepted, true);
			break;
		}
		case Message::UpgradeFailed:
		{
			if (!m)
				return true;

			if (Game::level.local == (src == Net::MessageSource::Loopback))
			{
				m->current_upgrade = Upgrade::None;
				m->state_timer = 0.0f;
				if (m->has<PlayerHuman>() && m->get<PlayerHuman>()->local())
					Audio::post_global(AK::EVENTS::STOP_DRONE_UPGRADE, m->get<PlayerHuman>()->gamepad);
			}
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
						{
							const AbilityInfo& info = AbilityInfo::list[s32(u)];
							if (info.type == AbilityInfo::Type::Passive)
								instance->get<Drone>()->current_ability = Ability::None;
							else
								instance->get<Drone>()->current_ability = Ability(u);
						}
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
			s16 energy_collected;
			serialize_s16(p, kills);
			serialize_s16(p, deaths);
			serialize_s16(p, flags_captured);
			serialize_s16(p, energy_collected);

			if (!m)
				return true;
		
			if (Game::level.local == (src == Net::MessageSource::Loopback)) // server does not accept these messages from clients
			{
				m->kills = kills;
				m->deaths = deaths;
				m->flags_captured = flags_captured;
				m->energy_collected = energy_collected;
			}
			break;
		}
		case Message::MakeOtherAdmin:
		{
			Ref<PlayerManager> target;
			serialize_ref(p, target);
			b8 value;
			serialize_bool(p, value);

			if (!m || !target.ref() || !m->flag(FlagIsAdmin) || !target.ref()->has<PlayerHuman>())
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
				m->flag(FlagIsAdmin, value);
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

			if (!m || !target.ref() || !m->flag(FlagIsAdmin) || !target.ref()->has<PlayerHuman>())
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

			if (!m->flag(FlagIsAdmin) || m == kickee.ref())
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

			if (!m->flag(FlagIsAdmin) || (map != AssetNull && Overworld::zone_max_teams(map) < Game::session.config.team_count))
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

			if (!m->flag(FlagIsAdmin) || Overworld::zone_max_teams(map) < Game::session.config.team_count)
				net_error();

			{
				AssetID uuid = Overworld::zone_uuid_for_id(map);
				if (!LEVEL_ALLOWED(uuid))
					net_error();
			}

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
			r32 value;
			serialize_r32(p, value);

			if (!m)
				return true;

			if (m->is_local() && m->has<PlayerHuman>())
				Audio::post_global(AK::EVENTS::PLAY_DRONE_CHARGE_RESTORE, m->get<PlayerHuman>()->gamepad);

			m->ability_cooldown[s32(a)] = value;

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

		if (has<PlayerHuman>())
			Audio::post_global(AK::EVENTS::PLAY_DRONE_UPGRADE, get<PlayerHuman>()->gamepad);

		return true;
	}
	else
	{
#if SERVER
		// let client know that we failed to start the upgrade
		PlayerManagerNet::send(this, PlayerManager::Message::UpgradeFailed);
#endif
		return false;
	}
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
	if (c > 0)
	{
		energy_collected = s16(s32(energy_collected) + c);
		team.ref()->energy_collected = s16(s32(team.ref()->energy_collected) + c);
	}
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
		vi_assert(ability_cooldown[s32(a)] < info.cooldown_use_threshold);
		ability_cooldown[s32(a)] += info.cooldown_use;
	}
}

void PlayerManager::make_admin(b8 value)
{
	vi_assert(Game::level.local);
	PlayerManagerNet::make_admin(this, value);
}

void PlayerManager::make_admin(PlayerManager* other, b8 value)
{
	vi_assert(flag(FlagIsAdmin));
	PlayerManagerNet::make_other_admin(this, other, value);
}

void PlayerManager::kick(PlayerManager* kickee)
{
	vi_assert(flag(FlagIsAdmin) && kickee != this);
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
	vi_assert(flag(FlagIsAdmin));
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
	else if (e->has<Turret>())
		return e->get<Turret>()->owner.ref();
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

	for (auto i = Turret::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->owner.ref() == this)
			i.item()->owner = nullptr;
	}

	for (auto i = MinionSpawner::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->owner.ref() == this)
			i.item()->owner = nullptr;
	}
}

void killer_name(Entity* killer, PlayerManager* killer_player, PlayerManager* player, char* result)
{
	if (killer_player)
		strncpy(result, killer_player->username, MAX_USERNAME);
	else
	{
		Entity* logged_killer = killer;
		if (killer->has<Bolt>())
			logged_killer = killer->get<Bolt>()->owner.ref();

		if (logged_killer && logged_killer->has<Turret>())
			strncpy(result, _(strings::turret), MAX_USERNAME);
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
			else if (e->has<MinionSpawner>())
				reward = ENERGY_MINION_SPAWNER_DESTROY;
			else if (e->has<Turret>())
				reward = ENERGY_TURRET_DESTROY;
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
	if (flag(FlagCanSpawn)
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
					internal_spawn_go(this, team.ref()->get_spawn_point());
			}
		}
		else if (Game::level.mode == Game::Mode::Parkour)
			internal_spawn_go(this, team.ref()->get_spawn_point());
	}

	for (s32 i = 0; i < s32(Ability::count); i++)
	{
		const AbilityInfo& info = AbilityInfo::list[i];
		b8 ready_previous = ability_cooldown[i] < info.cooldown_use_threshold;
		ability_cooldown[i] = vi_max(0.0f, ability_cooldown[i] - u.time.delta);
		b8 ready_now = ability_cooldown[i] < info.cooldown_use_threshold;
		if (ready_now && !ready_previous)
			PlayerManagerNet::ability_cooldown_ready(this, Ability(i));
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
					PlayerManagerNet::upgrade_completed(this, current_upgrade_ability_slot, current_upgrade);
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
		const AbilityInfo& info = AbilityInfo::list[i];
		r32 min = ability_cooldown[i] >= info.cooldown_use_threshold ? info.cooldown_use_threshold : 0.0f;
		ability_cooldown[i] = vi_max(min, ability_cooldown[i] - u.time.delta); // can't set it below threshold until server says we can
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

void PlayerManager::parkour_ready(b8 value)
{
	PlayerManagerNet::parkour_ready(this, value);
}


}
