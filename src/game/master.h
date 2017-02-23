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

struct CollectibleEntry
{
	AssetID zone;
	ID id;
};

enum class Group
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
	char username[MAX_USERNAME + 1];
	b8 zone_current_restore;
	b8 locke_spoken;

	Save();
	void reset();
};

enum class Message
{
	Ack,
	Ping, // client checking if master is present
	ServerStatusUpdate, // game server telling master what it's up to
	ServerLoad, // master telling a server to load a certain level
	ClientConnect, // master telling a client to connect to a game server
	ClientRequestServer, // a client requesting the master to allocate it a game server
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
	SessionType session_type;
	GameType game_type;
	AssetID level;
	s16 kill_limit;
	s16 respawns;
	s8 open_slots; // for servers, this is the number of open player slots. for clients, this is the number of players the client has locally
	s8 team_count;
	u8 time_limit_minutes;
	b8 equals(const ServerState&) const;
	void make_story();
};

template<typename Stream> b8 serialize_server_state(Stream* p, ServerState* s)
{
	serialize_enum(p, GameType, s->game_type);
	serialize_enum(p, SessionType, s->session_type);
	serialize_s16(p, s->level);
	serialize_int(p, s8, s->open_slots, 0, MAX_PLAYERS);
	serialize_int(p, s8, s->team_count, 2, MAX_PLAYERS);
	serialize_s16(p, s->respawns);
	serialize_s16(p, s->kill_limit);
	serialize_u8(p, s->time_limit_minutes);
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
	s32 username_length;
	if (Stream::IsWriting)
		username_length = strlen(s->username);
	serialize_int(p, s32, username_length, 0, MAX_USERNAME);
	serialize_bytes(p, (u8*)s->username, username_length);
	if (Stream::IsReading)
		s->username[username_length] = '\0';
	serialize_bool(p, s->zone_current_restore);
	serialize_bool(p, s->locke_spoken);
	return true;
}


}

}

}