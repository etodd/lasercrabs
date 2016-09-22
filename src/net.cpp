#include "net.h"
#include "net_serialize.h"
#include "platform/sock.h"
#include "game/game.h"
#if SERVER
#include "asset/level.h"
#endif

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
	count,
};

enum class ServerPacket
{
	Init,
	Update,
	count,
};

Sock::Handle sock;

void send(StreamWrite* p, const Sock::Address& address)
{
	p->finalize();
	Sock::udp_send(&sock, address, p->data.data, p->data.length * sizeof(u32));
}

#if SERVER

namespace Server
{

struct LocalPlayer;

struct Client
{
	Sock::Address address;
	r32 timeout;
	Ref<LocalPlayer> player;
};

Array<Client> clients;
r32 tick_timer;

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
	using Stream = StreamWrite;
	ServerPacket type = ServerPacket::Init;
	serialize_enum(p, ServerPacket, type);
	return true;
}

b8 build_packet_update(StreamWrite* p)
{
	using Stream = StreamWrite;
	ServerPacket type = ServerPacket::Update;
	serialize_enum(p, ServerPacket, type);
	return true;
}

void update(const Update& u)
{
	tick_timer += Game::real_time.delta;
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

	if (tick_timer > TICK_RATE)
	{
		tick_timer = 0.0f;
		StreamWrite p;
		build_packet_update(&p);
		for (s32 i = 0; i < clients.length; i++)
			send(&p, clients[i].address);
	}
}

b8 handle_packet(StreamRead* packet, const Sock::Address& address)
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
	if (!packet->read_checksum())
	{
		vi_debug("Discarding packet for invalid checksum.");
		return false;
	}

	ClientPacket type;
	serialize_enum(packet, ClientPacket, type);

	switch (type)
	{
		case ClientPacket::Connect:
		{
			b8 already_connected = false;
			for (s32 i = 0; i < clients.length; i++)
			{
				if (clients[i].address.equals(address))
				{
					already_connected = true;
					break;
				}
			}

			if (already_connected)
				vi_debug("Client %s:%hd already connected.", Sock::host_to_str(address.host), address.port);
			else
			{
				Client* client = clients.add();
				new (client) Client();
				client->address = address;
				vi_debug("Client %s:%hd connected.", Sock::host_to_str(address.host), address.port);

				StreamWrite p;
				build_packet_init(&p);
				send(&p, address);
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
	Connected,
};

Sock::Address server_address;
Mode mode;
r32 timeout;
r32 tick_timer;

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
	using Stream = StreamWrite;
	ClientPacket type = ClientPacket::Connect;
	serialize_enum(p, ClientPacket, type);

	return true;
}

b8 build_packet_update(StreamWrite* p)
{
	using Stream = StreamWrite;
	ClientPacket type = ClientPacket::Update;
	serialize_enum(p, ClientPacket, type);

	return true;
}

void update(const Update& u)
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
				send(&p, server_address);
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
			tick_timer += Game::real_time.delta;
			if (tick_timer > TICK_RATE)
			{
				tick_timer = 0.0f;
				StreamWrite p;
				build_packet_update(&p);
				send(&p, server_address);
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

b8 handle_packet(StreamRead* p, const Sock::Address& address)
{
	using Stream = StreamRead;
	if (!address.equals(server_address))
	{
		vi_debug("Discarding packet from unexpected host.");
		return false;
	}
	if (!p->read_checksum())
	{
		vi_debug("Discarding packet due to invalid checksum.");
		return false;
	}
	ServerPacket type;
	serialize_enum(p, ServerPacket, type);
	switch (type)
	{
		case ServerPacket::Init:
		{
			vi_debug("Connected to %s:%hd.", Sock::host_to_str(server_address.host), server_address.port);
			mode = Mode::Connected;
			break;
		}
		case ServerPacket::Update:
		{
			break;
		}
		default:
		{
			vi_debug("Discarding packet due to invalid packet type.");
			return false;
		}
	}

	timeout = 0.0f; // reset connection timeout
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

void update(const Update& u)
{
	static StreamRead incoming_packet;
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

#if SERVER
	Server::update(u);
#else
	Client::update(u);
#endif
}

void term()
{
	Sock::close(&sock);
}


}


}