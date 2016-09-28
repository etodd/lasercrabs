#include "net.h"
#include "net_serialize.h"
#include "platform/sock.h"
#include "game/game.h"
#if SERVER
#include "asset/level.h"
#endif
#include "mersenne/mersenne-twister.h"

namespace VI
{


namespace Net
{

// borrows heavily from https://github.com/networkprotocol/libyojimbo

typedef u8 SequenceID;

#define TIMEOUT 2.0f
#define TICK_RATE (1.0f / 60.0f)
#define SEQUENCE_BITS 8
#define SEQUENCE_COUNT (1 << SEQUENCE_BITS)

#define MESSAGE_BUFFER s32(TIMEOUT / TICK_RATE)
#define MAX_MESSAGES_SIZE (MAX_PACKET_SIZE / 2)

enum class ClientPacket
{
	Connect,
	Update,
	AckInit,
	count,
};

enum class ServerPacket
{
	Init,
	Keepalive,
	Update,
	count,
};

enum class MessageType
{
	Noop,
	EntitySpawn,
	EntityRemove,
	count,
};

struct MessageFrame // container for the amount of messages that can come in a single frame
{
	union
	{
		StreamRead read;
		StreamWrite write;
	};
	r32 timestamp;
	SequenceID sequence_id;

	MessageFrame(r32 t) : read(), sequence_id(), timestamp(t) {}
	~MessageFrame() {}
};

struct Ack
{
	u32 previous_sequences;
	SequenceID sequence_id;
};

struct MessageHistory
{
	StaticArray<MessageFrame, 256> msgs;
	s32 current_index;
};

#define SEQUENCE_RESEND_BUFFER 6
struct SequenceHistoryEntry
{
	r32 timestamp;
	SequenceID id;
};

typedef StaticArray<SequenceHistoryEntry, SEQUENCE_RESEND_BUFFER> SequenceHistory;

// true if s1 > s2
b8 sequence_more_recent(SequenceID s1, SequenceID s2)
{
	return ((s1 > s2) && (s1 - s2 <= SEQUENCE_COUNT / 2))
		|| ((s2 > s1) && (s2 - s1 > SEQUENCE_COUNT / 2));
}

s32 sequence_relative_to(SequenceID s1, SequenceID s2)
{
	if (sequence_more_recent(s1, s2))
	{
		if (s1 < s2)
			return (s32(s1) + SEQUENCE_COUNT) - s32(s2);
		else
			return s32(s1) - s32(s2);
	}
	else
	{
		if (s1 < s2)
			return s32(s1) - s32(s2);
		else
			return s32(s1) - (s32(s2) + SEQUENCE_COUNT);
	}
}

SequenceID sequence_advance(SequenceID start, s32 delta)
{
	s32 result = s32(start) + delta;
	while (result < 0)
		result += SEQUENCE_COUNT;
	while (result >= SEQUENCE_COUNT)
		result -= SEQUENCE_COUNT;
	return SequenceID(result);
}

#if DEBUG
void ack_debug(const char* caption, const Ack& ack)
{
	char str[33] = {};
	for (s32 i = 0; i < 32; i++)
		str[i] = (ack.previous_sequences & (1 << i)) ? '1' : '0';
	vi_debug("%s %d %s", caption, s32(ack.sequence_id), str);
}

void msg_history_debug(const MessageHistory& history)
{
	if (history.msgs.length > 0)
	{
		s32 index = history.current_index;
		for (s32 i = 0; i < 64; i++)
		{
			const MessageFrame& msg = history.msgs[index];
			vi_debug("%d", s32(msg.sequence_id));

			// loop backward through most recently received frames
			index = index > 0 ? index - 1 : history.msgs.length - 1;
			if (index == history.current_index) // we looped all the way around
				break;
		}
	}
}
#endif

MessageFrame* msg_history_add(MessageHistory* history, r32 timestamp)
{
	MessageFrame* frame;
	if (history->msgs.length < history->msgs.capacity())
	{
		frame = history->msgs.add();
		history->current_index = history->msgs.length - 1;
	}
	else
	{
		history->current_index = (history->current_index + 1) % history->msgs.capacity();
		frame = &history->msgs[history->current_index];
	}
	new (frame) MessageFrame(timestamp);
	return frame;
}

Ack msg_history_ack(const MessageHistory& history)
{
	Ack ack = {};
	if (history.msgs.length > 0)
	{
		s32 index = history.current_index;
		// find most recent sequence ID
		ack.sequence_id = history.msgs[index].sequence_id;
		for (s32 i = 0; i < 64; i++)
		{
			// loop backward through most recently received frames
			index = index > 0 ? index - 1 : history.msgs.length - 1;
			if (index == history.current_index) // we looped all the way around
				break;

			const MessageFrame& msg = history.msgs[index];
			if (sequence_more_recent(msg.sequence_id, ack.sequence_id))
				ack.sequence_id = msg.sequence_id;
		}

		index = history.current_index;
		for (s32 i = 0; i < 64; i++)
		{
			// loop backward through most recently received frames
			index = index > 0 ? index - 1 : history.msgs.length - 1;
			if (index == history.current_index) // we looped all the way around
				break;

			const MessageFrame& msg = history.msgs[index];
			if (msg.sequence_id != ack.sequence_id) // ignore the ack
			{
				s32 sequence_id_relative_to_most_recent = sequence_relative_to(msg.sequence_id, ack.sequence_id);
				vi_assert(sequence_id_relative_to_most_recent < 0);
				if (sequence_id_relative_to_most_recent >= -32)
					ack.previous_sequences |= 1 << (-sequence_id_relative_to_most_recent - 1);
			}
		}
	}
	return ack;
}

MessageHistory msgs_out_history;
StaticArray<StreamWrite, MESSAGE_BUFFER> msgs_out;
StreamRead incoming_packet;
r32 tick_timer;
Sock::Handle sock;
SequenceID local_sequence_id = 1;

void packet_init(StreamWrite* p)
{
	p->bits(NET_PROTOCOL_ID, 32); // packet_send() will replace this with the packet checksum
}

void packet_finalize(StreamWrite* p)
{
	vi_assert(p->data[0] == NET_PROTOCOL_ID);
	p->flush();
	u32 checksum = crc32((const u8*)&p->data[0], sizeof(u32));
	checksum = crc32((const u8*)&p->data[1], (p->data.length - 1) * sizeof(u32), checksum);
	p->data[0] = checksum;
}

void packet_send(const StreamWrite& p, const Sock::Address& address)
{
	Sock::udp_send(&sock, address, p.data.data, p.data.length * sizeof(u32));
}

b8 msg_send_noop()
{
	using Stream = StreamWrite;

	StreamWrite* p = msg_new();
	MessageType type = MessageType::Noop;
	serialize_enum(p, MessageType, type);
	return true;
}

// consolidate msgs_out into msgs_out_history
void msgs_out_consolidate()
{
	using Stream = StreamWrite;

	if (msgs_out.length == 0)
		msg_send_noop(); // we have to send SOMETHING every sequence

	s32 bytes = 0;
	s32 msgs = 0;
	for (s32 i = 0; i < msgs_out.length; i++)
	{
		s32 msg_bytes = msgs_out[i].bytes_written();
		if (64 + bytes + msg_bytes > MAX_MESSAGES_SIZE)
			break;
		bytes += msg_bytes;
		msgs++;
	}

	MessageFrame* frame = msg_history_add(&msgs_out_history, Game::real_time.total);

	frame->sequence_id = local_sequence_id;

	serialize_int(&frame->write, s32, bytes, 0, MAX_MESSAGES_SIZE); // message frame size
	if (bytes > 0)
	{
		serialize_int(&frame->write, SequenceID, frame->sequence_id, 0, SEQUENCE_COUNT - 1);
		for (s32 i = 0; i < msgs; i++)
			serialize_bytes(&frame->write, (u8*)msgs_out[i].data.data, msgs_out[i].bytes_written());
	}
	
	for (s32 i = msgs - 1; i >= 0; i--)
		msgs_out.remove_ordered(i);
}

const MessageFrame* msg_frame_advance(const MessageHistory& history, SequenceID* id)
{
	if (history.msgs.length > 0)
	{
		s32 index = history.current_index;
		SequenceID next_sequence = sequence_advance(*id, 1);
		for (s32 i = 0; i < 64; i++)
		{
			const MessageFrame& msg = history.msgs[index];
			if (msg.sequence_id == next_sequence)
			{
				*id = next_sequence;
				return &msg;
			}

			// loop backward through most recently received frames
			index = index > 0 ? index - 1 : history.msgs.length - 1;
			if (index == history.current_index) // we looped all the way around
				break;
		}
	}
	return nullptr;
}

b8 ack_get(const Ack& ack, SequenceID sequence_id)
{
	if (sequence_more_recent(sequence_id, ack.sequence_id))
		return false;
	else if (sequence_id == ack.sequence_id)
		return true;
	else
	{
		s32 relative = sequence_relative_to(sequence_id, ack.sequence_id);
		vi_assert(relative < 0);
		if (relative < -32)
			return false;
		else
			return ack.previous_sequences & (1 << (-relative - 1));
	}
}

void sequence_history_add(SequenceHistory* history, SequenceID id, r32 timestamp)
{
	if (history->length == history->capacity())
		history->remove(history->length - 1);
	*history->insert(0) = { timestamp, id };
}

b8 sequence_history_contains_newer_than(const SequenceHistory& history, SequenceID id, r32 timestamp_cutoff)
{
	for (s32 i = 0; i < history.length; i++)
	{
		const SequenceHistoryEntry& entry = history[i];
		if (entry.id == id && entry.timestamp > timestamp_cutoff)
			return true;
	}
	return false;
}

b8 msgs_write(StreamWrite* p, const MessageHistory& history, const Ack& remote_ack, SequenceHistory* recently_resent, r32 rtt)
{
	using Stream = StreamWrite;
	s32 bytes = 0;

	if (history.msgs.length > 0)
	{
		// resend previous frames
		{
			// rewind to 64 frames previous
			s32 index = history.current_index;
			for (s32 i = 0; i < 64; i++)
			{
				s32 next_index = index > 0 ? index - 1 : history.msgs.length - 1;
				if (next_index == history.current_index)
					break;
				index = next_index;
			}

			// start resending frames starting at that index
			r32 timestamp_cutoff = Game::real_time.total - (rtt * 3.0f); // wait a certain period before trying to resend a sequence
			for (s32 i = 0; i < 64 && index != history.current_index; i++)
			{
				const MessageFrame& frame = history.msgs[index];
				s32 relative_sequence = sequence_relative_to(frame.sequence_id, remote_ack.sequence_id);
				if (relative_sequence < 0
					&& relative_sequence >= -32
					&& !ack_get(remote_ack, frame.sequence_id)
					&& !sequence_history_contains_newer_than(*recently_resent, frame.sequence_id, timestamp_cutoff)
					&& bytes + frame.write.bytes_written() <= MAX_MESSAGES_SIZE)
				{
					vi_debug("Resending sequence %d", s32(frame.sequence_id));
					bytes += frame.write.bytes_written();
					serialize_bytes(p, (u8*)frame.write.data.data, frame.write.bytes_written());
					sequence_history_add(recently_resent, frame.sequence_id, Game::real_time.total);
				}

				index = index < history.msgs.length - 1 ? index + 1 : 0;
			}
		}

		// current frame
		{
			const MessageFrame& frame = history.msgs[history.current_index];
			if (bytes + frame.write.bytes_written() <= MAX_MESSAGES_SIZE)
				serialize_bytes(p, (u8*)frame.write.data.data, frame.write.bytes_written());
		}
	}

	bytes = 0;
	serialize_int(p, s32, bytes, 0, MAX_MESSAGES_SIZE); // zero sized frame marks end of message frames

	return true;
}

r32 calculate_rtt(r32 timestamp, const Ack& ack, const MessageHistory& send_history)
{
	if (send_history.msgs.length > 0)
	{
		s32 index = send_history.current_index;
		for (s32 i = 0; i < 64; i++)
		{
			const MessageFrame& msg = send_history.msgs[index];
			if (msg.sequence_id == ack.sequence_id)
				return timestamp - msg.timestamp;
			index = index > 0 ? index - 1 : send_history.msgs.length - 1;
			if (index == send_history.current_index)
				break;
		}
	}
	return -1.0f;
}

b8 msgs_read(StreamRead* p, MessageHistory* history, Ack* ack)
{
	using Stream = StreamRead;

	serialize_int(p, SequenceID, ack->sequence_id, 0, SEQUENCE_COUNT - 1);
	serialize_u32(p, ack->previous_sequences);

	while (true)
	{
		serialize_align(p);
		s32 bytes;
		serialize_int(p, s32, bytes, 0, MAX_MESSAGES_SIZE);
		if (bytes)
		{
			MessageFrame* frame = msg_history_add(history, Game::real_time.total);
			frame->read.data.length = bytes / sizeof(u32);

			serialize_int(p, SequenceID, frame->sequence_id, 0, SEQUENCE_COUNT - 1);
			serialize_bytes(p, (u8*)frame->read.data.data, bytes);
		}
		else
			break;
	}

	return true;
}

#if SERVER

namespace Server
{

struct LocalPlayer;

struct Client
{
	Sock::Address address;
	r32 timeout;
	r32 rtt;
	Ack ack = { u32(-1), 0 }; // most recent ack we've received from the client
	MessageHistory msgs_in_history; // messages we've received from the client
	SequenceHistory recently_resent; // sequences we resent to the client recently
	SequenceID processed_sequence_id; // most recent sequence ID we've processed from the client
	Ref<LocalPlayer> player;
	b8 connected;
};

enum Mode
{
	Waiting,
	Active,
	count,
};

Array<Client> clients;
r32 tick_timer;
Mode mode;
s32 expected_clients = 1;

s32 connected_clients()
{
	s32 result = 0;
	for (s32 i = 0; i < clients.length; i++)
	{
		if (clients[i].connected)
			result++;
	}
	return result;
}

b8 init()
{
	if (Sock::udp_open(&sock, 3494, true))
	{
		printf("%s\n", Sock::get_error());
		return false;
	}

	Game::session.multiplayer = true;
	Game::schedule_load_level(Asset::Level::Ponos, Game::Mode::Pvp);

	return true;
}

b8 build_packet_init(StreamWrite* p)
{
	packet_init(p);
	using Stream = StreamWrite;
	ServerPacket type = ServerPacket::Init;
	serialize_enum(p, ServerPacket, type);
	packet_finalize(p);
	return true;
}

b8 build_packet_keepalive(StreamWrite* p)
{
	packet_init(p);
	using Stream = StreamWrite;
	ServerPacket type = ServerPacket::Keepalive;
	serialize_enum(p, ServerPacket, type);
	packet_finalize(p);
	return true;
}

b8 build_packet_update(StreamWrite* p, Client* client)
{
	packet_init(p);
	using Stream = StreamWrite;
	ServerPacket type = ServerPacket::Update;
	serialize_enum(p, ServerPacket, type);
	Ack ack = msg_history_ack(client->msgs_in_history);
	serialize_int(p, SequenceID, ack.sequence_id, 0, SEQUENCE_COUNT - 1);
	serialize_u32(p, ack.previous_sequences);
	msgs_write(p, msgs_out_history, client->ack, &client->recently_resent, client->rtt);
	packet_finalize(p);
	return true;
}

void update(const Update& u)
{
	for (s32 i = 0; i < clients.length; i++)
	{
		Client* client = &clients[i];
		if (client->connected)
		{
			while (const MessageFrame* frame = msg_frame_advance(client->msgs_in_history, &client->processed_sequence_id))
			{
				// todo: process messages in frame
			}
		}
	}
}

void tick(const Update& u)
{
	if (mode == Mode::Active)
		msgs_out_consolidate();
	StreamWrite p;
	for (s32 i = 0; i < clients.length; i++)
	{
		Client* client = &clients[i];
		client->timeout += Game::real_time.delta;
		if (client->timeout > TIMEOUT)
		{
			vi_debug("Client %s:%hd timed out.", Sock::host_to_str(client->address.host), client->address.port);
			clients.remove(i);
			i--;
		}
		else if (client->connected)
		{
			p.reset();
			build_packet_update(&p, client);
			packet_send(p, clients[i].address);
		}
	}

	if (mode == Mode::Active)
	{
		local_sequence_id++;
		if (local_sequence_id == SEQUENCE_COUNT)
			local_sequence_id = 0;
	}
}

b8 packet_handle(StreamRead* p, const Sock::Address& address)
{
	Client* client = nullptr;
	for (s32 i = 0; i < clients.length; i++)
	{
		if (address.equals(clients[i].address))
		{
			client = &clients[i];
			break;
		}
	}

	using Stream = StreamRead;
	if (!p->read_checksum())
	{
		vi_debug("Discarding packet for invalid checksum.");
		return false;
	}

	ClientPacket type;
	serialize_enum(p, ClientPacket, type);

	switch (type)
	{
		case ClientPacket::Connect:
		{
			if (clients.length < expected_clients)
			{
				Client* client = nullptr;
				for (s32 i = 0; i < clients.length; i++)
				{
					if (clients[i].address.equals(address))
					{
						client = &clients[i];
						break;
					}
				}

				if (!client)
				{
					client = clients.add();
					new (client) Client();
					client->address = address;
				}

				{
					StreamWrite p;
					build_packet_init(&p);
					packet_send(p, address);
				}
			}
			break;
		}
		case ClientPacket::AckInit:
		{
			Client* client = nullptr;
			for (s32 i = 0; i < clients.length; i++)
			{
				if (clients[i].address.equals(address))
				{
					client = &clients[i];
					break;
				}
			}

			if (client && !client->connected)
			{
				vi_debug("Client %s:%hd connected.", Sock::host_to_str(address.host), address.port);
				client->connected = true;
			}

			if (connected_clients() == expected_clients)
				mode = Mode::Active;
			break;
		}
		case ClientPacket::Update:
		{
			if (!client)
			{
				vi_debug("Discarding packet from unknown client.");
				return false;
			}

			if (!msgs_read(p, &client->msgs_in_history, &client->ack))
				return false;
			client->rtt = calculate_rtt(Game::real_time.total, client->ack, msgs_out_history);
			vi_debug("RTT client %s:%hd: %dms", Sock::host_to_str(client->address.host), client->address.port, s32(client->rtt * 1000.0f));

			client->timeout = 0.0f;
			break;
		}
		default:
		{
			vi_debug("Discarding packet due to invalid packet type.");
			return false;
		}
	}

	return true;
}

}

#else

namespace Client
{

enum class Mode
{
	Disconnected,
	Connecting,
	Acking,
	Connected,
};

Sock::Address server_address;
Mode mode;
r32 timeout;
MessageHistory msgs_in_history; // messages we've received from the server
Ack server_ack = { u32(-1), 0 }; // most recent ack we've received from the server
SequenceHistory server_recently_resent; // sequences we recently resent to the server
r32 server_rtt;
SequenceID server_processed_sequence_id; // most recent sequence ID we've processed from the server

b8 init()
{
	if (Sock::udp_open(&sock, 3495, true))
	{
		if (Sock::udp_open(&sock, 3496, true))
		{
			printf("%s\n", Sock::get_error());
			return false;
		}
	}

	return true;
}

b8 build_packet_connect(StreamWrite* p)
{
	packet_init(p);
	using Stream = StreamWrite;
	ClientPacket type = ClientPacket::Connect;
	serialize_enum(p, ClientPacket, type);
	packet_finalize(p);
	return true;
}

b8 build_packet_ack_init(StreamWrite* p)
{
	packet_init(p);
	using Stream = StreamWrite;
	ClientPacket type = ClientPacket::AckInit;
	serialize_enum(p, ClientPacket, type);
	packet_finalize(p);
	return true;
}

b8 build_packet_update(StreamWrite* p)
{
	packet_init(p);
	using Stream = StreamWrite;
	ClientPacket type = ClientPacket::Update;
	serialize_enum(p, ClientPacket, type);

	// ack received messages
	Ack ack = msg_history_ack(msgs_in_history);
	serialize_int(p, SequenceID, ack.sequence_id, 0, SEQUENCE_COUNT - 1);
	serialize_u32(p, ack.previous_sequences);

	msgs_write(p, msgs_out_history, server_ack, &server_recently_resent, server_rtt);
	packet_finalize(p);
	return true;
}

void update(const Update& u)
{
	while (const MessageFrame* frame = msg_frame_advance(msgs_in_history, &server_processed_sequence_id))
	{
		// todo: process messages in frame
	}
}

void tick(const Update& u)
{
	timeout += Game::real_time.delta;
	switch (mode)
	{
		case Mode::Disconnected:
		{
			break;
		}
		case Mode::Connecting:
		{
			if (timeout > 0.5f)
			{
				timeout = 0.0f;
				vi_debug("Connecting to %s:%hd...", Sock::host_to_str(server_address.host), server_address.port);
				StreamWrite p;
				build_packet_connect(&p);
				packet_send(p, server_address);
			}
			break;
		}
		case Mode::Acking:
		{
			if (timeout > 0.5f)
			{
				timeout = 0.0f;
				vi_debug("Confirming connection to %s:%hd...", Sock::host_to_str(server_address.host), server_address.port);
				StreamWrite p;
				build_packet_ack_init(&p);
				packet_send(p, server_address);
			}
			break;
		}
		case Mode::Connected:
		{
			if (timeout > TIMEOUT)
			{
				vi_debug("Lost connection to %s:%hd.", Sock::host_to_str(server_address.host), server_address.port);
				mode = Mode::Disconnected;
			}
			else
			{
				msgs_out_consolidate();

				StreamWrite p;
				build_packet_update(&p);
				packet_send(p, server_address);

				local_sequence_id++;
				if (local_sequence_id == SEQUENCE_COUNT)
					local_sequence_id = 0;
			}
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}
}

void connect(const char* ip, u16 port)
{
	Sock::get_address(&server_address, ip, port);
	mode = Mode::Connecting;
}

b8 packet_handle(StreamRead* p, const Sock::Address& address)
{
	using Stream = StreamRead;
	if (!address.equals(server_address))
	{
		vi_debug("%s", "Discarding packet from unexpected host.");
		return false;
	}
	if (!p->read_checksum())
	{
		vi_debug("%s", "Discarding packet due to invalid checksum.");
		return false;
	}
	ServerPacket type;
	serialize_enum(p, ServerPacket, type);
	switch (type)
	{
		case ServerPacket::Init:
		{
			if (mode == Mode::Connecting)
				mode = Mode::Acking; // acknowledge the init packet
			break;
		}
		case ServerPacket::Keepalive:
		{
			timeout = 0.0f; // reset connection timeout
			break;
		}
		case ServerPacket::Update:
		{
			if (mode == Mode::Acking)
			{
				vi_debug("Connected to %s:%hd.", Sock::host_to_str(server_address.host), server_address.port);
				mode = Mode::Connected;
			}

			if (!msgs_read(p, &msgs_in_history, &server_ack))
				return false;
			server_rtt = calculate_rtt(Game::real_time.total, server_ack, msgs_out_history);
			vi_debug("RTT: %dms", s32(server_rtt * 1000.0f));
			timeout = 0.0f; // reset connection timeout
			break;
		}
		default:
		{
			vi_debug("%s", "Discarding packet due to invalid packet type.");
			return false;
		}
	}

	return true;
}

}

#endif

b8 init()
{
	if (Sock::init())
		return false;

#if SERVER
	return Server::init();
#else
	return Client::init();
#endif
}

b8 send_garbage()
{
	using Stream = StreamWrite;
	StreamWrite* msg = msg_new();
	u8 garbage[255];
	s32 len = 1 + mersenne::rand() % 254;
	serialize_bytes(msg, garbage, len);
	return true;
}

void update(const Update& u)
{
	while (true)
	{
		Sock::Address address;
		s32 bytes_received = Sock::udp_receive(&sock, &address, incoming_packet.data.data, MAX_PACKET_SIZE);
		if (bytes_received > 0)// && mersenne::randf_co() < 0.75f) // packet loss simulation
		{
			incoming_packet.data.length = bytes_received / sizeof(u32);
#if SERVER
			Server::packet_handle(&incoming_packet, address);
#else
			Client::packet_handle(&incoming_packet, address);
#endif
			incoming_packet.reset();
		}
		else
			break;
	}

#if SERVER
	Server::update(u);
#else
	Client::update(u);
#endif

	tick_timer += Game::real_time.delta;
	if (tick_timer > TICK_RATE)
	{
		tick_timer = 0.0f;

#if SERVER
		Server::tick(u);
#else
		Client::tick(u);
#endif
	}
}

void term()
{
	Sock::close(&sock);
}

StreamWrite* msg_new()
{
	StreamWrite* result = msgs_out.add();
	result->reset();
	return result;
}


}


}