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

// type of authentication used to obtain a user key from the master server
enum class AuthType : s8
{
	None,
	Itch,
	Steam,
	count,
};

struct CollectibleEntry
{
	AssetID zone;
	ID id;
};

enum class Group : s8
{
	None,
	WuGang,
	Ephyra,
	count,
};

struct Save
{
	r64 timestamp;
	r64 zone_lost_times[MAX_ZONES];
	Array<CollectibleEntry> collectibles;
	Vec3 zone_current_restore_position;
	r32 zone_current_restore_rotation;
	s32 locke_index;
	ZoneState zones[MAX_ZONES];
	Group group;
	s16 resources[s32(Resource::count)];
	AssetID zone_last;
	AssetID zone_current;
	AssetID zone_overworld;
	b8 zone_current_restore;
	b8 locke_spoken;
	b8 extended_parkour;

	Save();
	void reset();
};

enum class Message : s8
{
	Ack,
	Auth, // client logging in to master
	AuthResponse, // master responding to client with auth info
	ServerStatusUpdate, // game server telling master what it's up to
	ServerLoad, // master telling a server to load a certain level
	ExpectClient, // master telling a server to expect a certain client to connect to it
	ClientConnect, // master telling a client to connect to a game server
	ClientRequestServer, // a client requesting to connect to a virtual server; master will allocate it if necessary
	ClientRequestServerList, // a client requesting a server list from the master
	ServerList, // master responding to a client with a server list
	ClientCreateServerConfig, // a client telling the master server to create a new server config
	ServerConfigCreated, // master telling a client their config was created
	WrongVersion, // master telling a server or client that it needs to upgrade
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

	Array<OutgoingPacket> outgoing; // unordered, unacked messages
	std::unordered_map<Sock::Address, Peer> sequence_ids;

	SequenceID outgoing_sequence_id(Sock::Address) const;
	b8 has_unacked_outgoing_messages(Sock::Address) const;

	b8 add_header(StreamWrite*, Sock::Address, Message);
	void update(r64, Sock::Handle*, s32 = 0);
	void remove(Sock::Address);
	void reset();

	// these assume packets have already been checksummed and compressed
	void send(const StreamWrite&, r64, Sock::Address, Sock::Handle*);
	b8 received(Message, SequenceID, Sock::Address, Sock::Handle*); // returns true if the packet is in order and should be processed
};

struct ServerState // represents the current state of a game server
{
	u32 id; // the virtual server configuration currently active on this game server; 0 if it's story mode
	AssetID level;
	s8 player_slots; // for servers, this is the number of open player slots. for clients, this is the number of players the client has locally
};

template<typename Stream> b8 serialize_server_state(Stream* p, ServerState* s)
{
	serialize_u32(p, s->id);
	serialize_s16(p, s->level);
	serialize_int(p, s8, s->player_slots, 0, MAX_PLAYERS);
	return true;
}

#define MAX_SERVER_CONFIG_NAME 127
#define MAX_SERVER_LIST 8

struct ServerConfig
{
	u32 id;
	StaticArray<AssetID, 32> levels;
	s16 kill_limit = DEFAULT_ASSAULT_DRONES;
	s16 respawns = DEFAULT_ASSAULT_DRONES;
	SessionType session_type;
	GameType game_type = GameType::Assault;
	s16 allow_abilities = 0xffff;
	s16 start_abilities;
	s8 max_players = 1;
	s8 team_count = 2;
	u8 time_limit_minutes = 6;
	char name[MAX_SERVER_CONFIG_NAME + 1];
	b8 allow_minions;
	b8 is_private;

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
		serialize_enum(p, SessionType, c->session_type);
		serialize_int(p, u16, c->levels.length, 0, c->levels.capacity());
		for (s32 i = 0; i < c->levels.length; i++)
			serialize_s16(p, c->levels[i]);
		serialize_int(p, s8, c->max_players, 1, MAX_PLAYERS);
		serialize_int(p, s8, c->team_count, 2, MAX_TEAMS);
		serialize_s16(p, c->respawns);
		serialize_s16(p, c->kill_limit);
		serialize_s16(p, c->allow_abilities);
		serialize_s16(p, c->start_abilities);
		serialize_u8(p, c->time_limit_minutes);
		s32 name_length;
		if (Stream::IsWriting)
			name_length = strlen(c->name);
		serialize_int(p, s32, name_length, 0, MAX_SERVER_CONFIG_NAME);
		serialize_bytes(p, (u8*)c->name, name_length);
		if (Stream::IsReading)
			c->name[name_length] = '\0';
		serialize_bool(p, c->allow_minions);
		serialize_bool(p, c->is_private);
	}
	return true;
}

template<typename Stream> b8 serialize_save(Stream* p, Save* s)
{
	serialize_r64(p, s->timestamp);
	for (s32 i = 0; i < MAX_ZONES; i++)
		serialize_r64(p, s->zone_lost_times[i]);
	serialize_s32(p, s->collectibles.length);
	if (Stream::IsReading)
		s->collectibles.resize(s->collectibles.length);
	for (s32 i = 0; i < s->collectibles.length; i++)
	{
		serialize_s16(p, s->collectibles[i].zone);
		serialize_int(p, ID, s->collectibles[i].id, 0, MAX_ENTITIES - 1);
	}
	serialize_r32(p, s->zone_current_restore_position.x);
	serialize_r32(p, s->zone_current_restore_position.y);
	serialize_r32(p, s->zone_current_restore_position.z);
	serialize_r32(p, s->zone_current_restore_rotation);
	serialize_s32(p, s->locke_index);
	for (s32 i = 0; i < MAX_ZONES; i++)
		serialize_enum(p, ZoneState, s->zones[i]);
	serialize_enum(p, Group, s->group);
	for (s32 i = 0; i < s32(Resource::count); i++)
		serialize_s16(p, s->resources[i]);
	serialize_s16(p, s->zone_last);
	serialize_s16(p, s->zone_current);
	serialize_s16(p, s->zone_overworld);
	serialize_bool(p, s->zone_current_restore);
	serialize_bool(p, s->locke_spoken);
	serialize_bool(p, s->extended_parkour);
	return true;
}


}

}

}