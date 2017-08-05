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
#include "data/json.h"

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

#define MASTER_AUDIT_INTERVAL 1.12 // remove inactive nodes every x seconds
#define MASTER_MATCH_INTERVAL 0.25 // run matchmaking searches every x seconds
#define MASTER_INACTIVE_THRESHOLD 10.0 // remove node if it's inactive for x seconds
#define MASTER_CLIENT_CONNECTION_TIMEOUT 10.0 // clients have x seconds to connect to a server once we tell them to
#define MASTER_SETTINGS_FILE "config.txt"
#define MASTER_TOKEN_TIMEOUT (86400 * 2)
#define MASTER_SERVER_LOAD_TIMEOUT 10.0

	namespace Settings
	{
		s32 secret;
		u32 public_ip;
		char itch_api_key[MAX_AUTH_KEY + 1];
	}

	r64 timestamp;

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
		r64 state_change_timestamp;
		UserKey user_key;
		Sock::Address addr;
		ServerState server_state;
		State state;

		void transition(State s)
		{
			if (s != state)
			{
				state = s;
				state_change_timestamp = timestamp;
			}
		}
	};

	struct ClientConnection
	{
		r64 timestamp;
		Sock::Address client;
		Sock::Address server;
		s8 slots;
	};

	std::unordered_map<Sock::Address, Node> nodes;
	std::unordered_map<u32, Sock::Address> server_config_map;
	Sock::Handle sock;
	Messenger messenger;
	Array<Sock::Address> servers;
	Array<Sock::Address> clients_waiting;
	Array<ClientConnection> clients_connecting;
	Array<Sock::Address> servers_loading;
	u32 localhost_ip;
	sqlite3* db;

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
		if (sqlite3_bind_text(stmt, index + 1, text, -1, SQLITE_TRANSIENT))
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

	Node* server_for_config_id(u32 id)
	{
		auto i = server_config_map.find(id);
		if (i == server_config_map.end())
			return nullptr;
		else
			return node_for_address(i->second);
	}

	void server_state_for_config_id(u32 id, s8 max_players, ServerState* state, Sock::Address* addr = nullptr)
	{
		Node* server = server_for_config_id(id);
		if (server) // a server is running this config
		{
			*state = server->server_state;
			if (addr)
				*addr = server->addr;
		}
		else // config is not running on any server
		{
			state->id = id;
			state->player_slots = max_players;
			state->level = AssetNull;
			if (addr)
				*addr = {};
		}
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
					client->transition(Node::State::ClientWaiting);
				clients_connecting.remove(i);
				i--;
			}
		}
	}

	void server_config_parse(const char* text, ServerConfig* config)
	{
		// id, name, game_type, team_count, and is_private are stored in DB row, not here

		cJSON* json = cJSON_Parse(text);

		{
			cJSON* levels = cJSON_GetObjectItem(json, "levels");
			cJSON* level = levels->child;
			while (level)
			{
				config->levels.add(level->valueint);
				level = level->next;
			}
		}
		config->kill_limit = s16(Json::get_s32(json, "kill_limit", DEFAULT_ASSAULT_DRONES));
		config->respawns = s16(Json::get_s32(json, "respawns", DEFAULT_ASSAULT_DRONES));
		config->allow_upgrades = s16(Json::get_s32(json, "allow_upgrades", 0xffff));
		{
			cJSON* start_upgrades = cJSON_GetObjectItem(json, "start_upgrades");
			cJSON* u = start_upgrades->child;
			while (u)
			{
				config->start_upgrades.add(Upgrade(u->valueint));
				u = u->next;
			}
		}
		config->max_players = s8(Json::get_s32(json, "max_players", 4));
		config->min_players = s8(Json::get_s32(json, "min_players", 2));
		config->time_limit_minutes = s8(Json::get_s32(json, "time_limit_minutes", 6));
		config->enable_minions = b8(Json::get_s32(json, "enable_minions", 1));
		config->drone_shield = s8(Json::get_s32(json, "drone_shield", DRONE_SHIELD));
		config->start_energy = b8(Json::get_s32(json, "start_energy"));
		config->fill_bots = b8(Json::get_s32(json, "fill_bots"));
		cJSON_Delete(json);
	}

	// caller must free() the returned string
	char* server_config_stringify(const ServerConfig& config)
	{
		// id, name, game_type, team_count, and is_private are stored in DB row, not here

		cJSON* json = cJSON_CreateObject();

		{
			cJSON* levels = cJSON_CreateArray();
			cJSON_AddItemToObject(json, "levels", levels);
			for (s32 i = 0; i < config.levels.length; i++)
				cJSON_AddItemToArray(levels, cJSON_CreateNumber(config.levels[i]));
		}
		cJSON_AddNumberToObject(json, "kill_limit", config.kill_limit);
		cJSON_AddNumberToObject(json, "respawns", config.respawns);
		cJSON_AddNumberToObject(json, "allow_upgrades", config.allow_upgrades);
		{
			cJSON* start_upgrades = cJSON_CreateArray();
			cJSON_AddItemToObject(json, "start_upgrades", start_upgrades);
			for (s32 i = 0; i < config.start_upgrades.length; i++)
				cJSON_AddItemToArray(start_upgrades, cJSON_CreateNumber(s32(config.start_upgrades[i])));
		}
		cJSON_AddNumberToObject(json, "max_players", config.max_players);
		cJSON_AddNumberToObject(json, "min_players", config.min_players);
		cJSON_AddNumberToObject(json, "time_limit_minutes", config.time_limit_minutes);
		cJSON_AddNumberToObject(json, "enable_minions", config.enable_minions);
		cJSON_AddNumberToObject(json, "drone_shield", config.drone_shield);
		cJSON_AddNumberToObject(json, "start_energy", config.start_energy);
		cJSON_AddNumberToObject(json, "fill_bots", config.fill_bots);

		char* result = cJSON_Print(json);
		cJSON_Delete(json);
		return result; // caller must free() it
	}

	b8 server_config_get(u32 id, ServerConfig* config)
	{
		config->id = id;
		vi_assert(id > 0);
		sqlite3_stmt* stmt = db_query("select name, config, max_players, team_count, game_type, is_private from ServerConfig where id=?;");
		db_bind_int(stmt, 0, id);
		b8 found = false;
		if (db_step(stmt))
		{
			memset(config->name, 0, sizeof(config->name));
			strncpy(config->name, (const char*)db_column_text(stmt, 0), MAX_SERVER_CONFIG_NAME);
			server_config_parse((const char*)db_column_text(stmt, 1), config);
			config->max_players = db_column_int(stmt, 2);
			config->team_count = db_column_int(stmt, 3);
			config->game_type = GameType(db_column_int(stmt, 4));
			config->is_private = b8(db_column_int(stmt, 5));
			found = true;
		}
		db_finalize(stmt);
		return found;
	}

	b8 user_is_admin(u32 user_id, u32 config_id)
	{
		sqlite3_stmt* stmt = db_query("select is_admin from UserServer where user_id=? and server_id=? limit 1;");
		db_bind_int(stmt, 0, user_id);
		db_bind_int(stmt, 1, config_id);
		if (db_step(stmt))
			return b8(db_column_int(stmt, 0));
		else
			return false;
	}

	b8 server_details_get(u32 config_id, u32 user_id, ServerDetails* details)
	{
		if (server_config_get(config_id, &details->config))
		{
			server_state_for_config_id(config_id, details->config.max_players, &details->state, &details->addr);
			details->is_admin = user_is_admin(user_id, config_id);
			return true;
		}
		return false;
	}

	b8 client_desired_server_config(Node* client, ServerConfig* config)
	{
		if (client->server_state.id == 0)
		{
			config->id = 0; // story mode
			return true;
		}
		else
			return server_config_get(client->server_state.id, config);
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

	b8 send_reauth_required(Sock::Address addr)
	{
		using Stream = StreamWrite;
		StreamWrite p;
		packet_init(&p);
		messenger.add_header(&p, addr, Message::ReauthRequired);
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
			if (node->server_state.id)
				server_config_map.erase(node->server_state.id);

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

	// returns true if the user is an admin of the given server
	b8 update_user_server_linkage(u32 user_id, u32 server_id, b8 make_admin = false)
	{
		b8 is_admin = false;

		// this is a custom ServerConfig; find out if the user is an admin of it
		sqlite3_stmt* stmt = db_query("select is_admin from UserServer where user_id=? and server_id=? limit 1;");
		db_bind_int(stmt, 0, user_id);
		db_bind_int(stmt, 1, server_id);
		if (db_step(stmt))
		{
			is_admin = b8(db_column_int(stmt, 0));
			if (make_admin)
				is_admin = true;

			// update existing linkage
			{
				sqlite3_stmt* stmt = db_query("update UserServer set timestamp=?, is_admin=? where user_id=? and server_id=?;");
				db_bind_int(stmt, 0, platform::timestamp());
				db_bind_int(stmt, 1, is_admin);
				db_bind_int(stmt, 2, user_id);
				db_bind_int(stmt, 3, server_id);
				db_exec(stmt);
			}
		}
		else
		{
			// add new linkage
			if (make_admin)
				is_admin = true;
			sqlite3_stmt* stmt = db_query("insert into UserServer (timestamp, user_id, server_id, is_admin) values (?, ?, ?, ?);");
			db_bind_int(stmt, 0, platform::timestamp());
			db_bind_int(stmt, 1, user_id);
			db_bind_int(stmt, 2, server_id);
			db_bind_int(stmt, 3, is_admin);
			db_exec(stmt);
		}
		db_finalize(stmt);

		return is_admin;
	}

	b8 send_server_expect_client(Node* server, UserKey* user)
	{
		using Stream = StreamWrite;

		b8 is_admin = false;
		if (server->server_state.id != 0) // 0 = story mode
			is_admin = update_user_server_linkage(user->id, server->server_state.id);

		StreamWrite p;
		packet_init(&p);
		messenger.add_header(&p, server->addr, Message::ExpectClient);
		serialize_u32(&p, user->id);
		serialize_u32(&p, user->token);
		serialize_bool(&p, is_admin);
		packet_finalize(&p);
		messenger.send(p, timestamp, server->addr, &sock);
		return true;
	}

	b8 send_server_load(Node* server, Node* client)
	{
		using Stream = StreamWrite;

		server->transition(Node::State::ServerLoading);
		server->server_state.id = client->server_state.id;
		server->server_state.level = client->server_state.level;

		StreamWrite p;
		packet_init(&p);
		messenger.add_header(&p, server->addr, Message::ServerLoad);
		
		ServerConfig config;
		if (client_desired_server_config(client, &config))
		{
			if (!serialize_server_config(&p, &config))
				net_error();
		}
		else
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

	b8 send_server_config_saved(Node* client, u32 config_id, u32 request_id)
	{
		using Stream = StreamWrite;
		StreamWrite p;
		packet_init(&p);
		messenger.add_header(&p, client->addr, Message::ServerConfigSaved);
		serialize_u32(&p, config_id);
		serialize_u32(&p, request_id);
		packet_finalize(&p);
		messenger.send(p, timestamp, client->addr, &sock);
		return true;
	}

	b8 send_server_config(const Sock::Address& addr, const ServerConfig& config, u32 request_id = 0)
	{
		using Stream = StreamWrite;
		StreamWrite p;
		packet_init(&p);
		messenger.add_header(&p, addr, Message::ServerConfig);

		serialize_u32(&p, request_id);

		{
			ServerConfig c = config;
			if (!serialize_server_config(&p, &c))
				net_error();
		}

		packet_finalize(&p);
		messenger.send(p, timestamp, addr, &sock);
		return true;
	}

	b8 send_server_details(const Sock::Address& addr, const ServerDetails& details, u32 request_id)
	{
		using Stream = StreamWrite;
		StreamWrite p;
		packet_init(&p);
		messenger.add_header(&p, addr, Message::ServerDetails);

		serialize_u32(&p, request_id);

		{
			ServerDetails d = details;
			if (!serialize_server_details(&p, &d))
				net_error();
		}

		packet_finalize(&p);
		messenger.send(p, timestamp, addr, &sock);
		return true;
	}

	sqlite3_stmt* server_list_query(Node* client, ServerListType type, s32 offset)
	{
		sqlite3_stmt* stmt;
		switch (type)
		{
			case ServerListType::Top:
			{
				stmt = db_query("select id, name, max_players, team_count, game_type from ServerConfig where is_private=0 limit ?,24");
				db_bind_int(stmt, 0, offset);
				break;
			}
			case ServerListType::Recent:
			{
				stmt = db_query("select ServerConfig.id, ServerConfig.name, ServerConfig.max_players, ServerConfig.team_count, ServerConfig.game_type from ServerConfig inner join UserServer on UserServer.server_id=ServerConfig.id where UserServer.user_id=? order by UserServer.timestamp desc limit ?,24");
				db_bind_int(stmt, 0, client->user_key.id);
				db_bind_int(stmt, 1, offset);
				break;
			}
			case ServerListType::Mine:
			{
				stmt = db_query("select ServerConfig.id, ServerConfig.name, ServerConfig.max_players, ServerConfig.team_count, ServerConfig.game_type from ServerConfig inner join UserServer on UserServer.server_id=ServerConfig.id where UserServer.user_id=? and UserServer.is_admin=1 order by UserServer.timestamp desc limit ?,24");
				db_bind_int(stmt, 0, client->user_key.id);
				db_bind_int(stmt, 1, offset);
				break;
			}
			default:
				vi_assert(false);
				break;
		}

		return stmt;
	}

	b8 send_server_list_fragment(Node* client, ServerListType type, sqlite3_stmt* stmt, s32* offset, b8* done)
	{
		using Stream = Net::StreamWrite;

		Net::StreamWrite p;
		packet_init(&p);
		messenger.add_header(&p, client->addr, Message::ServerList);

		serialize_enum(&p, ServerListType, type);

		*done = false;
		s32 count = 0;
		while (true)
		{
			if (!db_step(stmt))
			{
				*done = true;
				break;
			}
			serialize_s32(&p, *offset);

			ServerListEntry entry;

			u32 id = db_column_int(stmt, 0);
			strncpy(entry.name, (const char*)db_column_text(stmt, 1), MAX_SERVER_CONFIG_NAME);
			entry.max_players = db_column_int(stmt, 2);
			entry.team_count = db_column_int(stmt, 3);
			entry.game_type = GameType(db_column_int(stmt, 4));

			server_state_for_config_id(id, entry.max_players, &entry.server_state, &entry.addr);

			if (!serialize_server_list_entry(&p, &entry))
				net_error();

			(*offset)++;

			if (count == MAX_SERVER_LIST)
				break;
			count++;
		}

		{
			s32 index = -1;
			serialize_s32(&p, index); // done
		}

		packet_finalize(&p);
		messenger.send(p, timestamp, client->addr, &sock);
		return true;
	}

	b8 send_server_list(Node* client, ServerListType type, s32 offset)
	{
		offset = vi_max(offset - 12, 0);
		sqlite3_stmt* stmt = server_list_query(client, type, offset);
		while (true)
		{
			b8 done;
			send_server_list_fragment(client, type, stmt, &offset, &done);
			if (done)
				break;
		}
		db_finalize(stmt);
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
		return server->server_state.player_slots - server_client_slots_connecting(server);
	}

	void server_update_state(Node* server, const ServerState& s)
	{
		if (server->state == Node::State::Invalid)
		{
			// add to server list
			vi_debug("Server %s:%hd connected.", Sock::host_to_str(server->addr.host), server->addr.port);
			servers.add(server->addr);
		}

		server->transition(s.level == AssetNull ? Node::State::ServerIdle : Node::State::ServerActive);

		if (server->server_state.id && s.level == AssetNull)
			server_config_map.erase(server->server_state.id);

		if (s.id)
		{
			vi_assert(s.level != AssetNull);
			server_config_map[s.id] = server->addr;
		}

		s8 original_open_slots = server->server_state.player_slots;
		server->server_state = s;
		s8 clients_connecting_count = server_client_slots_connecting(server);
		if (clients_connecting_count > 0)
		{
			// there are clients trying to connect to this server
			// check if the server has registered that these clients have connected
			if (s.player_slots <= server->server_state.player_slots - clients_connecting_count)
			{
				// the clients have connected, all's well
				// remove ClientConnection records
				server_remove_clients_connecting(server);
			}
			else // clients have not connected yet, maintain old slot count
				server->server_state.player_slots = original_open_slots;
		}
	}

	b8 check_user_key(StreamRead* p, Node* node)
	{
		using Stream = Net::StreamRead;
		UserKey key;
		serialize_u32(p, key.id);
		serialize_u32(p, key.token);

		if (key.id != 0)
		{
			if (node->user_key.equals(key)) // already authenticated
				return true;
			else if (node->user_key.id == 0 && node->user_key.token == 0) // user hasn't been authenticated yet
			{
				sqlite3_stmt* stmt = db_query("select token, token_timestamp from User where id=? limit 1;");
				db_bind_int(stmt, 0, key.id);
				if (db_step(stmt))
				{
					s64 token = db_column_int(stmt, 0);
					s64 token_timestamp = db_column_int(stmt, 1);
					if (u32(token) == key.token && platform::timestamp() - token_timestamp < MASTER_TOKEN_TIMEOUT)
					{
						node->user_key = key;
						return true;
					}
				}
			}
		}

		send_reauth_required(node->addr);
		return false;
	}

	Node* client_requested_server(Node* client)
	{
		// if the user is requesting to connect to a virtual server that is already active, they can connect immediately
		if (!client->save)
		{
			vi_assert(client->server_state.id);
			return server_for_config_id(client->server_state.id);
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
		connection->slots = client->server_state.player_slots;

		client->transition(Node::State::ClientConnecting);
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
		messenger.received(type, seq, addr, &sock);

		Node* node = node_add_or_get(addr);
		node->last_message_timestamp = timestamp;

		switch (type)
		{
			case Message::Ack:
			case Message::Keepalive:
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
						void* user_data = *reinterpret_cast<void**>(&node->addr);
						if (!Http::request_for_user_data(user_data)) // make sure we're not already trying to authenticate this user
						{
							char header[MAX_PATH_LENGTH + 1] = {};
							snprintf(header, MAX_PATH_LENGTH, "Authorization: %s", auth_key);
							Http::get("https://itch.io/api/1/jwt/me", &itch_auth_callback, header, user_data);
						}
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
				if (!check_user_key(p, node))
					return false;

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
					else
					{
						// check if requested config exists
						sqlite3_stmt* stmt = db_query("select count(1) from ServerConfig where id=? limit 1;");
						db_bind_int(stmt, 0, requested_server_id);
						db_step(stmt);
						s64 count = db_column_int(stmt, 0);
						db_finalize(stmt);
						if (count == 0)
							net_error();
					}
					node->server_state.id = requested_server_id;
					if (node->save)
						node->server_state.level = node->save->zone_current;

					if (node->state != Node::State::ClientWaiting)
					{
						// add to client waiting list
						clients_waiting.add(node->addr);
						node->transition(Node::State::ClientWaiting);
					}
				}
				else // invalid state transition
					net_error();
				break;
			}
			case Message::RequestServerConfig:
			{
				if (!check_user_key(p, node))
					return false;

				u32 request_id;
				serialize_u32(p, request_id);

				u32 config_id;
				serialize_u32(p, config_id);

				if (config_id == 0) // story mode
					net_error();
				else
				{
					ServerConfig config;
					if (server_config_get(config_id, &config))
						send_server_config(node->addr, config, request_id);
					else
						net_error();
				}

				break;
			}
			case Message::ClientRequestServerDetails:
			{
				if (!check_user_key(p, node))
					return false;

				u32 request_id;
				serialize_u32(p, request_id);

				u32 config_id;
				serialize_u32(p, config_id);

				if (config_id == 0) // story mode
					net_error();
				else
				{
					ServerDetails details;
					if (server_details_get(config_id, node->user_key.id, &details))
						send_server_details(node->addr, details, request_id);
					else
						net_error();
				}

				break;
			}
			case Message::ClientSaveServerConfig:
			{
				if (!check_user_key(p, node))
					return false;

				u32 request_id;
				serialize_u32(p, request_id);
				b8 create_new;
				serialize_bool(p, create_new);
				ServerConfig config;
				if (!serialize_server_config(p, &config))
					net_error();

				u32 config_id;
				if (create_new)
				{
					// create config in db
					sqlite3_stmt* stmt = db_query("insert into ServerConfig (creator_id, name, config, max_players, team_count, game_type, is_private) values (?, ?, ?, ?, ?, ?, ?);");
					db_bind_int(stmt, 0, node->user_key.id);
					db_bind_text(stmt, 1, config.name);
					char* json = server_config_stringify(config);
					db_bind_text(stmt, 2, json);
					free(json);
					db_bind_int(stmt, 3, config.max_players);
					db_bind_int(stmt, 4, config.team_count);
					db_bind_int(stmt, 5, config.is_private);
					db_bind_int(stmt, 6, s32(config.game_type));
					config_id = db_exec(stmt);
				}
				else
				{
					// updating an existing config
					// check if the user actually has privileges to edit it
					if (!user_is_admin(node->user_key.id, config.id))
						net_error();
					sqlite3_stmt* stmt = db_query("update ServerConfig set name=?, config=?, max_players=?, team_count=?, game_type=?, is_private=? where id=?;");
					db_bind_text(stmt, 0, config.name);
					char* json = server_config_stringify(config);
					db_bind_text(stmt, 1, json);
					free(json);
					db_bind_int(stmt, 2, config.max_players);
					db_bind_int(stmt, 3, config.team_count);
					db_bind_int(stmt, 4, s32(config.game_type));
					db_bind_int(stmt, 5, config.is_private);
					db_bind_int(stmt, 6, config.id);
					db_exec(stmt);
					config_id = config.id;
				}

				update_user_server_linkage(node->user_key.id, config_id, true); // true = make them an admin

				Node* server = server_for_config_id(config_id);
				if (server)
					send_server_config(server->addr, config);

				send_server_config_saved(node, config_id, request_id);

				break;
			}
			case Message::ClientRequestServerList:
			{
				if (!check_user_key(p, node))
					return false;

				ServerListType type;
				serialize_enum(p, ServerListType, type);
				s32 offset;
				serialize_s32(p, offset);
				if (offset < 0)
					return false;

				send_server_list(node, type, offset);
				break;
			}
			case Message::ServerStatusUpdate:
			{
				s32 secret;
				serialize_s32(p, secret);
				if (secret != Settings::secret)
					net_error();
				ServerState s;
				if (!serialize_server_state(p, &s))
					net_error();
				if (node->state == Node::State::ServerLoading)
				{
					if (node->state_change_timestamp > timestamp - MASTER_SERVER_LOAD_TIMEOUT)
					{
						if (s.id == node->server_state.id) // done loading
						{
							server_update_state(node, s);

							// tell clients to connect to this server
							for (s32 i = 0; i < clients_connecting.length; i++)
							{
								const ClientConnection& connection = clients_connecting[i];
								if (connection.server.equals(node->addr))
									send_client_connect(node->addr, connection.client);
							}
						}
					}
					else
					{
						// load timed out
						server_update_state(node, s);
					}
				}
				else if (node->state == Node::State::Invalid
					|| node->state == Node::State::ServerActive
					|| node->state == Node::State::ServerIdle)
				{
					server_update_state(node, s);
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
				db_exec("create table ServerConfig (id integer primary key autoincrement, creator_id integer not null, name text not null, config text, max_players integer not null, team_count integer not null, game_type integer not null, is_private boolean not null, foreign key (creator_id) references User(id));");
				db_exec("create table UserServer (user_id not null, server_id not null, timestamp integer not null, is_admin boolean not null, foreign key (user_id) references User(id), foreign key (server_id) references ServerConfig(id));");
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
					Settings::secret = Json::get_s32(json, "secret");
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
							client->transition(Node::State::ClientWaiting); // give up connecting, go back to matchmaking
							clients_waiting.add(client->addr);
						}

						Node* server = node_for_address(c.server);
						if (server)
						{
							// since there are clients connecting to this server, we've been ignoring its updates telling us how many open slots it has
							// we need to manually update the open slot count until the server gives us a fresh count
							// don't try to fill these slots until the server tells us for sure they're available
							server->server_state.player_slots -= c.slots;
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
						if (server_open_slots(server) >= client->server_state.player_slots)
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
