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

#define MASTER_AUDIT_INTERVAL 3.0 // remove inactive nodes every x seconds
#define MASTER_MATCH_INTERVAL 1.0 // run matchmaking searches every x seconds
#define MASTER_INACTIVE_THRESHOLD 8.0 // remove node if it's inactive for x seconds
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
			ServerIdle,
			ClientWaiting,
			ClientIdle,
			count,
		};

		r64 last_message_timestamp;
		State state;
		Sock::Address addr;
		ServerState server_state;
	};

	std::unordered_map<Sock::Address, Node> nodes;
	Sock::Handle sock;
	Messenger messenger;
	Array<Sock::Address> servers;
	Array<Sock::Address> clients_waiting;

	Node* node_for_address(Sock::Address addr)
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

	b8 packet_handle(StreamRead* p, Sock::Address addr, r64 timestamp)
	{
		using Stream = StreamRead;
		SequenceID seq;
		serialize_int(p, SequenceID, seq, 0, NET_SEQUENCE_COUNT - 1);
		Message type;
		serialize_enum(p, Message, type);
		if (!messenger.received(type, seq, addr, &sock))
			return false; // out of order

		Node* node = node_for_address(addr);
		node->last_message_timestamp = timestamp;

		switch (type)
		{
			case Message::Ack:
			case Message::Keepalive:
			{
				break;
			}
			case Message::ClientRequestServer:
			{
				ServerState s;
				if (!serialize_server_state(p, &s))
					net_error();
				if (s.level < 0 || s.level >= Asset::Level::count)
					net_error();
				if (node->state == Node::State::Invalid
					|| node->state == Node::State::ClientIdle
					|| node->state == Node::State::ClientWaiting)
				{
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
				if (node->state == Node::State::Invalid
					|| node->state == Node::State::ServerActive
					|| node->state == Node::State::ServerIdle)
				{
					node->server_state = s;
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

	b8 send_server_load(r64 timestamp, ServerState* s, Sock::Address addr)
	{
		using Stream = StreamWrite;
		StreamWrite p;
		packet_init(&p);
		messenger.add_header(&p, addr, Message::ServerLoad);
		if (!serialize_server_state(&p, s))
			return false;
		packet_finalize(&p);
		messenger.send(p, timestamp, addr, &sock);
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

	void match(r64 timestamp, Node* server, Node* client)
	{
		server->state = Node::State::ServerActive;
		server->server_state = client->server_state;
		send_server_load(timestamp, &server->server_state, server->addr);
		send_client_connect(timestamp, server->addr, client->addr);
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

		while (true)
		{
			r64 timestamp = platform::timestamp();

			messenger.update(timestamp, &sock);

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
				{
					const Sock::Address& addr = removals[i];
					const Node& node = nodes[addr];
					if (node.state == Node::State::ServerActive || node.state == Node::State::ServerIdle)
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
					}
					else if (node.state == Node::State::ClientWaiting)
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
					}
					nodes.erase(addr);
					messenger.remove(addr);
				}
				last_audit = timestamp;
			}

			if (timestamp - last_match > MASTER_MATCH_INTERVAL)
			{
				last_match = timestamp;
				for (s32 i = 0; i < servers.length; i++)
				{
					Node* server = &nodes[servers[i]];
					if (server->state == Node::State::ServerIdle
						&& !messenger.has_unacked_outgoing_messages(server->addr))
					{
						// find someone looking for a story-mode server and give this server to them
						for (s32 j = 0; j < clients_waiting.length; j++)
						{
							Node* client = &nodes[clients_waiting[j]];
							if (client->server_state.story_mode)
								match(timestamp, server, client);
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