#include "master.h"

namespace VI
{

namespace Net
{

namespace Master
{

#define DEBUG_MSG 1
#define NET_MASTER_RESEND_INTERVAL 1.5

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
	serialize_enum(p, Message, type);
#if DEBUG_MSG
	vi_debug("Sending seq %d message %d to %s:%hd", s32(seq), s32(type), Sock::host_to_str(addr.host), addr.port);
#endif
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
	send_unreliable(p, addr, sock);
}

void Messenger::send_unreliable(const StreamWrite& p, Sock::Address addr, Sock::Handle* sock)
{
	SequenceID seq = sequence_advance(outgoing_sequence_id(addr), 1);
	{
		auto i = sequence_ids.find(addr);
		if (i == sequence_ids.end())
			sequence_ids[addr] = { 0, seq };
		else
			i->second.outgoing_seq = seq;
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

#if DEBUG_MSG
		vi_debug("Received seq %d message %d from %s:%hd", s32(seq), s32(type), Sock::host_to_str(addr.host), addr.port);
#endif

		// check if sequence is more recent
		{
			auto i = sequence_ids.find(addr);
			if (i == sequence_ids.end())
				sequence_ids[addr] = { seq, 0 }; // never received a message from this peer before
			else
			{
				// compare against previous messages
				if (sequence_more_recent(seq, i->second.incoming_seq))
					i->second.incoming_seq = seq; // update most recent sequence
				else
					return false; // packet is out of order
			}
		}

		messenger_send_ack(seq, addr, sock);
	}
	return true; // packet is in order; process it
}

void Messenger::update(r64 timestamp, Sock::Handle* sock)
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

void Messenger::reset()
{
	outgoing.length = 0;
	sequence_ids.clear();
}

void Messenger::remove(Sock::Address addr)
{
	sequence_ids.erase(addr);
}

}

}

}