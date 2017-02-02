#pragma once
#include "types.h"
#include "net_serialize.h"
#include "platform/sock.h"

namespace VI
{

namespace Net
{

namespace Master
{


struct OutgoingPacket
{
	r64 timestamp;
	StreamWrite data;
	SequenceID sequence_id;
};

enum class Message
{
	Keepalive,
	Ack,
	ServerStatusUpdate, // game server telling master what it's up to
	ServerLoad, // master telling a server to load a certain level
	ClientConnect, // master telling a client to connect to a game server
	ClientRequestServer, // a client requesting the master to allocate it a game server
	count,
};

struct Channel
{
	Array<OutgoingPacket> outgoing; // unordered, unacked messages
	Sock::Address addr;

	SequenceID outgoing_sequence_id() const;
	b8 add_header(StreamWrite*, Message);
	void send(const StreamWrite&, Sock::Handle*); // packet is assumed to have already been checksummed and compressed
	b8 received(Message, SequenceID, Sock::Handle*); // packet is assumed to have already been checksummed and decompressed
	void update(Sock::Handle*);
	void reset();
};

struct ServerState // represents the current state of a game server
{
	AssetID level;
	b8 story_mode;
	s8 open_slots;
};


}

}

}