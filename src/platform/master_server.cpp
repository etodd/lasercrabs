#define _AMD64_

#include "game/master.h"

#include <thread>
#if _WIN32
#include <Windows.h>
#else
#include <sys/stat.h>
#endif
#include <time.h>
#include <chrono>
#include "sock.h"
#include <unordered_map>
#include "asset/level.h"
#include "cjson/cJSON.h"
#include "sqlite/sqlite3.h"
#include "http.h"
#include "mersenne/mersenne-twister.h"

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

#if _WIN32
	s64 filetime_to_posix(FILETIME ft)
	{
		// takes the last modified date
		LARGE_INTEGER date, adjust;
		date.HighPart = ft.dwHighDateTime;
		date.LowPart = ft.dwLowDateTime;

		// 100-nanoseconds = milliseconds * 10000
		adjust.QuadPart = 11644473600000 * 10000;

		// removes the diff between 1970 and 1601
		date.QuadPart -= adjust.QuadPart;

		// converts back from 100-nanoseconds to seconds
		return date.QuadPart / 10000000;
	}

	s64 filemtime(const char* file)
	{
		WIN32_FIND_DATA FindFileData;
		HANDLE handle = FindFirstFile(file, &FindFileData);
		if (handle == INVALID_HANDLE_VALUE)
			return 0;
		else
		{
			FindClose(handle);
			return filetime_to_posix(FindFileData.ftLastWriteTime);
		}
	}
#else
	s64 filemtime(const std::string& file)
	{
		struct stat st;
		if (stat(file.c_str(), &st))
			return 0;
		return st.st_mtime;
	}
#endif

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
#define TOKEN_TIMEOUT 86400

	namespace Settings
	{
		s32 secret;
		u32 public_ip;
		char itch_api_key[MAX_AUTH_KEY + 1];
	}

	struct Node // could be a server or client
	{
		enum class State : s8
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
		UserKey user_key;
		Sock::Address addr;
		ServerState server_state;
		State state;
	};

	struct ClientConnection
	{
		r64 timestamp;
		Sock::Address client;
		Sock::Address server;
		s8 slots;
	};

	std::unordered_map<Sock::Address, Node> nodes;
	Sock::Handle sock;
	Messenger messenger;
	Array<Sock::Address> servers;
	Array<Sock::Address> clients_waiting;
	Array<ClientConnection> clients_connecting;
	Array<Sock::Address> servers_loading;
	u32 localhost_ip;
	sqlite3* db;
	r64 timestamp;

	sqlite3_stmt* db_query(const char* sql)
	{
		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr))
		{
			fprintf(stderr, "SQL: Failed to prepare statement: %s\nError: %s", sql, sqlite3_errmsg(db));
			vi_assert(false);
		}
		return stmt;
	}

	void db_bind_int(sqlite3_stmt* stmt, s32 index, s64 value)
	{
		if (sqlite3_bind_int64(stmt, index + 1, value))
		{
			fprintf(stderr, "SQL: Could not bind integer at index %d.\nError: %s", index, sqlite3_errmsg(db));
			vi_assert(false);
		}
	}

	void db_bind_text(sqlite3_stmt* stmt, s32 index, const char* text)
	{
		if (sqlite3_bind_text(stmt, index + 1, text, -1, SQLITE_STATIC))
		{
			fprintf(stderr, "SQL: Could not bind text at index %d.\nError: %s", index, sqlite3_errmsg(db));
			vi_assert(false);
		}
	}

	b8 db_step(sqlite3_stmt* stmt)
	{
		s32 result = sqlite3_step(stmt);
		if (result == SQLITE_ROW)
			return true;
		else if (result == SQLITE_DONE)
			return false;
		else
		{
			fprintf(stderr, "SQL: Failed to step query.\nError: %s", sqlite3_errmsg(db));
			vi_assert(false);
			return false;
		}
	}

	s64 db_column_int(sqlite3_stmt* stmt, s32 index)
	{
		return sqlite3_column_int64(stmt, index);
	}

	const unsigned char* db_column_text(sqlite3_stmt* stmt, s32 index)
	{
		return sqlite3_column_text(stmt, index);
	}

	void db_finalize(sqlite3_stmt* stmt)
	{
		if (sqlite3_finalize(stmt))
		{
			fprintf(stderr, "SQL: Failed to finalize query.\nError: %s", sqlite3_errmsg(db));
			vi_assert(false);
		}
	}

	s64 db_exec(sqlite3_stmt* stmt)
	{
		b8 more = db_step(stmt);
		vi_assert(!more); // this kind of query shouldn't return any rows
		db_finalize(stmt);
		return sqlite3_last_insert_rowid(db);
	}
	
	void db_exec(const char* sql)
	{
		char* err;
		if (sqlite3_exec(db, sql, nullptr, nullptr, &err))
		{
			fprintf(stderr, "SQL statement failed: %s\nError: %s", sql, err);
			vi_assert(false);
		}
	}

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

	void client_desired_server_config(Node* client, ServerConfig* config)
	{
		config->id = client->server_state.id;
		config->level = client->server_state.level;
		if (client->save)
		{
			vi_assert(config->id == 0);
			config->game_type = GameType::Assault;
			config->session_type = SessionType::Story;
			config->kill_limit = 0;
			config->respawns = DEFAULT_ASSAULT_DRONES;
			config->open_slots = 1;
			config->team_count = 2;
			config->time_limit_minutes = 7;
			config->allow_abilities = true;
		}
		else
		{
			// todo: pull server config from database
		}
	}

	b8 send_auth_response(Sock::Address addr, UserKey* key)
	{
		using Stream = StreamWrite;
		StreamWrite p;
		packet_init(&p);
		messenger.add_header(&p, addr, Message::AuthResponse);

		b8 success = b8(key);
		serialize_bool(&p, success);
		if (success)
		{
			serialize_u32(&p, key->id);
			serialize_u32(&p, key->token);
		}
		packet_finalize(&p);
		messenger.send(p, timestamp, addr, &sock);
		return true;
	}

	void itch_auth_result(void* user_data, b8 success, s64 itch_id, const char* username)
	{
		Sock::Address addr = *reinterpret_cast<Sock::Address*>(&user_data);

		Node* node = node_for_address(addr);
		if (node)
		{
			if (success)
			{
				UserKey key;
				key.token = u32(mersenne::rand());

				// save user in database
				sqlite3_stmt* stmt = db_query("select id from User where itch_id=? limit 1;");
				db_bind_int(stmt, 0, itch_id);
				if (db_step(stmt))
				{
					key.id = s32(db_column_int(stmt, 0));

					// update existing user
					{
						sqlite3_stmt* stmt = db_query("update User set token=?, token_timestamp=?, username=? where itch_id=?;");
						db_bind_int(stmt, 0, key.token);
						db_bind_int(stmt, 1, platform::timestamp());
						db_bind_text(stmt, 2, username);
						db_bind_int(stmt, 3, itch_id);
						db_exec(stmt);
					}
				}
				else
				{
					// insert new user
					sqlite3_stmt* stmt = db_query("insert into User (token, token_timestamp, itch_id, username) values (?, ?, ?, ?);");
					db_bind_int(stmt, 0, key.token);
					db_bind_int(stmt, 1, platform::timestamp());
					db_bind_int(stmt, 2, itch_id);
					db_bind_text(stmt, 3, username);
					key.id = s32(db_exec(stmt));
				}
				db_finalize(stmt);

				send_auth_response(addr, &key);
			}
			else
				send_auth_response(addr, nullptr);
		}
	}

	void itch_download_key_callback(s32 code, const char* data, void* user_data)
	{
		b8 success = false;
		if (code == 200)
		{
			cJSON* json = cJSON_Parse(data);
			if (json)
			{
				cJSON* download_key = cJSON_GetObjectItem(json, "download_key");
				if (download_key)
				{
					cJSON* owner = cJSON_GetObjectItem(download_key, "owner");
					if (owner)
					{
						s64 itch_id = cJSON_GetObjectItem(owner, "id")->valuedouble;
						const char* username = cJSON_GetObjectItem(owner, "username")->valuestring;
						itch_auth_result(user_data, true, itch_id, username);
						success = true;
					}
				}
				cJSON_Delete(json);
			}
		}

		if (!success)
			itch_auth_result(user_data, false, 0, nullptr);
	}

	void itch_auth_callback(s32 code, const char* data, void* user_data)
	{
		b8 success = false;
		if (code == 200)
		{
			cJSON* json = cJSON_Parse(data);
			if (json)
			{
				cJSON* user = cJSON_GetObjectItem(json, "user");
				if (user)
				{
					cJSON* id = cJSON_GetObjectItem(user, "id");
					if (id)
					{
						success = true;

						char url[MAX_PATH_LENGTH + 1] = {};
						snprintf(url, MAX_PATH_LENGTH, "https://itch.io/api/1/%s/game/65651/download_keys?user_id=%d", Settings::itch_api_key, id->valueint);
						Http::get(url, &itch_download_key_callback, nullptr, user_data);
					}
				}
				cJSON_Delete(json);
			}
		}

		if (!success)
			itch_auth_result(user_data, false, 0, nullptr);
	}

	void disconnected(Sock::Address addr)
	{
		Node* node = node_for_address(addr);
		if (node->state == Node::State::ServerActive
			|| node->state == Node::State::ServerLoading
			|| node->state == Node::State::ServerIdle)
		{
			// it's a server; remove from the server list
			vi_debug("Server %s:%hd disconnected.", Sock::host_to_str(addr.host), addr.port);
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
		nodes.erase(addr);
		messenger.remove(addr);
	}

	b8 send_server_expect_client(Node* server, UserKey* user)
	{
		using Stream = StreamWrite;

		StreamWrite p;
		packet_init(&p);
		messenger.add_header(&p, server->addr, Message::ExpectClient);
		serialize_u32(&p, user->id);
		serialize_u32(&p, user->token);
		packet_finalize(&p);
		messenger.send(p, timestamp, server->addr, &sock);
		return true;
	}

	b8 send_server_load(Node* server, Node* client)
	{
		using Stream = StreamWrite;

		ServerConfig config;
		client_desired_server_config(client, &config);

		server->state = Node::State::ServerLoading;
		server->server_state.id = config.id;
		server->server_state.level = config.level;

		StreamWrite p;
		packet_init(&p);
		messenger.add_header(&p, server->addr, Message::ServerLoad);

		serialize_u32(&p, client->user_key.id);
		serialize_u32(&p, client->user_key.token);

		if (!serialize_server_config(&p, &config))
			net_error();

		vi_assert(b8(client->save) == (config.id == 0));
		if (client->save)
		{
			if (!serialize_save(&p, client->save))
				net_error();
		}

		packet_finalize(&p);
		messenger.send(p, timestamp, server->addr, &sock);
		return true;
	}

	b8 send_client_connect(Sock::Address server_addr, Sock::Address addr)
	{
		using Stream = StreamWrite;
		StreamWrite p;
		packet_init(&p);
		messenger.add_header(&p, addr, Message::ClientConnect);

		// if the server is running on localhost, send the public IP rather than a useless 127.0.0.1 address
		u32 host;
		if (server_addr.host == localhost_ip)
			host = Settings::public_ip;
		else
			host = server_addr.host;

		serialize_u32(&p, host);
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
				slots += connection.slots;
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

	b8 check_user_key(StreamRead* p, UserKey* key)
	{
		using Stream = Net::StreamRead;
		serialize_u32(p, key->id);
		serialize_u32(p, key->token);

		sqlite3_stmt* stmt = db_query("select token, token_timestamp from User where id=? limit 1;");
		db_bind_int(stmt, 0, key->id);
		if (db_step(stmt))
		{
			s64 token = db_column_int(stmt, 0);
			s64 token_timestamp = db_column_int(stmt, 1);
			return u32(token) == key->token && platform::timestamp() - token_timestamp < TOKEN_TIMEOUT;
		}

		return false;
	}

	Node* client_requested_server(Node* client)
	{
		// if the user is requesting to connect to a virtual server that is already active, they can connect immediately
		if (!client->save)
		{
			// check live nodes and see if any of them are running the given virtual server config
			for (s32 i = 0; i < servers.length; i++)
			{
				Node* server = node_for_address(servers[i]);
				if (server->server_state.id == client->server_state.id)
					return server;
			}
		}

		return nullptr;
	}

	void client_queue_join(Node* server, Node* client)
	{
		for (s32 i = 0; i < clients_waiting.length; i++)
		{
			if (clients_waiting[i].equals(client->addr))
			{
				clients_waiting.remove(i);
				break;
			}
		}

		if (client->save)
		{
			delete client->save;
			client->save = nullptr;
		}

		ClientConnection* connection = clients_connecting.add();
		connection->timestamp = timestamp;
		connection->server = server->addr;
		connection->client = client->addr;
		connection->slots = client->server_state.open_slots;

		client->state = Node::State::ClientConnecting;
	}

	void client_connect_to_existing_server(Node* client, Node* server)
	{
		send_server_expect_client(server, &client->user_key);
		send_client_connect(server->addr, client->addr);
		client_queue_join(server, client);
	}

	b8 packet_handle(StreamRead* p, Sock::Address addr)
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
			case Message::Auth:
			{
				AuthType auth_type;
				serialize_enum(p, AuthType, auth_type);
				s32 auth_key_length;
				serialize_int(p, s32, auth_key_length, 0, MAX_AUTH_KEY);
				char auth_key[MAX_AUTH_KEY + 1];
				serialize_bytes(p, (u8*)auth_key, auth_key_length);
				switch (auth_type)
				{
					case Master::AuthType::None:
						break;
					case Master::AuthType::Itch:
					{
						char header[MAX_PATH_LENGTH + 1] = {};
						snprintf(header, MAX_PATH_LENGTH, "Authorization: %s", auth_key);
						Http::get("https://itch.io/api/1/jwt/me", &itch_auth_callback, header, *reinterpret_cast<void**>(&node->addr));
						break;
					}
					case Master::AuthType::Steam:
					{
						// todo: Steam auth
						break;
					}
					default:
					{
						vi_assert(false);
						break;
					}
				}

				break;
			}
			case Message::ClientRequestServer:
			{
				{
					UserKey user;
					if (!check_user_key(p, &user))
						return false;
					node->user_key = user;
				}

				u32 requested_server_id;
				serialize_u32(p, requested_server_id);

				if (node->state == Node::State::ClientConnecting)
				{
					// ignore
				}
				else if (node->state == Node::State::Invalid
					|| node->state == Node::State::ClientIdle
					|| node->state == Node::State::ClientWaiting)
				{
					if (requested_server_id == 0) // story mode
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
					node->server_state.id = requested_server_id;
					if (node->save)
						node->server_state.level = node->save->zone_current;

					if (node->state != Node::State::ClientWaiting)
					{
						// add to client waiting list
						clients_waiting.add(node->addr);
						node->state = Node::State::ClientWaiting;
					}
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
								send_client_connect(node->addr, connection.client);
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
						vi_debug("Server %s:%hd connected.", Sock::host_to_str(addr.host), addr.port);
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

	s32 proc()
	{
		mersenne::srand(platform::timestamp());

		if (Sock::init())
			return 1;

		if (Sock::udp_open(&sock, 3497, true))
		{
			fprintf(stderr, "%s\n", Sock::get_error());
			return 1;
		}

		if (!Net::Http::init())
			return 1;

		// get localhost address
		{
			Sock::Address addr;
			Sock::get_address(&addr, "127.0.0.1", 3494);
			localhost_ip = addr.host;
		}

		// open sqlite database
		{
			b8 init_db = false;
			if (platform::filemtime("deceiver.db") == 0)
				init_db = true;
			
			if (sqlite3_open("deceiver.db", &db))
			{
				fprintf(stderr, "Can't open sqlite database: %s", sqlite3_errmsg(db));
				return 1;
			}

			if (init_db)
			{
				db_exec("create table User (id integer primary key autoincrement, token integer not null, token_timestamp integer not null, itch_id integer, steam_id integer, username varchar(256) not null, unique(itch_id), unique(steam_id));");
				db_exec("create table VirtualServer (id integer primary key autoincrement, name text not null, config text not null);");
				db_exec("create table UserServer (user_id not null, server_id not null, timestamp integer not null, is_admin boolean not null, foreign key (user_id) references User(id), foreign key (server_id) references Server(id));");
			}
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
					{
						cJSON* secret = cJSON_GetObjectItem(json, "secret");
						if (secret)
							Settings::secret = secret->valueint;
					}
					{
						cJSON* public_ip = cJSON_GetObjectItem(json, "public_ip");
						if (public_ip)
						{
							Sock::Address addr;
							Sock::get_address(&addr, public_ip->valuestring, 3494);
							Settings::public_ip = addr.host;
						}
						else
							Settings::public_ip = localhost_ip;
					}
					{
						cJSON* itch_api_key = cJSON_GetObjectItem(json, "itch_api_key");
						if (itch_api_key)
							strncpy(Settings::itch_api_key, itch_api_key->valuestring, MAX_AUTH_KEY);
					}
					cJSON_Delete(json);
				}
				else
					fprintf(stderr, "Can't parse json file '%s': %s\n", MASTER_SETTINGS_FILE, cJSON_GetErrorPtr());
				free(data);
			}
		}

		r64 last_audit = 0.0;
		r64 last_match = 0.0;
		r64 last_update = platform::time();

		while (true)
		{
			{
				r64 t = platform::time();
				timestamp += vi_min(0.25, t - last_update);
				last_update = t;
			}

			Net::Http::update();
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
						{
							client->state = Node::State::ClientWaiting; // give up connecting, go back to matchmaking
							clients_waiting.add(client->addr);
						}

						Node* server = node_for_address(c.server);
						if (server)
						{
							// since there are clients connecting to this server, we've been ignoring its updates telling us how many open slots it has
							// we need to manually update the open slot count until the server gives us a fresh count
							// don't try to fill these slots until the server tells us for sure they're available
							server->server_state.open_slots -= c.slots;
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

				s32 idle_servers = 0;
				for (s32 i = 0; i < servers.length; i++)
				{
					Node* server = node_for_address(servers[i]);
					if (server->state == Node::State::ServerIdle)
						idle_servers++;
				}

				for (s32 i = 0; i < clients_waiting.length; i++)
				{
					Node* client = node_add_or_get(clients_waiting[i]);
					Node* server = client_requested_server(client);
					if (server)
					{
						if (server_open_slots(server) >= client->server_state.open_slots)
						{
							client_connect_to_existing_server(client, server);
							i--; // client has been removed from clients_waiting
						}
					}
					else if (idle_servers > 0)
					{
						// allocate an idle server for this client
						Node* idle_server = nullptr;
						for (s32 i = 0; i < servers.length; i++)
						{
							Node* server = node_for_address(servers[i]);
							if (server->state == Node::State::ServerIdle)
							{
								idle_server = server;
								break;
							}
						}
						vi_assert(idle_server);
						idle_servers--;
						send_server_load(idle_server, client);
						send_server_expect_client(idle_server, &client->user_key);
						client_queue_join(idle_server, client);
						i--; // client has been removed from clients_waiting
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
					packet_handle(&packet, addr);
				}
				else
					vi_debug("%s", "Discarding packet due to invalid checksum.");
			}
			else
				platform::sleep(1.0f / 60.0f);
		}

		sqlite3_close(db);

		Net::Http::term();

		return 0;
	}

}

}

}

int main(int argc, char** argv)
{
	return VI::Net::Master::proc();
}
