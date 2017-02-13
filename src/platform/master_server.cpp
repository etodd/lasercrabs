#define _AMD64_

#include "game/master.h"

#include <thread>
#if _WIN32
#include <Windows.h>
#endif
#include <time.h>
#include <chrono>
#include "sock.h"
#include <unordered_map>
#include "asset/level.h"
#include "cjson/cJSON.h"

namespace VI
{

namespace platform
{

	u64 timestamp()
	{
		time_t t;
		::time(&t);
		return u64(t);
	}

	r64 time()
	{
		return r64(std::chrono::high_resolution_clock::now().time_since_epoch().count()) / 1000000000.0;
	}

	void sleep(r32 time)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(s64(time * 1000.0f)));
	}

}

namespace Net
{

namespace Master
{

#define MASTER_AUDIT_INTERVAL 1.25 // remove inactive nodes every x seconds
#define MASTER_MATCH_INTERVAL 0.5 // run matchmaking searches every x seconds
#define MASTER_INACTIVE_THRESHOLD 7.0 // remove node if it's inactive for x seconds
#define MASTER_CLIENT_CONNECTION_TIMEOUT 8.0 // clients have x seconds to connect to a server once we tell them to
#define MASTER_SETTINGS_FILE "config.txt"

	namespace Settings
	{
		s32 secret;
	}

	struct Node // could be a server or client
	{
		enum class State
		{
			Invalid,
			ServerActive,
			ServerLoading,
			ServerIdle,
			ClientWaiting,
			ClientConnecting,
			ClientIdle,
			count,
		};

		Save* save;
		r64 last_message_timestamp;
		State state;
		Sock::Address addr;
		ServerState server_state;
	};

	struct ClientConnection
	{
		r64 timestamp;
		Sock::Address client;
		Sock::Address server;
	};

	std::unordered_map<Sock::Address, Node> nodes;
	Sock::Handle sock;
	Messenger messenger;
	Array<Sock::Address> servers;
	Array<Sock::Address> clients_waiting;
	Array<ClientConnection> clients_connecting;
	Array<Sock::Address> servers_loading;

	Node* node_add_or_get(Sock::Address addr)
	{
		auto i = nodes.find(addr);
		Node* n;
		if (i == nodes.end())
		{
			auto i = nodes.insert(std::pair<Sock::Address, Node>(addr, Node()));
			n = &i.first->second;
			n->addr = addr;
		}
		else
			n = &i->second;
		return n;
	}

	Node* node_for_address(Sock::Address addr)
	{
		auto i = nodes.find(addr);
		if (i == nodes.end())
			return nullptr;
		else
			return &i->second;
	}

	void server_remove_clients_connecting(Node* server)
	{
		// reset any clients trying to connect to this server
		for (s32 i = 0; i < clients_connecting.length; i++)
		{
			const ClientConnection& connection = clients_connecting[i];
			if (connection.server.equals(server->addr))
			{
				Node* client = node_for_address(connection.client);
				if (client)
					client->state = Node::State::ClientWaiting;
				clients_connecting.remove(i);
				i--;
			}
		}
	}

	void disconnected(Sock::Address addr)
	{
		Node* node = node_for_address(addr);
		if (node->state == Node::State::ServerActive
			|| node->state == Node::State::ServerLoading
			|| node->state == Node::State::ServerIdle)
		{
			// it's a server; remove from the server list
			for (s32 i = 0; i < servers.length; i++)
			{
				if (servers[i].equals(addr))
				{
					servers.remove(i);
					i--;
				}
			}

			// reset any clients trying to connect to this server
			server_remove_clients_connecting(node);
		}
		else if (node->state == Node::State::ClientWaiting)
		{
			// it's a client waiting for a server; remove it from the wait list
			for (s32 i = 0; i < clients_waiting.length; i++)
			{
				if (clients_waiting[i].equals(addr))
				{
					clients_waiting.remove(i);
					i--;
				}
			}
			if (node->save)
			{
				delete node->save;
				node->save = nullptr;
			}
		}
		else if (node->state == Node::State::ClientConnecting)
		{
			vi_assert(!node->save);
		}
		nodes.erase(addr);
		messenger.remove(addr);
	}

	b8 send_server_load(r64 timestamp, Node* server, ServerState* s, Save* save = nullptr)
	{
		server->state = Node::State::ServerLoading;
		server->server_state = *s;
		using Stream = StreamWrite;
		StreamWrite p;
		packet_init(&p);
		messenger.add_header(&p, server->addr, Message::ServerLoad);
		if (!serialize_server_state(&p, s))
			net_error();
		vi_assert(b8(save) == s->story_mode);
		if (s->story_mode)
		{
			if (!serialize_save(&p, save))
				net_error();
		}
		packet_finalize(&p);
		messenger.send(p, timestamp, server->addr, &sock);
		return true;
	}

	b8 send_client_connect(r64 timestamp, Sock::Address server_addr, Sock::Address addr)
	{
		using Stream = StreamWrite;
		StreamWrite p;
		packet_init(&p);
		messenger.add_header(&p, addr, Message::ClientConnect);
		serialize_u32(&p, server_addr.host);
		serialize_u16(&p, server_addr.port);
		packet_finalize(&p);
		messenger.send(p, timestamp, addr, &sock);
		return true;
	}

	s8 server_client_slots_connecting(Node* server)
	{
		s8 slots = 0;
		for (s32 i = 0; i < clients_connecting.length; i++)
		{
			const ClientConnection& connection = clients_connecting[i];
			if (connection.server.equals(server->addr))
				slots += node_for_address(connection.client)->server_state.open_slots;
		}
		return slots;
	}

	s8 server_open_slots(Node* server)
	{
		vi_assert(server->state == Node::State::ServerLoading
			|| server->state == Node::State::ServerActive
			|| server->state == Node::State::ServerIdle);
		return server->server_state.open_slots - server_client_slots_connecting(server);
	}

	void server_update_state(Node* server, const ServerState& s)
	{
		s8 original_open_slots = server->server_state.open_slots;
		server->server_state = s;
		s8 clients_connecting_count = server_client_slots_connecting(server);
		if (clients_connecting_count > 0)
		{
			// there are clients trying to connect to this server
			// check if the server has registered that these clients have connected
			if (s.open_slots <= server->server_state.open_slots - clients_connecting_count)
			{
				// the clients have connected, all's well
				// remove ClientConnection records
				server_remove_clients_connecting(server);
			}
			else // clients have not connected yet, maintain old slot count
				server->server_state.open_slots = original_open_slots;
		}
	}

	b8 packet_handle(StreamRead* p, Sock::Address addr, r64 timestamp)
	{
		using Stream = StreamRead;
		{
			s16 version;
			serialize_s16(p, version);
			if (version < GAME_VERSION)
			{
				using Stream = StreamWrite;
				StreamWrite p;
				packet_init(&p);
				messenger.add_header(&p, addr, Message::WrongVersion);
				packet_finalize(&p);
				Sock::udp_send(&sock, addr, p.data.data, p.bytes_written());
				return false; // ignore this packet
			}
		}
		SequenceID seq;
		serialize_int(p, SequenceID, seq, 0, NET_SEQUENCE_COUNT - 1);
		Message type;
		serialize_enum(p, Message, type);
		if (!messenger.received(type, seq, addr, &sock))
			return false; // out of order

		Node* node = node_add_or_get(addr);
		node->last_message_timestamp = timestamp;

		switch (type)
		{
			case Message::Ack:
			{
				break;
			}
			case Message::Disconnect:
			{
				disconnected(addr);
				break;
			}
			case Message::ClientRequestServer:
			{
				ServerState s;
				if (!serialize_server_state(p, &s))
					net_error();
				if (s.level < 0
					|| s.open_slots == 0
					|| s.level >= Asset::Level::count
					|| (s.story_mode && s.open_slots != 1)
					|| (s.story_mode && s.team_count != 2))
					net_error();
				if (node->state == Node::State::ClientConnecting)
				{
					// ignore
				}
				else if (node->state == Node::State::Invalid
					|| node->state == Node::State::ClientIdle
					|| node->state == Node::State::ClientWaiting)
				{
					if (s.story_mode)
					{
						if (!node->save)
							node->save = new Save();
						if (!serialize_save(p, node->save))
						{
							delete node->save;
							node->save = nullptr;
							net_error();
						}
					}
					node->server_state = s;
					if (node->state != Node::State::ClientWaiting)
					{
						// add to client waiting list
						clients_waiting.add(node->addr);
					}
					node->state = Node::State::ClientWaiting;
				}
				else // invalid state transition
					net_error();
				break;
			}
			case Message::ServerStatusUpdate:
			{
				s32 secret;
				serialize_s32(p, secret);
				if (secret != Settings::secret)
					net_error();
				b8 active;
				serialize_bool(p, active);
				ServerState s;
				if (!serialize_server_state(p, &s))
					net_error();
				if (node->state == Node::State::ServerLoading)
				{
					if (s.equals(node->server_state) && active) // done loading
					{
						server_update_state(node, s);
						node->state = Node::State::ServerActive;
						
						// tell clients to connect to this server
						for (s32 i = 0; i < clients_connecting.length; i++)
						{
							const ClientConnection& connection = clients_connecting[i];
							if (connection.server.equals(node->addr))
								send_client_connect(timestamp, node->addr, connection.client);
						}
					}
				}
				else if (node->state == Node::State::Invalid
					|| node->state == Node::State::ServerActive
					|| node->state == Node::State::ServerIdle)
				{
					server_update_state(node, s);
					if (node->state == Node::State::Invalid)
					{
						// add to server list
						servers.add(node->addr);
					}
					node->state = active ? Node::State::ServerActive : Node::State::ServerIdle;
				}
				break;
			}
			default:
			{
				net_error();
				break;
			}
		}

		return true;
	}

	Node* alloc_server(r64 timestamp, ServerState* s, Save* save = nullptr)
	{
		for (s32 i = 0; i < servers.length; i++)
		{
			Node* n = node_for_address(servers[i]);
			if (n->state == Node::State::ServerIdle)
			{
				send_server_load(timestamp, n, s, save);
				return n;
			}
		}
		return nullptr;
	}

	void client_queue_join(r64 timestamp, Node* server, Node* client)
	{
		vi_assert(client->state == Node::State::ClientWaiting);
		b8 found = false;
		for (s32 i = 0; i < clients_waiting.length; i++)
		{
			if (clients_waiting[i].equals(client->addr))
			{
				found = true;
				clients_waiting.remove(i);
				break;
			}
		}
		vi_assert(found);
		if (client->save)
		{
			delete client->save;
			client->save = nullptr;
		}

		ClientConnection* connection = clients_connecting.add();
		connection->timestamp = timestamp;
		connection->server = server->addr;
		connection->client = client->addr;

		client->state = Node::State::ClientConnecting;
	}

	s32 proc()
	{
		if (Sock::init())
			return 1;

		if (Sock::udp_open(&sock, 3497, true))
		{
			fprintf(stderr, "%s\n", Sock::get_error());
			return 1;
		}

		// load settings
		{
			FILE* f = fopen(MASTER_SETTINGS_FILE, "rb");
			if (f)
			{
				fseek(f, 0, SEEK_END);
				long len = ftell(f);
				fseek(f, 0, SEEK_SET);

				char* data = (char*)calloc(sizeof(char), len + 1);
				fread(data, 1, len, f);
				data[len] = '\0';
				fclose(f);

				cJSON* json = cJSON_Parse(data);
				if (json)
				{
					cJSON* secret = cJSON_GetObjectItem(json, "secret");
					if (secret)
						Settings::secret = secret->valueint;
					cJSON_Delete(json);
				}
				else
					fprintf(stderr, "Can't parse json file '%s': %s\n", MASTER_SETTINGS_FILE, cJSON_GetErrorPtr());
				free(data);
			}
		}

		r64 last_audit = 0.0;
		r64 last_match = 0.0;
		r64 timestamp = 0.0;
		r64 last_update = platform::time();

		while (true)
		{
			{
				r64 t = platform::time();
				timestamp += vi_min(0.25, t - last_update);
				last_update = t;
			}

			messenger.update(timestamp, &sock);

			// remove timed out client connection attempts
			{
				r64 threshold = timestamp - MASTER_CLIENT_CONNECTION_TIMEOUT;
				for (s32 i = 0; i < clients_connecting.length; i++)
				{
					const ClientConnection& c = clients_connecting[i];
					if (c.timestamp < threshold)
					{
						Node* client = node_for_address(c.client);
						if (client && client->state == Node::State::ClientConnecting)
							client->state = Node::State::ClientWaiting; // give up connecting, go back to matchmaking

						Node* server = node_for_address(c.server);
						if (server)
						{
							// since there are clients connecting to this server, we've been ignoring its updates telling us how many open slots it has
							// we need to manually update the open slot count until the server gives us a fresh count
							// don't try to fill these slots until the server tells us for sure they're available
							server->server_state.open_slots -= client->server_state.open_slots;
						}

						clients_connecting.remove(i);
						i--;
					}
				}
			}

			// remove inactive nodes
			if (timestamp - last_audit > MASTER_AUDIT_INTERVAL)
			{
				r64 threshold = timestamp - MASTER_INACTIVE_THRESHOLD;
				Array<Sock::Address> removals;
				for (auto i = nodes.begin(); i != nodes.end(); i++)
				{
					if (i->second.last_message_timestamp < threshold)
						removals.add(i->first);
				}
				for (s32 i = 0; i < removals.length; i++)
					disconnected(removals[i]);
				last_audit = timestamp;
			}

			if (timestamp - last_match > MASTER_MATCH_INTERVAL)
			{
				last_match = timestamp;
				Array<Sock::Address> multiplayer_servers;
				s32 existing_multiplayer_slots = 0;

				s32 idle_servers = 0;
				for (s32 i = 0; i < servers.length; i++)
				{
					Node* server = node_for_address(servers[i]);
					if (server->state == Node::State::ServerIdle)
					{
						// find someone looking for a story-mode server and give this server to them
						for (s32 j = 0; j < clients_waiting.length; j++)
						{
							Node* client = node_for_address(clients_waiting[j]);
							if (client->server_state.story_mode)
							{
								send_server_load(timestamp, server, &client->server_state, client->save);
								client_queue_join(timestamp, server, client);
								break;
							}
						}

						if (server->state == Node::State::ServerIdle) // still idle
							idle_servers += 1;
					}
					else
					{
						vi_assert(server->state == Node::State::ServerActive || server->state == Node::State::ServerLoading);
						s8 open_slots = server_open_slots(server);
						if (!server->server_state.story_mode && open_slots > 0)
						{
							multiplayer_servers.add(server->addr);
							existing_multiplayer_slots += open_slots;
						}
					}
				}

				// allocate multiplayer servers as necessary
				s32 needed_multiplayer_slots = 0;
				{
					for (s32 i = 0; i < clients_waiting.length; i++)
					{
						Node* node = node_for_address(clients_waiting[i]);
						if (!node->server_state.story_mode)
							needed_multiplayer_slots += node->server_state.open_slots;
					}

					// todo: different multiplayer setups
					ServerState multiplayer_state;
					multiplayer_state.level = Asset::Level::Medias_Res;
					multiplayer_state.open_slots = 4;
					multiplayer_state.story_mode = false;
					multiplayer_state.team_count = 2;
					multiplayer_state.game_type = GameType::Rush;

					s32 server_allocs = vi_min(idle_servers, ((needed_multiplayer_slots - existing_multiplayer_slots) + MAX_PLAYERS - 1) / MAX_PLAYERS); // ceil divide
					for (s32 i = 0; i < server_allocs; i++)
					{
						Node* server = alloc_server(timestamp, &multiplayer_state);
						if (server)
							multiplayer_servers.add(server->addr);
						else
							break; // not enough servers available
					}
				}

				// assign clients to servers
				for (s32 i = 0; i < multiplayer_servers.length; i++)
				{
					Node* server = node_for_address(multiplayer_servers[i]);
					s8 slots = server_open_slots(server);
					for (s32 j = 0; j < clients_waiting.length; j++)
					{
						Node* client = node_for_address(clients_waiting[j]);
						if (slots >= client->server_state.open_slots)
						{
							client_queue_join(timestamp, server, client);
							if (server->state != Node::State::ServerLoading) // server already loaded, no need to wait for it
								send_client_connect(timestamp, server->addr, client->addr);
							slots -= client->server_state.open_slots;
							if (slots == 0)
								break;
						}
					}
				}
			}

			Sock::Address addr;
			StreamRead packet;
			s32 bytes_read = Sock::udp_receive(&sock, &addr, packet.data.data, NET_MAX_PACKET_SIZE);
			packet.resize_bytes(bytes_read);
			if (bytes_read > 0)
			{
				if (packet.read_checksum())
				{
					packet_decompress(&packet, bytes_read);
					packet_handle(&packet, addr, timestamp);
				}
				else
					vi_debug("%s", "Discarding packet due to invalid checksum.");
			}
			else
				platform::sleep(1.0f / 60.0f);
		}

		return 0;
	}

}

}

}

int main(int argc, char** argv)
{
	return VI::Net::Master::proc();
}