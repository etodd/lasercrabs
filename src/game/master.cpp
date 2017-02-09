#include "master.h"
#include "asset/level.h"

namespace VI
{

namespace Net
{

namespace Master
{

#define DEBUG_MSG 0
#define NET_MASTER_RESEND_INTERVAL 1.5

Save::Save()
{
	reset();
}

void Save::reset()
{
	this->~Save();

	memset(this, 0, sizeof(*this));

	zone_last = AssetNull;
	zone_current = AssetNull;
	zone_overworld = AssetNull;
	locke_index = -1;

	strcpy(username, "etodd");
	zones[Asset::Level::Dock] = ZoneState::GroupOwned;
	zones[Asset::Level::Qualia] = ZoneState::GroupOwned;

	resources[(s32)Resource::Energy] = (s16)(CREDITS_INITIAL * 3.5f);
}


Messenger::Peer::Peer()
	: incoming_seq(NET_SEQUENCE_COUNT - 1),
	outgoing_seq(0)
{

}

b8 ServerState::equals(const ServerState& s) const
{
	return level == s.level
		&& story_mode == s.story_mode
		&& open_slots == s.open_slots
		&& team_count == s.team_count;
}

SequenceID Messenger::outgoing_sequence_id(Sock::Address addr) const
{
	auto i = sequence_ids.find(addr);
	if (i == sequence_ids.end())
		return 0;
	else
		return i->second.outgoing_seq;
}

b8 Messenger::has_unacked_outgoing_messages(Sock::Address addr) const
{
	for (s32 i = 0; i < outgoing.length; i++)
	{
		if (outgoing[i].addr.equals(addr))
			return true;
	}
	return false;
}

b8 Messenger::add_header(StreamWrite* p, Sock::Address addr, Message type)
{
	using Stream = StreamWrite;
	SequenceID seq = outgoing_sequence_id(addr);
	serialize_int(p, SequenceID, seq, 0, NET_SEQUENCE_COUNT - 1);
#if DEBUG_MSG
	vi_debug("Sending seq %d message %d to %s:%hd", s32(seq), s32(type), Sock::host_to_str(addr.host), addr.port);
#endif
	serialize_enum(p, Message, type);
	return true;
}

void Messenger::send(const StreamWrite& p, r64 timestamp, Sock::Address addr, Sock::Handle* sock)
{
	SequenceID seq = outgoing_sequence_id(addr);

	OutgoingPacket* packet = outgoing.add();
	packet->data = p;
	packet->sequence_id = seq;
	packet->timestamp = timestamp;
	packet->addr = addr;

	SequenceID seq_next = sequence_advance(seq, 1);
	{
		auto i = sequence_ids.find(addr);
		if (i == sequence_ids.end())
		{
			// haven't sent a message to this address yet
			Peer peer;
			peer.outgoing_seq = seq_next;
			sequence_ids[addr] = peer;
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
	serialize_int(&p, SequenceID, seq, 0, NET_SEQUENCE_COUNT - 1);
	{
		Message ack_type = Message::Ack;
		serialize_enum(&p, Message, ack_type);
	}
	packet_finalize(&p);
	Sock::udp_send(sock, addr, p.data.data, p.bytes_written());
	return true;
}

// returns true if the packet is in order and should be processed
b8 Messenger::received(Message type, SequenceID seq, Sock::Address addr, Sock::Handle* sock)
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
		return false; // ignore this packet
	}
	else
	{
		// when we receive any kind of message other than an ack, we must send an ack back

		messenger_send_ack(seq, addr, sock);

		// check if sequence is more recent
		auto i = sequence_ids.find(addr);
		if (i == sequence_ids.end())
		{
			// haven't received a message from this address yet
			Peer peer;
			peer.incoming_seq = seq;
			sequence_ids[addr] = peer;
		}
		else
		{
			// compare against previous messages
			if (sequence_more_recent(seq, i->second.incoming_seq))
				i->second.incoming_seq = seq; // update most recent sequence
			else
			{
				// packet is out of order
#if DEBUG_MSG
				vi_debug("Discarding out-of-order seq %d message %d from %s:%hd", s32(seq), s32(type), Sock::host_to_str(addr.host), addr.port);
#endif
				return false;
			}
		}

#if DEBUG_MSG
		vi_debug("Received seq %d message %d from %s:%hd", s32(seq), s32(type), Sock::host_to_str(addr.host), addr.port);
#endif

	}
	return true; // packet is in order; process it
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
				vi_debug("Resending seq %d to %s:%hd", s32(packet->sequence_id), Sock::host_to_str(packet->addr.host), packet->addr.port);
#endif
				packet->timestamp = timestamp;
				Sock::udp_send(sock, packet->addr, packet->data.data.data, packet->data.bytes_written());
			}
		}
	}
}

void Messenger::reset()
{
	outgoing.length = 0;
	sequence_ids.clear();
}

void Messenger::remove(Sock::Address addr)
{
	for (s32 i = 0; i < outgoing.length; i++)
	{
		if (outgoing[i].addr.equals(addr))
		{
			outgoing.remove(i);
			i--;
		}
	}
	sequence_ids.erase(addr);
}

}

}

}