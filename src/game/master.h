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

enum class Role : s8
{
	None,
	Banned,
	Allowed,
	Admin,
	count,
};

// type of authentication used to obtain a user key from the master server
enum class AuthType : s8
{
	None,
	Itch,
	Steam,
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
	ClientConnect, // master telling a client to connect to a game server
	ClientRequestServer, // a client requesting to connect to a virtual server; master will allocate it if necessary
	ClientRequestServerDetails, // a client requesting to connect to a virtual server; master will allocate it if necessary
	RequestServerConfig, // a client or server requesting ServerConfig data from the master
	ClientRequestServerList, // a client requesting a server list from the master
	ServerList, // master responding to a client with a server list
	ClientSaveServerConfig, // a client telling the master server to create or update a server config
	ServerConfig, // master responding to a client or server with ServerConfig data
	ServerDetails, // master responding to a client with ServerDetails data
	ServerConfigSaved, // master telling a client their config was created
	WrongVersion, // master telling a server or client that it needs to upgrade
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

	// these assume packets have already been checksummed and compressed
	void send(const StreamWrite&, r64, const Sock::Address&, Sock::Handle*);
	void received(Message, SequenceID, const Sock::Address&, Sock::Handle*);
};

struct ServerState // represents the current state of a game server
{
	u32 id; // the virtual server configuration currently active on this game server; 0 if it's story mode
	AssetID level;
	s8 player_slots; // for servers, this is the number of open player slots. for clients, this is the number of players the client has locally
	Region region;
};

template<typename Stream> b8 serialize_server_state(Stream* p, ServerState* s)
{
	serialize_u32(p, s->id);
	serialize_s16(p, s->level);
	serialize_int(p, s8, s->player_slots, 0, MAX_PLAYERS);
	serialize_enum(p, Region, s->region);
	return true;
}

#define MAX_SERVER_CONFIG_NAME 127
#define MAX_SERVER_LIST 8

struct ServerListEntry
{
	ServerState server_state;
	Sock::Address addr;
	char name[MAX_SERVER_CONFIG_NAME + 1];
	char creator_username[MAX_USERNAME + 1];
	s8 max_players;
	s8 team_count;
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
			name_length = strlen((const char*)s->name);
		serialize_int(p, s32, name_length, 0, MAX_SERVER_CONFIG_NAME);
		serialize_bytes(p, (u8*)s->name, name_length);
		if (Stream::IsReading)
			s->name[name_length] = '\0';
	}

	{
		s32 creator_length;
		if (Stream::IsWriting)
			creator_length = strlen((const char*)s->creator_username);
		serialize_int(p, s32, creator_length, 0, MAX_USERNAME);
		serialize_bytes(p, (u8*)s->creator_username, creator_length);
		if (Stream::IsReading)
			s->creator_username[creator_length] = '\0';
	}

	serialize_enum(p, GameType, s->game_type);
	serialize_int(p, s8, s->max_players, 2, MAX_PLAYERS);
	serialize_int(p, s8, s->team_count, 2, MAX_TEAMS);

	return true;
}

struct ServerConfig
{
	u32 id;
	u32 creator_id;
	StaticArray<AssetID, 32> levels;
	s16 kill_limit = DEFAULT_ASSAULT_DRONES;
	s16 respawns = DEFAULT_ASSAULT_DRONES;
	s16 allow_upgrades = s16((1 << s32(Upgrade::count)) - 1);
	s16 start_energy = ENERGY_INITIAL;
	GameType game_type = GameType::Assault;
	StaticArray<Upgrade, 3> start_upgrades;
	s8 max_players = 1;
	s8 min_players = 1;
	s8 team_count = 2;
	s8 drone_shield = DRONE_SHIELD_AMOUNT;
	Region region;
	u8 time_limit_minutes = 10;
	char name[MAX_SERVER_CONFIG_NAME + 1];
	b8 enable_minions = true;
	b8 enable_batteries = true;
	b8 enable_battery_stealth = true;
	b8 is_private;
	b8 fill_bots;

	r32 time_limit() const
	{
		return r32(time_limit_minutes) * 60.0f;
	}
};

template<typename Stream> b8 serialize_server_config(Stream* p, ServerConfig* c)
{
	serialize_u32(p, c->id);
	if (c->id != 0) // 0 = story mode
	{
		serialize_enum(p, GameType, c->game_type);
		serialize_int(p, u16, c->levels.length, 1, c->levels.capacity());
		for (s32 i = 0; i < c->levels.length; i++)
			serialize_s16(p, c->levels[i]);
		serialize_int(p, s8, c->max_players, 1, MAX_PLAYERS);
		serialize_int(p, s8, c->min_players, 1, MAX_PLAYERS);
		if (c->game_type == GameType::Assault)
			c->team_count = 2;
		else
			serialize_int(p, s8, c->team_count, 2, MAX_TEAMS);
		c->team_count = vi_min(c->team_count, c->max_players);
		serialize_int(p, s16, c->respawns, 1, 1000);
		serialize_u32(p, c->creator_id);
		serialize_int(p, s16, c->kill_limit, 0, 1000);
		serialize_s16(p, c->allow_upgrades);
#if SERVER
		// disallow extra drone upgrade without actually storing that preference in the master server database
		if (Stream::IsReading && c->game_type == GameType::Deathmatch)
			c->allow_upgrades &= ~(1 << s32(Upgrade::ExtraDrone)); // can't purchase extra drones when you have infinite drones
#endif
		serialize_int(p, s16, c->start_energy, 0, MAX_START_ENERGY);
		serialize_int(p, s8, c->drone_shield, 0, DRONE_SHIELD_AMOUNT);
		serialize_enum(p, Region, c->region);
		serialize_int(p, u16, c->start_upgrades.length, 0, c->start_upgrades.capacity());
		for (s32 i = 0; i < c->start_upgrades.length; i++)
			serialize_enum(p, Upgrade, c->start_upgrades[i]);
		serialize_int(p, u8, c->time_limit_minutes, 1, 254);
		s32 name_length;
		if (Stream::IsWriting)
			name_length = strlen(c->name);
		serialize_int(p, s32, name_length, 0, MAX_SERVER_CONFIG_NAME);
		serialize_bytes(p, (u8*)c->name, name_length);
		if (Stream::IsReading)
			c->name[name_length] = '\0';
		serialize_bool(p, c->enable_minions);
		serialize_bool(p, c->enable_batteries);
		serialize_bool(p, c->enable_battery_stealth);
		serialize_bool(p, c->is_private);
		serialize_bool(p, c->fill_bots);
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
			creator_username_length = strlen(d->creator_username);
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