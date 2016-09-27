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

#define TIMEOUT 2.0f
#define TICK_RATE (1.0f / 60.0f)
#define SEQUENCE_COUNT 65535

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

typedef u16 SequenceID;

struct MessageFrame // container for the amount of messages that can come in a single frame
{
	union
	{
		StreamRead read;
		StreamWrite write;
	};
	SequenceID sequence_id;

	MessageFrame() : read(), sequence_id() {}
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

b8 sequence_more_recent(SequenceID s1, SequenceID s2)
{
	return (s1 > s2) && (s1 - s2 <= SEQUENCE_COUNT / 2) 
	   || (s2 > s1) && (s2 - s1 > SEQUENCE_COUNT / 2);
}

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

MessageFrame* msg_history_add(MessageHistory* history)
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
	new (frame) MessageFrame();
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
				s32 sequence_id_relative_to_most_recent;
				if (msg.sequence_id < ack.sequence_id)
					sequence_id_relative_to_most_recent = s32(msg.sequence_id) - s32(ack.sequence_id);
				else
					sequence_id_relative_to_most_recent = s32(msg.sequence_id) - (s32(ack.sequence_id) + SEQUENCE_COUNT);
				vi_assert(sequence_id_relative_to_most_recent < 0);
				if (sequence_id_relative_to_most_recent >= -32)
					ack.previous_sequences |= 1 << (-sequence_id_relative_to_most_recent - 1);
			}
		}
	}
	return ack;
}

StaticArray<MessageFrame, MESSAGE_BUFFER> msgs_out_history;
s32 msgs_out_history_index;
StaticArray<StreamWrite, MESSAGE_BUFFER> msgs_out;
StreamRead incoming_packet;
r32 tick_timer;
Sock::Handle sock;
SequenceID local_sequence_id;

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

// TODO: this function removes messages from the msgs_out queue. If there is more than one client, that's not going to work.
// Figure out a separate message queue for each client? Or figure out a better way?
b8 msgs_write(StreamWrite* p)
{
	using Stream = StreamWrite;
	s32 bytes = 0;
	s32 msgs = 0;
	for (s32 i = 0; i < msgs_out.length; i++)
	{
		s32 msg_bytes = msgs_out[i].bytes_written();
		if (64 + bytes + msg_bytes >= MAX_MESSAGES_SIZE)
			break;
		bytes += msg_bytes;
		msgs++;
	}

	MessageFrame* frame;
	if (msgs_out_history.length < msgs_out_history.capacity())
	{
		frame = msgs_out_history.add();
		msgs_out_history_index = msgs_out_history.length - 1;
	}
	else
	{
		msgs_out_history_index = (msgs_out_history_index + 1) % msgs_out_history.capacity();
		frame = &msgs_out_history[msgs_out_history_index];
		new (frame) MessageFrame();
	}

	frame->sequence_id = local_sequence_id;

	serialize_int(&frame->write, s32, bytes, 0, MAX_MESSAGES_SIZE); // message frame szie
	if (bytes > 0)
	{
		serialize_u16(&frame->write, frame->sequence_id);
		for (s32 i = 0; i < msgs; i++)
			serialize_bytes(&frame->write, (u8*)msgs_out[i].data.data, msgs_out[i].bytes_written());
	}
	
	for (s32 i = msgs - 1; i >= 0; i--)
		msgs_out.remove_ordered(i);

	serialize_bytes(p, (u8*)frame->write.data.data, frame->write.bytes_written());
	bytes = 0;
	serialize_int(p, s32, bytes, 0, MAX_MESSAGES_SIZE); // zero sized frame marks end of message frames

	return true;
}

b8 msgs_read(StreamRead* p, MessageHistory* history, Ack* ack)
{
	using Stream = StreamRead;

	serialize_u16(p, ack->sequence_id);
	serialize_u32(p, ack->previous_sequences);

	while (true)
	{
		serialize_align(p);
		s32 bytes;
		serialize_int(p, s32, bytes, 0, MAX_MESSAGES_SIZE);
		if (bytes)
		{
			MessageFrame* frame = msg_history_add(history);
			frame->read.data.length = bytes / sizeof(u32);

			serialize_u16(p, frame->sequence_id);
			serialize_bytes(p, (u8*)frame->read.data.data, bytes);

			vi_debug("Received %d bytes on sequence %d", bytes, s32(frame->sequence_id));
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
	Ack ack;
	MessageHistory msgs_in_history;
	Ref<LocalPlayer> player;
	b8 connected;
};

Array<Client> clients;
r32 tick_timer;

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
	serialize_u16(p, ack.sequence_id);
	serialize_u32(p, ack.previous_sequences);
	msgs_write(p);
	packet_finalize(p);
	return true;
}

void tick(const Update& u)
{
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
		ack_debug("Client ack:", client->ack);
		ack_debug("My ack:", msg_history_ack(client->msgs_in_history));
	}
}

b8 handle_packet(StreamRead* p, const Sock::Address& address)
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
MessageHistory msgs_in_history;
Ack ack_server;

b8 init()
{
	if (Sock::udp_open(&sock, 3495, true))
	{
		printf("%s\n", Sock::get_error());
		return false;
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
	serialize_u16(p, ack.sequence_id);
	serialize_u32(p, ack.previous_sequences);

	msgs_write(p);
	packet_finalize(p);
	return true;
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
			StreamWrite p;
			build_packet_update(&p);
			packet_send(p, server_address);
			ack_debug("Server ack:", ack_server);
			ack_debug("My ack:", msg_history_ack(msgs_in_history));
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

b8 handle_packet(StreamRead* p, const Sock::Address& address)
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

			if (!msgs_read(p, &msgs_in_history, &ack_server))
				return false;
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
		if (bytes_received > 0 && mersenne::randf_co() > 0.5f) // packet loss simulation
		{
			incoming_packet.data.length = bytes_received / sizeof(u32);
#if SERVER
			Server::handle_packet(&incoming_packet, address);
#else
			Client::handle_packet(&incoming_packet, address);
#endif
			incoming_packet.reset();
		}
		else
			break;
	}

	tick_timer += Game::real_time.delta;
	if (tick_timer > TICK_RATE)
	{
		tick_timer = 0.0f;

#if SERVER
		if (Server::connected_clients() > 0)
		{
#else
		if (Client::mode == Client::Mode::Connected)
		{
#endif
			send_garbage();
		}

		vi_debug("Local sequence ID: %d", s32(local_sequence_id));

#if SERVER
		Server::tick(u);
#else
		Client::tick(u);
#endif
		local_sequence_id++;
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