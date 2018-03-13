#include "master.h"
#include "cjson/cJSON.h"
#include "data/json.h"

namespace VI
{

namespace Net
{

namespace Master
{

#define DEBUG_MSG 0
#define NET_MASTER_RESEND_INTERVAL 0.5

Messenger::Peer::Peer()
	: incoming_seq(NET_SEQUENCE_COUNT - 1),
	outgoing_seq(0)
{

}

SequenceID Messenger::outgoing_sequence_id(const Sock::Address& addr) const
{
	auto i = sequence_ids.find(addr.hash());
	if (i == sequence_ids.end())
		return 0;
	else
		return i->second.outgoing_seq;
}

b8 Messenger::has_unacked_outgoing_messages(const Sock::Address& addr) const
{
	for (s32 i = 0; i < outgoing.length; i++)
	{
		if (outgoing[i].addr.equals(addr))
			return true;
	}
	return false;
}

b8 Messenger::add_header(StreamWrite* p, const Sock::Address& addr, Message type)
{
	using Stream = StreamWrite;
	SequenceID seq = outgoing_sequence_id(addr);
	{
		s16 version = GAME_VERSION;
		serialize_s16(p, version);
	}
	serialize_int(p, SequenceID, seq, 0, NET_SEQUENCE_COUNT - 1);
#if DEBUG_MSG
	{
		char str[NET_MAX_ADDRESS];
		addr.str(str);
		vi_debug("Sending seq %d message %d to %s", s32(seq), s32(type), str);
	}
#endif
	serialize_enum(p, Message, type);
	return true;
}

void Messenger::send(const StreamWrite& p, r64 timestamp, const Sock::Address& addr, Sock::Handle* sock)
{
	last_sent_timestamp = timestamp;
	SequenceID seq = outgoing_sequence_id(addr);

	OutgoingPacket* packet = outgoing.add();
	packet->data = p;
	packet->sequence_id = seq;
	packet->timestamp = timestamp;
	packet->addr = addr;

	SequenceID seq_next = sequence_advance(seq, 1);
	{
		u64 hash = addr.hash();
		auto i = sequence_ids.find(hash);
		if (i == sequence_ids.end())
		{
			// haven't sent a message to this address yet
			Peer peer;
			peer.outgoing_seq = seq_next;
			sequence_ids[hash] = peer;
		}
		else // update existing sequence ID
			i->second.outgoing_seq = seq_next;
	}

	Sock::udp_send(sock, addr, p.data.data, p.bytes_written());
}

b8 messenger_send_ack(SequenceID seq, Sock::Address addr, Sock::Handle* sock)
{
	using Stream = StreamWrite;
	StreamWrite p;
	packet_init(&p);
	{
		s16 version = GAME_VERSION;
		serialize_s16(&p, version);
	}
	serialize_int(&p, SequenceID, seq, 0, NET_SEQUENCE_COUNT - 1);
	{
		Message ack_type = Message::Ack;
		serialize_enum(&p, Message, ack_type);
	}
	packet_finalize(&p);
	Sock::udp_send(sock, addr, p.data.data, p.bytes_written());
	return true;
}

void Messenger::received(Message type, SequenceID seq, const Sock::Address& addr, Sock::Handle* sock)
{
	if (type == Message::Ack)
	{
		// they are acking a sequence we sent
		// remove that sequence from our outgoing queue

		for (s32 i = 0; i < outgoing.length; i++)
		{
			if (outgoing[i].sequence_id == seq)
			{
				outgoing.remove(i);
				break;
			}
		}
	}
	else if (type != Message::Disconnect)
	{
		// when we receive any kind of message other than an ack, we must send an ack back

#if DEBUG_MSG
		{
			char str[NET_MAX_ADDRESS];
			addr.str(str);
			vi_debug("Received seq %d message %d from %s", s32(seq), s32(type), str);
		}
#endif

		messenger_send_ack(seq, addr, sock);
	}
}

void Messenger::update(r64 timestamp, Sock::Handle* sock, s32 max_outgoing)
{
	if (max_outgoing > 0 && outgoing.length > max_outgoing)
		reset();
	else
	{
		r64 timestamp_cutoff = timestamp - NET_MASTER_RESEND_INTERVAL;
		for (s32 i = 0; i < outgoing.length; i++)
		{
			OutgoingPacket* packet = &outgoing[i];
			if (packet->timestamp < timestamp_cutoff)
			{
#if DEBUG_MSG
				{
					char str[NET_MAX_ADDRESS];
					packet->addr.str(str);
					vi_debug("Resending seq %d to %s", s32(packet->sequence_id), str);
				}
#endif
				packet->timestamp = timestamp;
				Sock::udp_send(sock, packet->addr, packet->data.data.data, packet->data.bytes_written());
			}
		}
	}
}

void Messenger::cancel_outgoing()
{
#if DEBUG_MSG
	vi_debug("%s", "Canceling all outgoing messages");
#endif
	outgoing.length = 0;
}

void Messenger::reset()
{
	cancel_outgoing();
#if DEBUG_MSG
	vi_debug("%s", "Resetting all connections");
#endif
	sequence_ids.clear();
}

void Messenger::remove(const Sock::Address& addr)
{
#if DEBUG_MSG
	{
		char str[NET_MAX_ADDRESS];
		addr.str(str);
		vi_debug("Removing peer %s", str);
	}
#endif
	for (s32 i = 0; i < outgoing.length; i++)
	{
		if (outgoing[i].addr.equals(addr))
		{
			outgoing.remove(i);
			i--;
		}
	}
	sequence_ids.erase(addr.hash());
}

Ruleset Ruleset::presets[s32(Preset::count)];

void Ruleset::init()
{
	{
		// Standard
		Ruleset* ruleset = &presets[s32(Preset::Standard)];
	}
	{
		// Arcade
		Ruleset* ruleset = &presets[s32(Preset::Arcade)];
		ruleset->enable_batteries = false;
		ruleset->upgrades_allow = 0;
		ruleset->upgrades_default = (1 << s32(Upgrade::count)) - 1;
	}
	{
		// Custom
		Ruleset* ruleset = &presets[s32(Preset::Custom)];
	}
}

const char* ServerConfig::game_type_string(GameType type)
{
	switch (type)
	{
		case GameType::Deathmatch:
			return "dm";
		case GameType::Assault:
			return "as";
		case GameType::CaptureTheFlag:
			return "ctf";
		default:
			vi_assert(false);
			return nullptr;
	}
}

const char* ServerConfig::game_type_string_human(GameType type)
{
	// not localized because this is used for Discord rich presence
	switch (type)
	{
		case GameType::Deathmatch:
			return "DM";
		case GameType::Assault:
			return "Assault";
		case GameType::CaptureTheFlag:
			return "CTF";
		default:
			vi_assert(false);
			return nullptr;
	}
}

void server_config_parse(const char* text, ServerConfig* config)
{
	cJSON* json = cJSON_Parse(text);
	server_config_parse(json, config);
	cJSON_Delete(json);
}

void server_config_parse(cJSON* json, ServerConfig* config)
{
	// id, name, max_players, creator_id, game_type, team_count, preset, secret, region, and is_private are stored in DB row, not here
	{
		config->levels.length = 0;
		cJSON* levels = cJSON_GetObjectItem(json, "levels");
		cJSON* level = levels->child;
		while (level)
		{
			config->levels.add(level->valueint);
			level = level->next;
		}
	}
	ServerConfig defaults;
	new (&defaults) ServerConfig();

	config->ruleset.upgrades_allow = s16(Json::get_s32(json, "upgrades_allow", defaults.ruleset.upgrades_allow));
	config->ruleset.upgrades_default = s16(Json::get_s32(json, "upgrades_default", defaults.ruleset.upgrades_default));
	{
		cJSON* start_abilities = cJSON_GetObjectItem(json, "start_abilities");
		if (start_abilities)
		{
			cJSON* u = start_abilities->child;
			while (u && config->ruleset.start_abilities.length < config->ruleset.start_abilities.capacity())
			{
				config->ruleset.start_abilities.add(Ability(u->valueint));
				u = u->next;
			}
		}
	}
	config->ruleset.enable_batteries = b8(Json::get_s32(json, "enable_batteries", defaults.ruleset.enable_batteries));
	config->ruleset.drone_shield = s8(Json::get_s32(json, "drone_shield", defaults.ruleset.drone_shield));
	config->ruleset.spawn_delay = s8(vi_max(1, vi_min(120, s32(Json::get_s32(json, "spawn_delay", defaults.ruleset.spawn_delay)))));
	config->ruleset.start_energy = s16(Json::get_s32(json, "start_energy", defaults.ruleset.start_energy));
	config->ruleset.cooldown_speed_index = u8(Json::get_s32(json, "cooldown_speed_index", defaults.ruleset.cooldown_speed_index));

	config->min_players = s8(Json::get_s32(json, "min_players", defaults.min_players));
	config->time_limit_parkour_ready = u8(Json::get_s32(json, "time_limit_parkour_ready", defaults.time_limit_parkour_ready));
	for (s32 i = 0; i < s32(GameType::count); i++)
	{
		char key[64];
		snprintf(key, 64, "time_limit_minutes_%s", ServerConfig::game_type_string(GameType(i)));
		config->time_limit_minutes[i] = u8(Json::get_s32(json, key, defaults.time_limit_minutes[i]));
	}
	config->fill_bots = s8(Json::get_s32(json, "fill_bots", defaults.fill_bots));
	config->kill_limit = s16(Json::get_s32(json, "kill_limit", defaults.kill_limit));
	config->flag_limit = s16(Json::get_s32(json, "flag_limit", defaults.flag_limit));
	strncpy(config->secret, Json::get_string(json, "secret", ""), MAX_SERVER_CONFIG_SECRET);
}

// caller must free() the returned string
char* server_config_stringify(const ServerConfig& config)
{
	cJSON* json = server_config_json(config);
	char* result = cJSON_Print(json);
	cJSON_Delete(json);
	return result;
}

cJSON* server_config_json(const ServerConfig& config)
{
	ServerConfig defaults;
	new (&defaults) ServerConfig();

	cJSON* json = cJSON_CreateObject();

	if (config.ruleset.upgrades_allow != defaults.ruleset.upgrades_allow)
		cJSON_AddNumberToObject(json, "upgrades_allow", config.ruleset.upgrades_allow);
	if (config.ruleset.upgrades_default != defaults.ruleset.upgrades_default)
		cJSON_AddNumberToObject(json, "upgrades_default", config.ruleset.upgrades_default);
	if (config.ruleset.start_abilities.length > 0)
	{
		cJSON* start_abilities = cJSON_CreateArray();
		cJSON_AddItemToObject(json, "start_abilities", start_abilities);
		for (s32 i = 0; i < config.ruleset.start_abilities.length; i++)
			cJSON_AddItemToArray(start_abilities, cJSON_CreateNumber(s32(config.ruleset.start_abilities[i])));
	}
	if (config.ruleset.enable_batteries != defaults.ruleset.enable_batteries)
		cJSON_AddNumberToObject(json, "enable_batteries", config.ruleset.enable_batteries);
	if (config.ruleset.drone_shield != defaults.ruleset.drone_shield)
		cJSON_AddNumberToObject(json, "drone_shield", config.ruleset.drone_shield);
	if (config.ruleset.spawn_delay != defaults.ruleset.spawn_delay)
		cJSON_AddNumberToObject(json, "spawn_delay", config.ruleset.spawn_delay);
	if (config.ruleset.start_energy != defaults.ruleset.start_energy)
		cJSON_AddNumberToObject(json, "start_energy", config.ruleset.start_energy);
	if (config.ruleset.cooldown_speed_index != defaults.ruleset.cooldown_speed_index)
		cJSON_AddNumberToObject(json, "cooldown_speed_index", config.ruleset.cooldown_speed_index);

	{
		cJSON* levels = cJSON_CreateArray();
		cJSON_AddItemToObject(json, "levels", levels);
		for (s32 i = 0; i < config.levels.length; i++)
			cJSON_AddItemToArray(levels, cJSON_CreateNumber(config.levels[i]));
	}
	if (config.min_players != defaults.min_players)
		cJSON_AddNumberToObject(json, "min_players", config.min_players);
	if (config.fill_bots != defaults.fill_bots)
		cJSON_AddNumberToObject(json, "fill_bots", config.fill_bots);
	if (config.kill_limit != defaults.kill_limit)
		cJSON_AddNumberToObject(json, "kill_limit", config.kill_limit);
	if (config.flag_limit != defaults.flag_limit)
		cJSON_AddNumberToObject(json, "flag_limit", config.flag_limit);
	if (config.time_limit_parkour_ready != defaults.time_limit_parkour_ready)
		cJSON_AddNumberToObject(json, "time_limit_parkour_ready", config.time_limit_parkour_ready);
	for (s32 i = 0; i < s32(GameType::count); i++)
	{
		if (config.time_limit_minutes[i] != defaults.time_limit_minutes[i])
		{
			char key[64];
			snprintf(key, 64, "time_limit_minutes_%s", ServerConfig::game_type_string(GameType(i)));
			cJSON_AddNumberToObject(json, key, config.time_limit_minutes[i]);
		}
	}

	return json; // caller must cJSON_Delete() it
}

void server_list_entry_for_config(ServerListEntry* entry, const ServerConfig& config)
{
	entry->max_players = MAX_GAMEPADS;
	strncpy(entry->name, config.name, MAX_SERVER_CONFIG_NAME);
	entry->game_type = config.game_type;
	entry->team_count = config.team_count;
	entry->preset = config.preset;
	entry->server_state.id = config.id;
	entry->server_state.level = AssetNull;
	entry->server_state.max_players = MAX_GAMEPADS;
	entry->server_state.player_slots = MAX_PLAYERS;
	entry->creator_username[0] = '\0';
	entry->creator_vip = false;
}


}

}

}
