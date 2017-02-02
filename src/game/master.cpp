#include "master.h"
#include "platform/util.h"

namespace VI
{

namespace Net
{

namespace Master
{

#define DEBUG_MSG 1
#define NET_MASTER_RESEND_INTERVAL 1.5

SequenceID Channel::outgoing_sequence_id() const
{
	if (outgoing.length > 0)
	{
		SequenceID most_recent = outgoing[0].sequence_id;
		for (s32 i = 0; i < outgoing.length; i++)
		{
			if (sequence_more_recent(outgoing[i].sequence_id, most_recent))
				most_recent = outgoing[i].sequence_id;
		}
		return sequence_advance(most_recent, 1);
	}
	else
		return 0;
}

b8 Channel::add_header(StreamWrite* p, Message type)
{
	using Stream = StreamWrite;
	SequenceID seq = outgoing_sequence_id();
	serialize_int(p, SequenceID, seq, 0, NET_SEQUENCE_COUNT - 1);
	serialize_enum(p, Message, type);
#if DEBUG_MSG
	vi_debug("Sending seq %d message %d to %s:%hd", s32(seq), s32(type), Sock::host_to_str(addr.host), addr.port);
#endif
	return true;
}

void Channel::send(const StreamWrite& p, Sock::Handle* sock)
{
	SequenceID seq = outgoing_sequence_id();
	OutgoingPacket* packet = outgoing.add();
	packet->data = p;
	packet->sequence_id = seq;
	packet->timestamp = platform::timestamp();
	Sock::udp_send(sock, addr, p.data.data, p.bytes_written());
}

b8 Channel::received(Message type, SequenceID seq, Sock::Handle* sock)
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
	else
	{
#if DEBUG_MSG
	vi_debug("Received seq %d message %d from %s:%hd", s32(seq), s32(type), Sock::host_to_str(addr.host), addr.port);
#endif
		// when we receive any other kind of message, we must send an ack back
		using Stream = StreamWrite;
		StreamWrite p;
		packet_init(&p);
		add_header(&p, Message::Ack);
		packet_finalize(&p);
		Sock::udp_send(sock, addr, p.data.data, p.bytes_written());
	}
	return true;
}

void Channel::update(Sock::Handle* sock)
{
	r64 timestamp = platform::timestamp();
	r64 timestamp_cutoff = timestamp - NET_MASTER_RESEND_INTERVAL;
	for (s32 i = 0; i < outgoing.length; i++)
	{
		OutgoingPacket* packet = &outgoing[i];
		if (packet->timestamp < timestamp_cutoff)
		{
#if DEBUG_MSG
			vi_debug("Resending seq %d to %s:%hd", s32(packet->sequence_id), Sock::host_to_str(addr.host), addr.port);
#endif
			packet->timestamp = timestamp;
			Sock::udp_send(sock, addr, packet->data.data.data, packet->data.bytes_written());
		}
	}
}

void Channel::reset()
{
	outgoing.length = 0;
}

}

}

}