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

#define MESSAGE_BUFFER s32(TIMEOUT / TICK_RATE)

struct MessageFrame // container for the amount of messages that can come in a single frame
{
	union
	{
		StreamRead read;
		StreamWrite write;
	};
	u8 sequence_id;

	MessageFrame() : read(), sequence_id() {}
	~MessageFrame() {}
};

StaticArray<MessageFrame, MESSAGE_BUFFER> msgs_in_history;
s32 msgs_in_history_index;
StaticArray<MessageFrame, MESSAGE_BUFFER> msgs_out_history;
s32 msgs_out_history_index;
StaticArray<StreamWrite, MESSAGE_BUFFER> msgs_out;
StreamRead incoming_packet;
r32 tick_timer;
Sock::Handle sock;
u8 local_sequence_id;

b8 sequence_more_recent(u8 s1, u8 s2)
{
	return (s1 > s2) && (s1 - s2 <= 127) 
	   || (s2 > s1) && (s2 - s1 > 127);
}

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

b8 msgs_write(StreamWrite* p)
{
	using Stream = StreamWrite;
	s32 bytes = 0;
	s32 msgs = 0;
	for (s32 i = 0; i < msgs_out.length; i++)
	{
		s32 msg_bytes = msgs_out[i].bytes_written();
		if (64 + bytes + msg_bytes >= MAX_PACKET_SIZE / 2)
			break;
		bytes += msg_bytes;
		msgs++;
	}

	MessageFrame* frame;
	if (msgs_out_history.length < msgs_out_history.capacity())
		frame = msgs_out_history.add();
	else
	{
		frame = &msgs_out_history[msgs_out_history_index];
		new (frame) MessageFrame();
		msgs_out_history_index = (msgs_out_history_index + 1) % msgs_out_history.capacity();
	}

	frame->sequence_id = local_sequence_id;

	serialize_u8(&frame->write, frame->sequence_id);
	serialize_int(&frame->write, s32, bytes, 0, MAX_PACKET_SIZE / 2);
	for (s32 i = 0; i < msgs; i++)
	{
		serialize_bytes(&frame->write, (u8*)msgs_out[i].data.data, msgs_out[i].bytes_written());
		serialize_align(&frame->write);
	}
	
	for (s32 i = msgs - 1; i >= 0; i--)
		msgs_out.remove_ordered(i);

	serialize_bytes(p, (u8*)frame->write.data.data, frame->write.bytes_written());

	return true;
}

b8 msgs_read(StreamRead* p)
{
	using Stream = StreamRead;

	MessageFrame* frame;
	if (msgs_in_history.length < msgs_in_history.capacity())
		frame = msgs_in_history.add();
	else
	{
		frame = &msgs_in_history[msgs_in_history_index];
		new (frame) MessageFrame();
		msgs_in_history_index = (msgs_in_history_index + 1) % msgs_in_history.capacity();
	}

	serialize_u8(p, frame->sequence_id);
	serialize_int(p, u16, frame->read.data.length, 0, MAX_PACKET_SIZE / 2);
	serialize_bytes(p, (u8*)frame->read.data.data, frame->read.data.length);

	vi_debug("Received %d bytes on sequence %d", frame->read.data.length, s32(frame->sequence_id));

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
	Ref<LocalPlayer> player;
	b8 connected;
	u8 last_acked_sequence_id;
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

b8 build_packet_update(StreamWrite* p)
{
	packet_init(p);
	using Stream = StreamWrite;
	ServerPacket type = ServerPacket::Update;
	serialize_enum(p, ServerPacket, type);
	msgs_write(p);
	packet_finalize(p);
	return true;
}

void tick(const Update& u)
{
	for (s32 i = 0; i < clients.length; i++)
	{
		clients[i].timeout += Game::real_time.delta;
		if (clients[i].timeout > TIMEOUT)
		{
			vi_debug("Client %s:%hd timed out.", Sock::host_to_str(clients[i].address.host), clients[i].address.port);
			clients.remove(i);
			i--;
		}
	}

	StreamWrite p;
	build_packet_update(&p);
	for (s32 i = 0; i < clients.length; i++)
	{
		if (clients[i].connected)
			packet_send(p, clients[i].address);
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

			if (!msgs_read(p))
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
u8 last_acked_sequence_id;

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

			if (!msgs_read(p))
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
	s32 len = mersenne::rand() % 255;
	serialize_bytes(msg, garbage, len);
	vi_debug("Sending %d bytes on sequence %d", len, s32(local_sequence_id));

	return true;
}

void update(const Update& u)
{
	while (true)
	{
		Sock::Address address;
		s32 bytes_received = Sock::udp_receive(&sock, &address, incoming_packet.data.data, MAX_PACKET_SIZE);
		if (bytes_received > 0)
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

#if SERVER
	if (Server::connected_clients() > 0)
	{
#else
	if (Client::mode == Client::Mode::Connected)
	{
#endif
		while (mersenne::randf_co() > 1.0f - u.time.delta * 5.0f)
			send_garbage();
	}

	tick_timer += Game::real_time.delta;
	if (tick_timer > TICK_RATE)
	{
		tick_timer = 0.0f;
#if SERVER
		Server::tick(u);
#else
		Client::tick(u);
#endif
		local_sequence_id++;
		vi_debug("local sequence: %d", s32(local_sequence_id));
	}
}

void term()
{
	Sock::close(&sock);
}

StreamWrite* msg_new()
{
	return msgs_out.add();
}


}


}