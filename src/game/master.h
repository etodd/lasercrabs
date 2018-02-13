#pragma once
#include "types.h"
#include "net_serialize.h"
#include "platform/sock.h"
#include <unordered_map>
#include "constants.h"
#include "data/array.h"

namespace VI
{

namespace Net
{

namespace Master
{

// master server gives this to clients to authenticate further requests to the master server and game servers
struct UserKey
{
	u32 id;
	u32 token;
	b8 equals(const UserKey& other) const
	{
		return id == other.id && token == other.token;
	}
};

enum class ClientConnectionStep : s8
{
	ContactingMaster,
	AllocatingServer,
	WaitingForSlot,
	count,
};

enum class Message : s8
{
	Ack,
	Auth, // client logging in to master
	AuthResponse, // master responding to client with auth info
	ReauthRequired, // master telling client to try the whole authentication process again
	ServerStatusUpdate, // game server telling master what it's up to
	ServerLoad, // master telling a server to load a certain level
	ExpectClient, // master telling a server to expect a certain client to connect to it
	ClientConnectionStep, // master telling a client the status of their connection request
	ClientConnect, // master telling a client to connect to a game server
	ClientRequestServer, // a client requesting to connect to a virtual server; master will allocate it if necessary
	ClientRequestServerDetails, // a client requesting to connect to a virtual server; master will allocate it if necessary
	RequestServerConfig, // a client or server requesting ServerConfig data from the master
	ClientRequestServerList, // a client requesting a server list from the master
	ServerList, // master responding to a client with a server list
	ClientSaveServerConfig, // a client telling the master server to create or update a server config
	ClientRequestAscension, // a client is requesting an ascension username from the master server
	Ascension, // master server responds with an ascension username
	ServerConfig, // master responding to a client or server with ServerConfig data
	ServerDetails, // master responding to a client with ServerDetails data
	ServerConfigSaved, // master telling a client their config was created
	WrongVersion, // master telling a server or client that it needs to upgrade
	FriendshipGet, // client asking the master about the friendship state of two players
	FriendshipState, // master tell a client about the friendship state of two players
	FriendAdd, // client telling master to add a friend
	FriendRemove, // client telling master to remove a friend
	UserRoleSet, // client or server telling master to set somebody's role within a server
	Keepalive,
	Disconnect,
	count,
};

struct Messenger
{
	struct Peer
	{
		SequenceID incoming_seq;
		SequenceID outgoing_seq;
		Peer();
	};

	struct OutgoingPacket
	{
		r64 timestamp;
		StreamWrite data;
		SequenceID sequence_id;
		Sock::Address addr;
	};

	r64 last_sent_timestamp;
	Array<OutgoingPacket> outgoing; // unordered, unacked messages
	std::unordered_map<u64, Peer> sequence_ids;

	SequenceID outgoing_sequence_id(const Sock::Address&) const;
	b8 has_unacked_outgoing_messages(const Sock::Address&) const;

	b8 add_header(StreamWrite*, const Sock::Address&, Message);
	void update(r64, Sock::Handle*, s32 = 0);
	void remove(const Sock::Address&);
	void reset();
	void cancel_outgoing();

	// these assume packets have already been checksummed and compressed
	void send(const StreamWrite&, r64, const Sock::Address&, Sock::Handle*);
	void received(Message, SequenceID, const Sock::Address&, Sock::Handle*);
};

struct ServerState // represents the current state of a game server
{
	u32 id; // the virtual server configuration currently active on this game server; 0 if it's story mode
	AssetID level;
	s8 player_slots; // for servers, this is the number of open player slots. for clients, this is the number of players the client has locally
	s8 max_players; // for servers, this is the max number of players. for clients, it's unused
	StoryModeTeam story_mode_team;
	Region region;
};

template<typename Stream> b8 serialize_server_state(Stream* p, ServerState* s)
{
	serialize_u32(p, s->id);
	serialize_s16(p, s->level);
	serialize_int(p, s8, s->player_slots, 0, MAX_PLAYERS);
	serialize_int(p, s8, s->max_players, 0, MAX_PLAYERS);
	serialize_enum(p, Region, s->region);
	serialize_enum(p, StoryModeTeam, s->story_mode_team);
	return true;
}

#define MAX_SERVER_CONFIG_NAME 127
#define MAX_SERVER_LIST 8

struct Ruleset
{
	enum class Preset : s8
	{
		Standard,
		Arcade,
		Custom,
		count,
	};

	static Ruleset presets[s32(Preset::count)];

	static void init();

	s16 upgrades_allow = s16((1 << s32(Upgrade::count)) - 1);
	s16 upgrades_default;
	s16 start_energy = ENERGY_INITIAL;
	StaticArray<Ability, MAX_ABILITIES> start_abilities;
	s8 spawn_delay = 8;
	s8 drone_shield = DRONE_SHIELD_AMOUNT;
	u8 cooldown_speed_index = 4; // multiply by 0.25 to get actual value
	b8 enable_batteries = true;
	b8 enable_battery_stealth = true;

	r32 cooldown_speed() const
	{
		return r32(cooldown_speed_index) * 0.25f;
	}
};

struct ServerListEntry
{
	ServerState server_state;
	Sock::Address addr;
	char name[MAX_SERVER_CONFIG_NAME + 1];
	char creator_username[MAX_USERNAME + 1];
	s8 max_players;
	s8 team_count;
	Ruleset::Preset preset;
	GameType game_type;
};

template<typename Stream> b8 serialize_server_list_entry(Stream* p, ServerListEntry* s)
{
	if (!serialize_server_state(p, &s->server_state))
		net_error();

	if (s->server_state.level != AssetNull)
	{
		if (!Sock::Address::serialize(p, &s->addr))
			net_error();
	}
	else if (Stream::IsReading)
		s->addr = {};

	{
		s32 name_length;
		if (Stream::IsWriting)
			name_length = s32(strlen((const char*)s->name));
		serialize_int(p, s32, name_length, 0, MAX_SERVER_CONFIG_NAME);
		serialize_bytes(p, (u8*)s->name, name_length);
		if (Stream::IsReading)
			s->name[name_length] = '\0';
	}

	{
		s32 creator_length;
		if (Stream::IsWriting)
			creator_length = s32(strlen((const char*)s->creator_username));
		serialize_int(p, s32, creator_length, 0, MAX_USERNAME);
		serialize_bytes(p, (u8*)s->creator_username, creator_length);
		if (Stream::IsReading)
			s->creator_username[creator_length] = '\0';
	}

	serialize_enum(p, GameType, s->game_type);
	serialize_enum(p, Ruleset::Preset, s->preset);
	serialize_int(p, s8, s->max_players, 2, MAX_PLAYERS);
	serialize_int(p, s8, s->team_count, 2, MAX_TEAMS);

	return true;
}

#define MAX_SERVER_CONFIG_SECRET 127

struct ServerConfig
{
	static const char* game_type_string(GameType);
	static const char* game_type_string_human(GameType);

	u32 id;
	u32 creator_id;
	StaticArray<AssetID, 32> levels;
	s16 kill_limit = 10;
	s16 flag_limit = 3;
	Ruleset ruleset;
	Region region;
	Ruleset::Preset preset;
	GameType game_type = GameType::Assault;
	s8 max_players = MAX_PLAYERS;
	s8 min_players = 1;
	s8 team_count = 2;
	u8 time_limit_parkour_ready = 5;
	s8 fill_bots; // if = 0, no bots. if > 0, total number of desired players including bots is fill_bots + 1
	u8 time_limit_minutes[s32(GameType::count)] = { DEFAULT_ASSAULT_TIME_LIMIT_MINUTES, 10, 10, }; // Assault, Deathmatch, CaptureTheFlag
	char name[MAX_SERVER_CONFIG_NAME + 1];
	char secret[MAX_SERVER_CONFIG_SECRET + 1];
	b8 is_private;

	r32 time_limit() const
	{
		return 60.0f * r32(time_limit_minutes[s32(game_type)]);
	}
};

template<typename Stream> b8 serialize_server_config(Stream* p, ServerConfig* c)
{
	serialize_u32(p, c->id);
	if (c->id != 0) // 0 = story mode
	{
		serialize_bool(p, c->ruleset.enable_batteries);
		serialize_bool(p, c->ruleset.enable_battery_stealth);
		serialize_int(p, s8, c->ruleset.spawn_delay, 0, 120);
		serialize_s16(p, c->ruleset.upgrades_allow);
		serialize_s16(p, c->ruleset.upgrades_default);
		serialize_int(p, u16, c->ruleset.start_abilities.length, 0, c->ruleset.start_abilities.capacity());
		for (s32 i = 0; i < c->ruleset.start_abilities.length; i++)
			serialize_enum(p, Ability, c->ruleset.start_abilities[i]);
		serialize_int(p, s16, c->ruleset.start_energy, 0, MAX_START_ENERGY);
		serialize_int(p, s8, c->ruleset.drone_shield, 0, DRONE_SHIELD_MAX);
		serialize_int(p, u8, c->ruleset.cooldown_speed_index, 1, COOLDOWN_SPEED_MAX_INDEX);

		serialize_enum(p, Ruleset::Preset, c->preset);
		serialize_enum(p, GameType, c->game_type);
		serialize_u8(p, c->time_limit_parkour_ready);
		for (s32 i = 0; i < s32(GameType::count); i++)
			serialize_int(p, u8, c->time_limit_minutes[i], 1, 255);
		serialize_int(p, s16, c->kill_limit, 0, MAX_RESPAWNS);
		serialize_int(p, s16, c->flag_limit, 0, MAX_RESPAWNS);
		for (s32 i = 0; i < c->levels.length; i++)
		{
			if (!LEVEL_ALLOWED(c->levels[i]))
			{
				c->levels.remove(i);
				i--;
			}
		}
		serialize_int(p, u16, c->levels.length, 1, c->levels.capacity());
		for (s32 i = 0; i < c->levels.length; i++)
			serialize_s16(p, c->levels[i]);
		serialize_int(p, s8, c->max_players, 1, MAX_PLAYERS);
		serialize_int(p, s8, c->min_players, 1, MAX_PLAYERS);
		serialize_int(p, s8, c->fill_bots, 0, MAX_PLAYERS - 1);
		if (c->game_type == GameType::Assault || c->game_type == GameType::CaptureTheFlag)
			c->team_count = 2;
		else
			serialize_int(p, s8, c->team_count, 2, MAX_TEAMS);
		c->team_count = vi_min(c->team_count, c->max_players);
		serialize_u32(p, c->creator_id);
		serialize_enum(p, Region, c->region);
		{
			s32 name_length;
			if (Stream::IsWriting)
				name_length = s32(strlen(c->name));
			serialize_int(p, s32, name_length, 0, MAX_SERVER_CONFIG_NAME);
			serialize_bytes(p, (u8*)c->name, name_length);
			if (Stream::IsReading)
				c->name[name_length] = '\0';
		}
		{
			s32 secret_length;
			if (Stream::IsWriting)
				secret_length = s32(strlen(c->secret));
			serialize_int(p, s32, secret_length, 0, MAX_SERVER_CONFIG_SECRET);
			serialize_bytes(p, (u8*)c->secret, secret_length);
			if (Stream::IsReading)
				c->secret[secret_length] = '\0';
		}
		serialize_bool(p, c->is_private);
	}
	return true;
}

struct ServerDetails
{
	ServerConfig config;
	ServerState state;
	Sock::Address addr;
	char creator_username[MAX_USERNAME + 1];
	b8 is_admin;
};

template<typename Stream> b8 serialize_server_details(Stream* p, ServerDetails* d)
{
	if (!serialize_server_config(p, &d->config))
		net_error();

	if (!serialize_server_state(p, &d->state))
		net_error();

	if (d->state.level != AssetNull)
	{
		if (!Sock::Address::serialize(p, &d->addr))
			net_error();
	}

	{
		s32 creator_username_length;
		if (Stream::IsWriting)
			creator_username_length = s32(strlen(d->creator_username));
		serialize_int(p, s32, creator_username_length, 0, MAX_USERNAME);
		serialize_bytes(p, (u8*)d->creator_username, creator_username_length);
		if (Stream::IsReading)
			d->creator_username[creator_username_length] = '\0';
	}

	serialize_bool(p, d->is_admin);

	return true;
}


}

}

}
