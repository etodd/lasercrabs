#define _AMD64_

#include "http.h"
#include "curl/curl.h"
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
#include "mersenne/mersenne-twister.h"
#include "data/json.h"
#include <cmath>
#include "mongoose/mongoose.h"

#if RELEASE_BUILD
#define OFFLINE_DEV 0
#else
#define OFFLINE_DEV 1
#endif
#define AUTHENTICATE_DOWNLOAD_KEYS 0
#define CRASH_DUMP_DIR "crash_dumps/"

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

	void vi_sleep(r32 time)
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
	s64 filemtime(const char* file)
	{
		struct stat st;
		if (stat(file, &st))
			return 0;
		return st.st_mtime;
	}
#endif

	b8 file_exists(const char* file)
	{
		return filemtime(file) != 0;
	}

}

namespace Net
{

namespace CrashReport
{


mg_mgr mgr;
mg_connection* conn_ipv4;
mg_connection* conn_ipv6;

struct mg_str upload_filename(mg_connection* nc, mg_str fname)
{
	if (fname.len == 0)
		return { nullptr, 0 }; // not cool

	s32 i = 0;
	while (char c = fname.p[i])
	{
		if ((c < 'A' || c > 'Z')
			&& (c < 'a' || c > 'z')
			&& (c < '0' || c > '9')
			&& c != '-'
			&& c != '.'
			&& c != '_')
			return { nullptr, 0 }; // not cool
		i++;
	}

	mg_str result = { nullptr, 0 };
	s32 try_index = 0;
	while (!result.p || platform::file_exists(result.p))
	{
		if (result.p)
			free((void*)(result.p));

		char suffix[32];
		if (try_index > 0)
			sprintf(suffix, "%d", try_index);
		else
			suffix[0] = '\0';

		result.len = strlen(CRASH_DUMP_DIR) + fname.len + strlen(suffix);
		char* full_path = (char*)(calloc(result.len + 1, sizeof(char)));
		sprintf(full_path, "%s%s%s", CRASH_DUMP_DIR, fname.p, suffix);
		result.p = full_path;
		try_index++;
	}
	return result;
}

void ev_handler(mg_connection* nc, int ev, void* ev_data)
{
	switch (ev)
	{
		case MG_EV_HTTP_REQUEST:
		{
			mg_printf
			(
				nc, "%s",
				"HTTP/1.1 403 Forbidden\r\n"
				"Content-Type: text/html\r\n"
				"Connection: close\r\n"
				"\r\n"
				"Forbidden"
			);
			nc->flags |= MG_F_SEND_AND_CLOSE;
			break;
		}
	}
}

void handle_upload(mg_connection* nc, int ev, void* p)
{
	switch (ev)
	{
		case MG_EV_HTTP_PART_BEGIN:
		case MG_EV_HTTP_PART_DATA:
		case MG_EV_HTTP_PART_END:
			mg_file_upload_handler(nc, ev, p, upload_filename);
	}
}

void init()
{
	mg_mgr_init(&mgr, nullptr);
	{
		char addr[32];
		sprintf(addr, "0.0.0.0:%d", NET_MASTER_HTTP_PORT);
		conn_ipv4 = mg_bind(&mgr, addr, ev_handler);

		sprintf(addr, "[::]:%d", NET_MASTER_HTTP_PORT);
		conn_ipv6 = mg_bind(&mgr, addr, ev_handler);
	}

	if (conn_ipv4)
	{
		mg_set_protocol_http_websocket(conn_ipv4);
		mg_register_http_endpoint(conn_ipv4, "/crash_dump", handle_upload);
		printf("Bound to 0.0.0.0:%d\n", NET_MASTER_HTTP_PORT);
	}

	if (conn_ipv6)
	{
		mg_set_protocol_http_websocket(conn_ipv6);
		mg_register_http_endpoint(conn_ipv6, "/crash_dump", handle_upload);
		printf("Bound to [::]:%d\n", NET_MASTER_HTTP_PORT);
	}

	vi_assert(conn_ipv4 || conn_ipv6);
}

void update()
{
	mg_mgr_poll(&mgr, 0);
}

void term()
{
	mg_mgr_free(&mgr);
}


}


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

		r64 last_message_timestamp;
		r64 state_change_timestamp;
		UserKey user_key;
		Sock::Address addr;
		Sock::Address public_ipv4;
		Sock::Address public_ipv6;
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

	std::unordered_map<u64, Node> nodes;
	std::unordered_map<u32, Sock::Address> server_config_map;
	Sock::Handle sock;
	Messenger messenger;
	Array<u64> servers;
	Array<u64> clients_waiting;
	Array<ClientConnection> clients_connecting;
	Array<u64> servers_loading;
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

	Sock::Address server_public_ip(Node* server, Sock::Host::Type type)
	{
		if (type == Sock::Host::Type::IPv4) // prefer ipv4
			return server->public_ipv4.port ? server->public_ipv4 : server->public_ipv6;
		else // prefer ipv6
			return server->public_ipv6.port ? server->public_ipv6 : server->public_ipv4;
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

	const char* db_column_text(sqlite3_stmt* stmt, s32 index)
	{
		return (const char*)sqlite3_column_text(stmt, index);
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

	Node* node_add_or_get(const Sock::Address& addr)
	{
		u64 hash = addr.hash();
		auto i = nodes.find(hash);
		Node* n;
		if (i == nodes.end())
		{
			auto i = nodes.insert(std::pair<u64, Node>(hash, Node()));
			n = &i.first->second;
			n->addr = addr;
		}
		else
			n = &i->second;
		return n;
	}

	Node* node_for_hash(u64 hash)
	{
		auto i = nodes.find(hash);
		if (i == nodes.end())
			return nullptr;
		else
			return &i->second;
	}

	Node* node_for_address(const Sock::Address& addr)
	{
		return node_for_hash(addr.hash());
	}

	Node* server_for_config_id(u32 id)
	{
		auto i = server_config_map.find(id);
		if (i == server_config_map.end())
			return nullptr;
		else
			return node_for_address(i->second);
	}

	void server_state_for_config_id(u32 id, s8 max_players, ServerState* state, Sock::Host::Type addr_type = Sock::Host::Type::IPv4, Sock::Address* addr = nullptr)
	{
		Node* server = server_for_config_id(id);
		if (server) // a server is running this config
		{
			*state = server->server_state;
			if (addr)
				*addr = server_public_ip(server, addr_type);
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
		config->enable_batteries = b8(Json::get_s32(json, "enable_batteries", 1));
		config->enable_battery_stealth = b8(Json::get_s32(json, "enable_battery_stealth", 1));
		config->drone_shield = s8(Json::get_s32(json, "drone_shield", DRONE_SHIELD_AMOUNT));
		config->start_energy = s16(Json::get_s32(json, "start_energy"));
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
		cJSON_AddNumberToObject(json, "enable_batteries", config.enable_batteries);
		cJSON_AddNumberToObject(json, "enable_battery_stealth", config.enable_battery_stealth);
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
		sqlite3_stmt* stmt = db_query("select name, config, max_players, team_count, game_type, creator_id, is_private, region from ServerConfig where id=?;");
		db_bind_int(stmt, 0, id);
		b8 found = false;
		if (db_step(stmt))
		{
			memset(config->name, 0, sizeof(config->name));
			strncpy(config->name, db_column_text(stmt, 0), MAX_SERVER_CONFIG_NAME);
			server_config_parse(db_column_text(stmt, 1), config);
			config->max_players = db_column_int(stmt, 2);
			config->team_count = db_column_int(stmt, 3);
			config->game_type = GameType(db_column_int(stmt, 4));
			config->creator_id = db_column_int(stmt, 5);
			config->is_private = b8(db_column_int(stmt, 6));
			config->region = Region(db_column_int(stmt, 7));
			found = true;
		}
		db_finalize(stmt);
		return found;
	}

	Role user_role(u32 user_id, u32 config_id)
	{
		sqlite3_stmt* stmt = db_query("select role from UserServer where user_id=? and server_id=? limit 1;");
		db_bind_int(stmt, 0, user_id);
		db_bind_int(stmt, 1, config_id);
		if (db_step(stmt))
			return Role(db_column_int(stmt, 0));
		else
			return Role::None;
	}

	void username_get(u32 user_id, char* username)
	{
		sqlite3_stmt* stmt = db_query("select username from User where id=? limit 1;");
		db_bind_int(stmt, 0, user_id);
		if (db_step(stmt))
			strncpy(username, db_column_text(stmt, 0), MAX_USERNAME);
		else
			username[0] = '\0';
	}

	b8 server_details_get(u32 config_id, u32 user_id, ServerDetails* details, Sock::Host::Type addr_type)
	{
		if (server_config_get(config_id, &details->config))
		{
			Role role = user_role(user_id, config_id);
			if (role == Role::Banned || (role == Role::None && details->config.is_private))
				return false;

			details->is_admin = role == Role::Admin;
			server_state_for_config_id(config_id, details->config.max_players, &details->state, addr_type, &details->addr);
			username_get(details->config.creator_id, details->creator_username);
			return true;
		}
		return false;
	}

	b8 client_desired_server_config(Node* client, ServerConfig* config)
	{
		if (client->server_state.id == 0)
		{
			// story mode
			config->id = 0;
			config->region = client->server_state.region;
			return true;
		}
		else
			return server_config_get(client->server_state.id, config);
	}

	b8 send_auth_response(const Sock::Address& addr, UserKey* key, const char* username)
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
			s32 username_length = vi_min(MAX_USERNAME, s32(strlen(username)));
			serialize_int(&p, s32, username_length, 0, MAX_USERNAME);
			serialize_bytes(&p, (u8*)username, username_length);
		}
		packet_finalize(&p);
		messenger.send(p, timestamp, addr, &sock);
		return true;
	}

	b8 send_reauth_required(const Sock::Address& addr)
	{
		using Stream = StreamWrite;
		StreamWrite p;
		packet_init(&p);
		messenger.add_header(&p, addr, Message::ReauthRequired);
		packet_finalize(&p);
		messenger.send(p, timestamp, addr, &sock);
		return true;
	}

	void itch_auth_result(u64 addr_hash, b8 success, s64 itch_id, const char* username)
	{
		Node* node = node_for_hash(addr_hash);
		if (node)
		{
			if (success)
			{
				UserKey key;
				key.token = u32(mersenne::rand());

				// save user in database

				sqlite3_stmt* stmt = db_query("select id, banned from User where itch_id=? limit 1;");
				db_bind_int(stmt, 0, itch_id);
				if (db_step(stmt))
				{
					key.id = s32(db_column_int(stmt, 0));
					b8 banned = b8(db_column_int(stmt, 1));
					if (banned)
						success = false;
					else
					{
						// update existing user
						sqlite3_stmt* stmt = db_query("update User set token=?, token_timestamp=?, username=? where id=?;");
						db_bind_int(stmt, 0, key.token);
						db_bind_int(stmt, 1, platform::timestamp());
						db_bind_text(stmt, 2, username);
						db_bind_int(stmt, 3, key.id);
						db_exec(stmt);
					}
				}
				else
				{
					// insert new user
					sqlite3_stmt* stmt = db_query("insert into User (token, token_timestamp, itch_id, username, banned) values (?, ?, ?, ?, 0);");
					db_bind_int(stmt, 0, key.token);
					db_bind_int(stmt, 1, platform::timestamp());
					db_bind_int(stmt, 2, itch_id);
					db_bind_text(stmt, 3, username);
					key.id = s32(db_exec(stmt));
				}
				db_finalize(stmt);

				if (success)
				{
					send_auth_response(node->addr, &key, username);
					return;
				}
			}
			else
			{
#if OFFLINE_DEV
				// log in to the first user in the database
				UserKey key;
				key.token = u32(mersenne::rand());
				sqlite3_stmt* stmt = db_query("select id, banned, username from User order by random() limit 1;");
				if (db_step(stmt))
				{
					key.id = s32(db_column_int(stmt, 0));
					b8 banned = b8(db_column_int(stmt, 1));
					const char* username = db_column_text(stmt, 2);
					if (!banned)
					{
						// update existing user
						sqlite3_stmt* stmt = db_query("update User set token=?, token_timestamp=? where id=?;");
						db_bind_int(stmt, 0, key.token);
						db_bind_int(stmt, 1, platform::timestamp());
						db_bind_int(stmt, 2, key.id);
						db_exec(stmt);

						send_auth_response(node->addr, &key, username);
						return;
					}
				}
#endif
				send_auth_response(node->addr, nullptr, nullptr);
			}
		}
	}

	void itch_download_key_callback(s32 code, const char* data, u64 user_data)
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
			itch_auth_result(user_data, false, 1, nullptr);
	}

	void itch_auth_callback(s32 code, const char* data, u64 user_data)
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

#if AUTHENTICATE_DOWNLOAD_KEYS
						char url[MAX_PATH_LENGTH + 1] = {};
						snprintf(url, MAX_PATH_LENGTH, "https://itch.io/api/1/%s/game/65651/download_keys?user_id=%d", Settings::itch_api_key, id->valueint);
						Http::get(url, &itch_download_key_callback, nullptr, user_data);
#else
						const char* username = cJSON_GetObjectItem(user, "username")->valuestring;
						itch_auth_result(user_data, true, id->valuedouble, username);
#endif
					}
				}
				cJSON_Delete(json);
			}
		}

		if (!success)
			itch_auth_result(user_data, false, 0, nullptr);
	}

	void db_set_server_online(u32 id, b8 online)
	{
		sqlite3_stmt* stmt = db_query("update ServerConfig set online=? where id=?;");
		db_bind_int(stmt, 0, online);
		db_bind_int(stmt, 1, id);
		db_exec(stmt);
	}

	void disconnected(const Sock::Address& addr)
	{
		Node* node = node_for_address(addr);
		if (node->state == Node::State::ServerActive
			|| node->state == Node::State::ServerLoading
			|| node->state == Node::State::ServerIdle)
		{
			// it's a server; remove from the server list
			{
				char addr_str[NET_MAX_ADDRESS];
				addr.str(addr_str);
				vi_debug("Server %s disconnected.", addr_str);
			}
			if (node->server_state.id)
			{
				db_set_server_online(node->server_state.id, false);
				server_config_map.erase(node->server_state.id);
			}

			{
				u64 hash = addr.hash();
				for (s32 i = 0; i < servers.length; i++)
				{
					if (servers[i] == hash)
					{
						servers.remove(i);
						i--;
					}
				}
			}

			// reset any clients trying to connect to this server
			server_remove_clients_connecting(node);
		}
		else if (node->state == Node::State::ClientWaiting)
		{
			// it's a client waiting for a server; remove it from the wait list
			{
				u64 hash = addr.hash();
				for (s32 i = 0; i < clients_waiting.length; i++)
				{
					if (clients_waiting[i] == hash)
					{
						clients_waiting.remove(i);
						i--;
					}
				}
			}
		}
		nodes.erase(addr.hash());
		messenger.remove(addr);
	}

	s64 server_config_score(s64 plays, s64 last_played)
	{
		const s64 epoch = 1503678420LL;
		return s64(log10(vi_max(s64(1), plays))) + ((last_played - epoch) / 45000LL);
	}

	// returns the user's role in the given server
	Role update_user_server_linkage(u32 user_id, u32 server_id, Role assign_role = Role::None)
	{
		Role role = Role::None;

		{
			// this is a custom ServerConfig; find out if the user is an admin of it
			sqlite3_stmt* stmt = db_query("select role from UserServer where user_id=? and server_id=? limit 1;");
			db_bind_int(stmt, 0, user_id);
			db_bind_int(stmt, 1, server_id);
			if (db_step(stmt))
			{
				role = Role(db_column_int(stmt, 0));
				if (assign_role != Role::None)
					role = assign_role;

				// update existing linkage
				{
					sqlite3_stmt* stmt = db_query("update UserServer set timestamp=?, role=? where user_id=? and server_id=?;");
					db_bind_int(stmt, 0, platform::timestamp());
					db_bind_int(stmt, 1, s64(role));
					db_bind_int(stmt, 2, user_id);
					db_bind_int(stmt, 3, server_id);
					db_exec(stmt);
				}
			}
			else
			{
				// add new linkage
				if (assign_role != Role::None)
					role = assign_role;
				sqlite3_stmt* stmt = db_query("insert into UserServer (timestamp, user_id, server_id, role) values (?, ?, ?, ?);");
				db_bind_int(stmt, 0, platform::timestamp());
				db_bind_int(stmt, 1, user_id);
				db_bind_int(stmt, 2, server_id);
				db_bind_int(stmt, 3, s64(role));
				db_exec(stmt);
			}
			db_finalize(stmt);
		}

		if (assign_role == Role::None)
		{
			// the user is just playing on this server; update the server's play count and score
			s64 plays;
			{
				sqlite3_stmt* stmt = db_query("select plays from ServerConfig where id=?;");
				db_bind_int(stmt, 0, server_id);
				{
					b8 success = db_step(stmt);
					vi_assert(success);
				}
				plays = db_column_int(stmt, 0) + 1;
				db_finalize(stmt);
			}

			{
				sqlite3_stmt* stmt = db_query("update ServerConfig set plays=?, score=? where id=?;");
				db_bind_int(stmt, 0, plays);
				db_bind_int(stmt, 0, server_config_score(plays, platform::timestamp()));
				db_exec(stmt);
			}
		}

		return role;
	}

	b8 send_server_expect_client(Node* server, UserKey* user)
	{
		using Stream = StreamWrite;

		Role role = Role::None;
		if (server->server_state.id != 0) // 0 = story mode
			role = update_user_server_linkage(user->id, server->server_state.id);

		StreamWrite p;
		packet_init(&p);
		messenger.add_header(&p, server->addr, Message::ExpectClient);
		serialize_u32(&p, user->id);
		serialize_u32(&p, user->token);
		{
			b8 is_admin = role == Role::Admin;
			serialize_bool(&p, is_admin);
		}
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
			if (config.id == 0) // story mode
				serialize_s16(&p, client->server_state.level); // desired level
		}
		else
			net_error();

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

	sqlite3_stmt* server_list_query(Node* client, Region region, ServerListType type, s32 offset)
	{
		sqlite3_stmt* stmt;
		switch (type)
		{
			case ServerListType::Top:
			{
				stmt = db_query("select ServerConfig.id, ServerConfig.name, User.username, ServerConfig.max_players, ServerConfig.team_count, ServerConfig.game_type from ServerConfig left join User on User.id=ServerConfig.creator_id left join UserServer on (ServerConfig.id=UserServer.server_id and UserServer.user_id=?) where (ServerConfig.is_private=0 or UserServer.role>1) and (ServerConfig.online=1 or ServerConfig.region=?) and (UserServer.role!=1 or UserServer.role is null) order by ServerConfig.online desc, ServerConfig.score desc limit ?,24");
				db_bind_int(stmt, 0, client->user_key.id);
				db_bind_int(stmt, 1, s64(region));
				db_bind_int(stmt, 2, offset);
				break;
			}
			case ServerListType::Recent:
			{
				stmt = db_query("select ServerConfig.id, ServerConfig.name, User.username, ServerConfig.max_players, ServerConfig.team_count, ServerConfig.game_type from ServerConfig inner join UserServer on UserServer.server_id=ServerConfig.id left join User on User.id=ServerConfig.creator_id where UserServer.user_id=? and UserServer.role!=1 order by ServerConfig.online desc, UserServer.timestamp desc limit ?,24");
				db_bind_int(stmt, 0, client->user_key.id);
				db_bind_int(stmt, 1, offset);
				break;
			}
			case ServerListType::Mine:
			{
				stmt = db_query("select ServerConfig.id, ServerConfig.name, User.username, ServerConfig.max_players, ServerConfig.team_count, ServerConfig.game_type from ServerConfig inner join UserServer on UserServer.server_id=ServerConfig.id left join User on User.id=ServerConfig.creator_id where UserServer.user_id=? and UserServer.role>=2 order by ServerConfig.online desc, UserServer.timestamp desc limit ?,24");
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
		using Stream = StreamWrite;

		StreamWrite p;
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
			memset(entry.name, 0, sizeof(entry.name));
			strncpy(entry.name, db_column_text(stmt, 1), MAX_SERVER_CONFIG_NAME);
			memset(entry.creator_username, 0, sizeof(entry.creator_username));
			strncpy(entry.creator_username, db_column_text(stmt, 2), MAX_USERNAME);
			entry.max_players = db_column_int(stmt, 3);
			entry.team_count = db_column_int(stmt, 4);
			entry.game_type = GameType(db_column_int(stmt, 5));

			server_state_for_config_id(id, entry.max_players, &entry.server_state, client->addr.host.type, &entry.addr);

			if (!serialize_server_list_entry(&p, &entry))
				net_error();

			(*offset)++;

			if (count == MAX_SERVER_LIST)
				break;
			count++;
		}

		{
			s32 index = -1;
			serialize_s32(&p, index); // done with the fragment
		}

		serialize_bool(&p, *done); // done with the whole list?

		packet_finalize(&p);
		messenger.send(p, timestamp, client->addr, &sock);
		return true;
	}

	b8 send_server_list(Node* client, Region region, ServerListType type, s32 offset)
	{
		offset = vi_max(offset - 12, 0);
		sqlite3_stmt* stmt = server_list_query(client, region, type, offset);
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

	b8 send_client_connect(Node* server, const Sock::Address& addr)
	{
		using Stream = StreamWrite;
		StreamWrite p;
		packet_init(&p);
		messenger.add_header(&p, addr, Message::ClientConnect);

		Sock::Address server_addr = addr.host.type == Sock::Host::Type::IPv4 ? server->public_ipv4 : server->public_ipv6;
		if (!Sock::Address::serialize(&p, &server_addr))
			net_error();

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
			char addr_str[NET_MAX_ADDRESS];
			server->addr.str(addr_str);
			vi_debug("Server %s connected.", addr_str);
			servers.add(server->addr.hash());
		}

		server->transition(s.level == AssetNull ? Node::State::ServerIdle : Node::State::ServerActive);

		if (server->server_state.id && s.level == AssetNull)
		{
			db_set_server_online(server->server_state.id, false);
			server_config_map.erase(server->server_state.id);
		}

		if (s.id)
		{
			vi_assert(s.level != AssetNull);
			db_set_server_online(s.id, true);
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
		using Stream = StreamRead;
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

	b8 user_exists(u32 id)
	{
		sqlite3_stmt* stmt = db_query("select count(1) from User where id=?");
		db_bind_int(stmt, 0, id);
		db_step(stmt);
		b8 result = b8(db_column_int(stmt, 0));
		db_finalize(stmt);
		return result;
	}

	Node* client_requested_server(Node* client)
	{
		// if the user is requesting to connect to a virtual server that is already active, they can connect immediately
		if (client->server_state.id)
			return server_for_config_id(client->server_state.id);

		return nullptr;
	}

	void client_queue_join(Node* server, Node* client)
	{
		for (s32 i = 0; i < clients_waiting.length; i++)
		{
			if (clients_waiting[i] == client->addr.hash())
			{
				clients_waiting.remove(i);
				break;
			}
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
		send_client_connect(server, client->addr);
		client_queue_join(server, client);
	}

	b8 friendship_get(u32 user1_id, u32 user2_id)
	{
		sqlite3_stmt* stmt = db_query("select count(1) from Friendship where user1_id=? and user2_id=?;");
		db_bind_int(stmt, 0, user1_id);
		db_bind_int(stmt, 1, user2_id);
		db_step(stmt);
		b8 result = db_column_int(stmt, 0);
		db_finalize(stmt);
		return result;
	}

	void friend_set_server_role(u32 user1_id, u32 user2_id, Role role)
	{
		// get list of user 1's servers
		sqlite3_stmt* stmt = db_query("select server_id from UserServer where user_id=? and role>2");
		db_bind_int(stmt, 0, user1_id);
		while (db_step(stmt))
		{
			u32 server_id = u32(db_column_int(stmt, 0));
			update_user_server_linkage(user2_id, server_id, role);
		}
		db_finalize(stmt);
	}

	void friendship_add(u32 user1_id, u32 user2_id)
	{
		friend_set_server_role(user1_id, user2_id, Role::Allowed);

		sqlite3_stmt* stmt = db_query("insert into Friendship (user1_id, user2_id) values (?, ?);");
		db_bind_int(stmt, 0, user1_id);
		db_bind_int(stmt, 1, user2_id);
		db_exec(stmt);
	}

	void friendship_remove(u32 user1_id, u32 user2_id)
	{
		friend_set_server_role(user1_id, user2_id, Role::Banned);

		sqlite3_stmt* stmt = db_query("delete from Friendship where user1_id=? and user2_id=?;");
		db_bind_int(stmt, 0, user1_id);
		db_bind_int(stmt, 1, user2_id);
		db_exec(stmt);
	}

	b8 send_friendship_state(Node* node, u32 friend_id, b8 state)
	{
		using Stream = StreamWrite;
		StreamWrite p;
		packet_init(&p);
		messenger.add_header(&p, node->addr, Message::FriendshipState);
		serialize_u32(&p, friend_id);
		serialize_bool(&p, state);
		packet_finalize(&p);
		messenger.send(p, timestamp, node->addr, &sock);
		return true;
	}

	b8 packet_handle(StreamRead* p, const Sock::Address& addr)
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
				auth_key[auth_key_length] = '\0';
				switch (auth_type)
				{
					case Master::AuthType::None:
						itch_auth_result(addr.hash(), false, 0, nullptr); // failed
						break;
					case Master::AuthType::Itch:
					{
						u64 hash = node->addr.hash();
						if (!Http::request_for_user_data(hash)) // make sure we're not already trying to authenticate this user
						{
							char header[MAX_PATH_LENGTH + 1] = {};
							snprintf(header, MAX_PATH_LENGTH, "Authorization: %s", auth_key);
							Http::get("https://itch.io/api/1/jwt/me", &itch_auth_callback, header, hash);
						}
						break;
					}
					case Master::AuthType::Steam:
						send_auth_response(addr, nullptr, nullptr); // todo
						break;
					default:
						vi_assert(false);
						break;
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
						AssetID level;
						serialize_s16(p, level);
						Region region;
						serialize_enum(p, Region, region);
						if (level < 0 || level >= Asset::Level::count) // todo: verify it is a PvP map
							net_error();
						node->server_state.level = level;
						node->server_state.region = region;
					}
					else
					{
						// check if requested config exists, and find out whether it is private and what region it should be launched in
						sqlite3_stmt* stmt = db_query("select is_private, region from ServerConfig where id=? limit 1;");
						db_bind_int(stmt, 0, requested_server_id);
						if (db_step(stmt))
						{
							b8 is_private = b8(db_column_int(stmt, 0));
							Region region = Region(db_column_int(stmt, 1));
							db_finalize(stmt);

							// check if user has privileges on this server
							Role role = user_role(node->user_key.id, requested_server_id);
							if (role == Role::Banned || (role == Role::None && is_private))
								net_error();

							node->server_state.region = region;
						}
						else // config doesn't exist
						{
							db_finalize(stmt);
							net_error();
						}
					}
					node->server_state.id = requested_server_id;

					if (node->state != Node::State::ClientWaiting)
					{
						// add to client waiting list
						clients_waiting.add(node->addr.hash());
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
					if (server_details_get(config_id, node->user_key.id, &details, node->addr.host.type))
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
					sqlite3_stmt* stmt = db_query("insert into ServerConfig (creator_id, name, config, max_players, team_count, game_type, is_private, online, region, plays, score) values (?, ?, ?, ?, ?, ?, ?, 0, ?, 0, ?);");
					db_bind_int(stmt, 0, node->user_key.id);
					db_bind_text(stmt, 1, config.name);
					char* json = server_config_stringify(config);
					db_bind_text(stmt, 2, json);
					free(json);
					db_bind_int(stmt, 3, config.max_players);
					db_bind_int(stmt, 4, config.team_count);
					db_bind_int(stmt, 5, s64(config.game_type));
					db_bind_int(stmt, 6, config.is_private);
					db_bind_int(stmt, 7, s64(config.region));
					db_bind_int(stmt, 8, server_config_score(0, platform::timestamp()));
					config_id = db_exec(stmt);

					// give friends access to new server
					{
						sqlite3_stmt* stmt = db_query("select user2_id from Friendship where user1_id=?;");
						db_bind_int(stmt, 0, node->user_key.id);
						while (db_step(stmt))
						{
							u32 friend_id = u32(db_column_int(stmt, 0));
							update_user_server_linkage(friend_id, config_id, Role::Allowed);
						}
						db_finalize(stmt);
					}
				}
				else
				{
					// update an existing config
					// check if the user actually has privileges to edit it
					if (user_role(node->user_key.id, config.id) != Role::Admin)
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

				update_user_server_linkage(node->user_key.id, config_id, Role::Admin); // make them an admin

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

				Region region;
				serialize_enum(p, Region, region);
				ServerListType type;
				serialize_enum(p, ServerListType, type);
				s32 offset;
				serialize_s32(p, offset);
				if (offset < 0)
					return false;

				send_server_list(node, region, type, offset);
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
				if (!Sock::Address::serialize(p, &node->public_ipv4))
					net_error();
				if (!Sock::Address::serialize(p, &node->public_ipv6))
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
									send_client_connect(node, connection.client);
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
			case Message::FriendshipGet:
			{
				if (!check_user_key(p, node))
					return false;

				u32 friend_id;
				serialize_u32(p, friend_id);

				send_friendship_state(node, friend_id, friendship_get(node->user_key.id, friend_id));
				break;
			}
			case Message::FriendAdd:
			{
				if (!check_user_key(p, node))
					return false;

				u32 friend_id;
				serialize_u32(p, friend_id);

				if (user_exists(friend_id) && !friendship_get(node->user_key.id, friend_id))
				{
					friendship_add(node->user_key.id, friend_id);
					send_friendship_state(node, friend_id, true);
				}
				else
					send_friendship_state(node, friend_id, false);
				break;
			}
			case Message::FriendRemove:
			{
				if (!check_user_key(p, node))
					return false;

				u32 friend_id;
				serialize_u32(p, friend_id);

				if (user_exists(friend_id))
					friendship_remove(node->user_key.id, friend_id);
				send_friendship_state(node, friend_id, false);
				break;
			}
			case Message::AdminMake:
			case Message::AdminRemove:
			{
				if (!check_user_key(p, node))
					return false;

				u32 config_id;
				serialize_u32(p, config_id);

				u32 friend_id;
				serialize_u32(p, friend_id);

				if (user_role(node->user_key.id, config_id) == Role::Admin && user_exists(friend_id))
				{
					Role role;
					if (type == Message::AdminMake)
						role = Role::Admin;
					else
					{
						// remove admin status
						if (user_role(node->user_key.id, friend_id) == Role::Admin)
							role = Role::Allowed;
						else // some other role already assigned; leave it as-is
							role = Role::None;
					}
					update_user_server_linkage(friend_id, config_id, role);
				}
				break;
			}
			default:
				net_error();
				break;
		}

		return true;
	}

	s32 proc()
	{
		mersenne::srand(platform::timestamp());

		Sock::init();

		if (Sock::udp_open(&sock, NET_MASTER_PORT))
		{
			fprintf(stderr, "%s\n", Sock::get_error());
			return 1;
		}

		Http::init();

		CrashReport::init();

		// open sqlite database
		{
			b8 init_db = !platform::file_exists("deceiver.db");
			
			if (sqlite3_open("deceiver.db", &db))
			{
				fprintf(stderr, "Can't open sqlite database: %s", sqlite3_errmsg(db));
				return 1;
			}

			if (init_db)
			{
				db_exec("create table User (id integer primary key autoincrement, token integer not null, token_timestamp integer not null, itch_id integer, steam_id integer, username varchar(256) not null, banned boolean not null, unique(itch_id), unique(steam_id));");
				db_exec("create table ServerConfig (id integer primary key autoincrement, creator_id integer not null, name text not null, config text, max_players integer not null, team_count integer not null, game_type integer not null, is_private boolean not null, online boolean not null, region integer not null, plays integer not null, score integer not null, foreign key (creator_id) references User(id));");
				db_exec("create table UserServer (user_id integer not null, server_id integer not null, timestamp integer not null, role integer not null, foreign key (user_id) references User(id), foreign key (server_id) references ServerConfig(id), primary key (user_id, server_id));");
				db_exec("create table Friendship (user1_id integer not null, user2_id integer not null, foreign key (user1_id) references User(id), foreign key (user2_id) references User(id), primary key (user1_id, user2_id));");
			}
			db_exec("update ServerConfig set online=0;");
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
						const char* itch_api_key = Json::get_string(json, "itch_api_key");
						if (itch_api_key)
							strncpy(Settings::itch_api_key, itch_api_key, MAX_AUTH_KEY);
					}
					{
						const char* ca_path = Json::get_string(json, "ca_path");
						if (ca_path)
							strncpy(Http::ca_path, ca_path, MAX_PATH_LENGTH);
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

			Http::update();

			CrashReport::update();

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
							clients_waiting.add(client->addr.hash());
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
						removals.add(i->second.addr);
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
					Node* server = node_for_hash(servers[i]);
					if (server->state == Node::State::ServerIdle)
						idle_servers++;
				}

				for (s32 i = 0; i < clients_waiting.length; i++)
				{
					Node* client = node_for_hash(clients_waiting[i]);
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
							Node* server = node_for_hash(servers[i]);
							if (server->state == Node::State::ServerIdle && server->server_state.region == client->server_state.region)
							{
								idle_server = server;
								break;
							}
						}

						if (idle_server)
						{
							idle_servers--;
							send_server_load(idle_server, client);
							send_server_expect_client(idle_server, &client->user_key);
							client_queue_join(idle_server, client);
							i--; // client has been removed from clients_waiting
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
					packet_handle(&packet, addr);
				}
				else
					vi_debug("%s", "Discarding packet due to invalid checksum.");
			}
			else
				platform::vi_sleep(1.0f / 60.0f);
		}

		sqlite3_close(db);

		Http::term();

		CrashReport::term();

		return 0;
	}

}

}

}

int main(int argc, char** argv)
{
	return VI::Net::Master::proc();
}
