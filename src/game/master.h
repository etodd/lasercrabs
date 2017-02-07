#pragma once
#include "types.h"
#include "net_serialize.h"
#include "platform/sock.h"
#include <unordered_map>
#include "constants.h"

namespace VI
{

namespace Net
{

namespace Master
{


enum class Message
{
	Ack,
	ServerStatusUpdate, // game server telling master what it's up to
	ServerLoad, // master telling a server to load a certain level
	ClientConnect, // master telling a client to connect to a game server
	ClientRequestServer, // a client requesting the master to allocate it a game server
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
	AssetID level;
	b8 story_mode;
	s8 open_slots; // for servers, this is the number of open player slots. for clients, this is the number of players the client has locally
	s8 team_count;
	b8 equals(const ServerState&) const;
};

template<typename Stream> b8 serialize_server_state(Stream* p, ServerState* s)
{
	serialize_s16(p, s->level);
	serialize_bool(p, s->story_mode);
	serialize_int(p, s8, s->open_slots, 0, MAX_PLAYERS);
	serialize_int(p, s8, s->team_count, 2, MAX_PLAYERS);
	return true;
}


}

}

}