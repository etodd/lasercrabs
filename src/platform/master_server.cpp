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
#include <ctime>
#include "sock.h"
#include <unordered_map>
#include "asset/level.h"
#include "asset/version.h"
#include "cjson/cJSON.h"
#include "sqlite/sqlite3.h"
#include "mersenne/mersenne-twister.h"
#include "data/json.h"
#include "data/unicode.h"
#include <cmath>
#include "mongoose/mongoose.h"
#include "sha1/sha1.h"
#include <ctype.h>

#define DEBUG_SQL 0

#if RELEASE_BUILD
#define OFFLINE_DEV 0
#else
#define OFFLINE_DEV 1
#endif
#define AUTHENTICATE_DOWNLOAD_KEYS 0
#define CRASH_DUMP_DIR "crash_dumps/"

#define MAX_DISCORD_ID_LENGTH 32

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

namespace Settings
{
	u64 secret;
	char itch_api_key[MAX_AUTH_KEY + 1];
	char steam_api_key[MAX_AUTH_KEY + 1];
	char gamejolt_api_key[MAX_AUTH_KEY + 1];
	char dashboard_username[MAX_USERNAME];
	char dashboard_password_hash[MAX_AUTH_KEY];
	char discord_webhook[MAX_PATH_LENGTH + 1];
	char discord_bot_token[MAX_AUTH_KEY + 1];
	char discord_channel_id[MAX_DISCORD_ID_LENGTH + 1];
	char discord_bot_user_id[MAX_DISCORD_ID_LENGTH + 1];
	char discord_guild_id[MAX_DISCORD_ID_LENGTH + 1];
	char discord_available_role_id[MAX_DISCORD_ID_LENGTH + 1];
}

namespace Net
{

namespace Master
{
	namespace Dashboard
	{
		void handle_static(mg_connection*, int, void*);
		void handle_api(mg_connection*, int, void*);
	}

	namespace DiscordBot
	{
		void update();
	}
}

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

void ev_handler(mg_connection* conn, int ev, void* ev_data)
{
	switch (ev)
	{
		case MG_EV_HTTP_REQUEST:
		{
			mg_printf
			(
				conn, "%s",
				"HTTP/1.1 403 Forbidden\r\n"
				"Content-Type: text/html\r\n"
				"Transfer-Encoding: chunked\r\n"
				"\r\n"
			);
			mg_printf_http_chunk(conn, "%s", "Forbidden");
			mg_send_http_chunk(conn, "", 0);
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
			mg_file_upload_handler(nc, ev, p, upload_filename);
			break;
		case MG_EV_HTTP_PART_END:
		{
			mg_file_upload_handler(nc, ev, p, upload_filename);
#if RELEASE_BUILD
			Http::smtp("root@master.deceivergame.com", "support@deceivergame.com", "Deceiver crash dump", "A crash dump has been received.");
#endif
			break;
		}
	}
}

void register_endpoints(mg_connection* conn)
{
	mg_register_http_endpoint(conn, "/crash_dump", handle_upload);
	mg_register_http_endpoint(conn, "/dashboard", Master::Dashboard::handle_static);
	mg_register_http_endpoint(conn, "/dashboard/api", Master::Dashboard::handle_api);
}

void init()
{
	mg_mgr_init(&mgr, nullptr);
	{
		char addr[32];
		sprintf(addr, "127.0.0.1:%d", NET_MASTER_HTTP_PORT);
		conn_ipv4 = mg_bind(&mgr, addr, ev_handler);

		sprintf(addr, "[::1]:%d", NET_MASTER_HTTP_PORT);
		conn_ipv6 = mg_bind(&mgr, addr, ev_handler);
	}

	if (conn_ipv4)
	{
		mg_set_protocol_http_websocket(conn_ipv4);
		register_endpoints(conn_ipv4);
		printf("Bound to 127.0.0.1:%d\n", NET_MASTER_HTTP_PORT);
	}

	if (conn_ipv6)
	{
		mg_set_protocol_http_websocket(conn_ipv6);
		register_endpoints(conn_ipv6);
		printf("Bound to [::1]:%d\n", NET_MASTER_HTTP_PORT);
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

	r64 global_timestamp;

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

		struct Client
		{
			UserKey user_key;
			char username[MAX_USERNAME + 1];
		};

		struct Server
		{
			Sock::Address public_ipv4;
			Sock::Address public_ipv6;
		};

		r64 last_message_timestamp;
		r64 state_change_timestamp;
		Sock::Address addr;
		union
		{
			Client client;
			Server server;
		};
		ServerState server_state;
		State state;

		void transition(State s)
		{
			if (s != state)
			{
				state = s;
				state_change_timestamp = global_timestamp;
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

	struct Global
	{
		sqlite3* db;
		std::unordered_map<u64, Node> nodes;
		std::unordered_map<u32, Sock::Address> server_config_map;
		Sock::Handle sock;
		Messenger messenger;
		Array<u64> servers;
		Array<u64> clients_waiting;
		Array<ClientConnection> clients_connecting;
	};
	Global global;

	sqlite3_stmt* db_query(const char* sql)
	{
		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(global.db, sql, -1, &stmt, nullptr))
		{
			fprintf(stderr, "SQL: Failed to prepare statement: %s\nError: %s", sql, sqlite3_errmsg(global.db));
			vi_assert(false);
		}
#if DEBUG_SQL
		printf("%s\n", sql);
#endif
		return stmt;
	}

	void db_bind_int(sqlite3_stmt* stmt, s32 index, s64 value)
	{
		if (sqlite3_bind_int64(stmt, index + 1, value))
		{
			fprintf(stderr, "SQL: Could not bind integer at index %d.\nError: %s", index, sqlite3_errmsg(global.db));
			vi_assert(false);
		}
	}

	void db_bind_null(sqlite3_stmt* stmt, s32 index)
	{
		if (sqlite3_bind_null(stmt, index + 1))
		{
			fprintf(stderr, "SQL: Could not bind null at index %d.\nError: %s", index, sqlite3_errmsg(global.db));
			vi_assert(false);
		}
	}

	void db_bind_text(sqlite3_stmt* stmt, s32 index, const char* text)
	{
		if (sqlite3_bind_text(stmt, index + 1, text, -1, SQLITE_TRANSIENT))
		{
			fprintf(stderr, "SQL: Could not bind text at index %d.\nError: %s", index, sqlite3_errmsg(global.db));
			vi_assert(false);
		}
	}

	Sock::Address server_public_ip(Node* server, Sock::Host::Type type)
	{
		if (type == Sock::Host::Type::IPv4) // prefer ipv4
			return server->server.public_ipv4.port ? server->server.public_ipv4 : server->server.public_ipv6;
		else // prefer ipv6
			return server->server.public_ipv6.port ? server->server.public_ipv6 : server->server.public_ipv4;
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
			fprintf(stderr, "SQL: Failed to step query.\nError: %s", sqlite3_errmsg(global.db));
			vi_assert(false);
			return false;
		}
	}

	s64 db_column_int(sqlite3_stmt* stmt, s32 index)
	{
		return sqlite3_column_int64(stmt, index);
	}

	b8 db_column_null(sqlite3_stmt* stmt, s32 index)
	{
		return sqlite3_column_type(stmt, index) == SQLITE_NULL;
	}

	const char* db_column_text(sqlite3_stmt* stmt, s32 index)
	{
		return (const char*)sqlite3_column_text(stmt, index);
	}

	void db_finalize(sqlite3_stmt* stmt)
	{
		if (sqlite3_finalize(stmt))
		{
			fprintf(stderr, "SQL: Failed to finalize query.\nError: %s", sqlite3_errmsg(global.db));
			vi_assert(false);
		}
	}

	s64 db_exec(sqlite3_stmt* stmt)
	{
		b8 more = db_step(stmt);
		vi_assert(!more); // this kind of query shouldn't return any rows
		db_finalize(stmt);
		return sqlite3_last_insert_rowid(global.db);
	}
	
	void db_exec(const char* sql)
	{
#if DEBUG_SQL
		printf("%s\n", sql);
#endif
		char* err;
		if (sqlite3_exec(global.db, sql, nullptr, nullptr, &err))
		{
			fprintf(stderr, "SQL statement failed: %s\nError: %s", sql, err);
			vi_assert(false);
		}
	}

	Node* node_add_or_get(const Sock::Address& addr)
	{
		u64 hash = addr.hash();
		auto i = global.nodes.find(hash);
		Node* n;
		if (i == global.nodes.end())
		{
			auto i = global.nodes.insert(std::pair<u64, Node>(hash, Node()));
			n = &i.first->second;
			n->addr = addr;
		}
		else
			n = &i->second;
		return n;
	}

	Node* node_for_hash(u64 hash)
	{
		auto i = global.nodes.find(hash);
		if (i == global.nodes.end())
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
		auto i = global.server_config_map.find(id);
		if (i == global.server_config_map.end())
			return nullptr;
		else
			return node_for_address(i->second);
	}

	void server_state_for_config_id(u32 id, s8 max_players, Region region, ServerState* state, Sock::Host::Type addr_type = Sock::Host::Type::IPv4, Sock::Address* addr = nullptr)
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
			state->region = region;
			state->story_mode_team = StoryModeTeam(0);
			if (addr)
				*addr = {};
		}
	}

	void server_remove_clients_connecting(Node* server)
	{
		// reset any clients trying to connect to this server
		for (s32 i = 0; i < global.clients_connecting.length; i++)
		{
			const ClientConnection& connection = global.clients_connecting[i];
			if (connection.server.equals(server->addr))
			{
				Node* client = node_for_address(connection.client);
				if (client)
					client->transition(Node::State::ClientWaiting);
				global.clients_connecting.remove(i);
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
		ServerConfig defaults;
		new (&defaults) ServerConfig();
		config->kill_limit = s16(Json::get_s32(json, "kill_limit", defaults.kill_limit));
		config->flag_limit = s16(Json::get_s32(json, "flag_limit", defaults.flag_limit));
		config->respawns = s16(Json::get_s32(json, "respawns", defaults.respawns));
		config->allow_upgrades = s16(Json::get_s32(json, "allow_upgrades", defaults.allow_upgrades));
		{
			cJSON* start_upgrades = cJSON_GetObjectItem(json, "start_upgrades");
			cJSON* u = start_upgrades->child;
			while (u && config->start_upgrades.length < config->start_upgrades.capacity())
			{
				config->start_upgrades.add(Upgrade(u->valueint));
				u = u->next;
			}
		}
		config->max_players = s8(Json::get_s32(json, "max_players", defaults.max_players));
		config->min_players = s8(Json::get_s32(json, "min_players", defaults.min_players));
		for (s32 i = 0; i < s32(GameType::count); i++)
		{
			char key[64];
			snprintf(key, 64, "time_limit_minutes_%s", ServerConfig::game_type_string(GameType(i)));
			config->time_limit_minutes[i] = u8(Json::get_s32(json, key, defaults.time_limit_minutes[i]));
		}
		config->enable_minions = b8(Json::get_s32(json, "enable_minions", defaults.enable_minions));
		config->enable_batteries = b8(Json::get_s32(json, "enable_batteries", defaults.enable_batteries));
		config->enable_battery_stealth = b8(Json::get_s32(json, "enable_battery_stealth", defaults.enable_battery_stealth));
		config->enable_spawn_shields = b8(Json::get_s32(json, "enable_spawn_shields", defaults.enable_spawn_shields));
		config->drone_shield = s8(Json::get_s32(json, "drone_shield", defaults.drone_shield));
		config->start_energy = s16(Json::get_s32(json, "start_energy", defaults.start_energy));
		config->start_energy_attacker = s16(Json::get_s32(json, "start_energy_attacker", defaults.start_energy_attacker));
		config->cooldown_speed_index = u8(Json::get_s32(json, "cooldown_speed_index", defaults.cooldown_speed_index));
		config->fill_bots = s8(Json::get_s32(json, "fill_bots", defaults.fill_bots));
		cJSON_Delete(json);
	}

	// caller must free() the returned string
	char* server_config_stringify(const ServerConfig& config)
	{
		// id, name, game_type, team_count, and is_private are stored in DB row, not here

		ServerConfig defaults;
		new (&defaults) ServerConfig();

		cJSON* json = cJSON_CreateObject();

		{
			cJSON* levels = cJSON_CreateArray();
			cJSON_AddItemToObject(json, "levels", levels);
			for (s32 i = 0; i < config.levels.length; i++)
				cJSON_AddItemToArray(levels, cJSON_CreateNumber(config.levels[i]));
		}
		if (config.kill_limit != defaults.kill_limit)
			cJSON_AddNumberToObject(json, "kill_limit", config.kill_limit);
		if (config.flag_limit != defaults.flag_limit)
			cJSON_AddNumberToObject(json, "flag_limit", config.flag_limit);
		if (config.respawns != defaults.respawns)
			cJSON_AddNumberToObject(json, "respawns", config.respawns);
		if (config.allow_upgrades != defaults.allow_upgrades)
			cJSON_AddNumberToObject(json, "allow_upgrades", config.allow_upgrades);
		{
			cJSON* start_upgrades = cJSON_CreateArray();
			cJSON_AddItemToObject(json, "start_upgrades", start_upgrades);
			for (s32 i = 0; i < config.start_upgrades.length; i++)
				cJSON_AddItemToArray(start_upgrades, cJSON_CreateNumber(s32(config.start_upgrades[i])));
		}
		if (config.max_players != defaults.max_players)
			cJSON_AddNumberToObject(json, "max_players", config.max_players);
		if (config.min_players != defaults.min_players)
			cJSON_AddNumberToObject(json, "min_players", config.min_players);
		for (s32 i = 0; i < s32(GameType::count); i++)
		{
			if (config.time_limit_minutes[i] != defaults.time_limit_minutes[i])
			{
				char key[64];
				snprintf(key, 64, "time_limit_minutes_%s", ServerConfig::game_type_string(GameType(i)));
				cJSON_AddNumberToObject(json, key, config.time_limit_minutes[i]);
			}
		}
		if (config.enable_minions != defaults.enable_minions)
			cJSON_AddNumberToObject(json, "enable_minions", config.enable_minions);
		if (config.enable_batteries != defaults.enable_batteries)
			cJSON_AddNumberToObject(json, "enable_batteries", config.enable_batteries);
		if (config.enable_battery_stealth != defaults.enable_battery_stealth)
			cJSON_AddNumberToObject(json, "enable_battery_stealth", config.enable_battery_stealth);
		if (config.enable_spawn_shields != defaults.enable_spawn_shields)
			cJSON_AddNumberToObject(json, "enable_spawn_shields", config.enable_spawn_shields);
		if (config.drone_shield != defaults.drone_shield)
			cJSON_AddNumberToObject(json, "drone_shield", config.drone_shield);
		if (config.start_energy != defaults.start_energy)
			cJSON_AddNumberToObject(json, "start_energy", config.start_energy);
		if (config.start_energy_attacker != defaults.start_energy_attacker)
			cJSON_AddNumberToObject(json, "start_energy_attacker", config.start_energy_attacker);
		if (config.cooldown_speed_index != defaults.cooldown_speed_index)
			cJSON_AddNumberToObject(json, "cooldown_speed_index", config.cooldown_speed_index);
		if (config.fill_bots != defaults.fill_bots)
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
			config->max_players = s8(db_column_int(stmt, 2));
			config->team_count = s8(db_column_int(stmt, 3));
			config->game_type = GameType(db_column_int(stmt, 4));
			config->creator_id = u32(db_column_int(stmt, 5));
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
			server_state_for_config_id(config_id, details->config.max_players, details->config.region, &details->state, addr_type, &details->addr);
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

	b8 send_auth_response(const Sock::Address& addr, AuthType auth_type, UserKey* key, const char* username)
	{
		{
			// record auth attempt
			sqlite3_stmt* stmt = db_query("insert into AuthAttempt (timestamp, type, ip, user_id) values (?, ?, ?, ?);");
			db_bind_int(stmt, 0, platform::timestamp());
			db_bind_int(stmt, 1, s32(auth_type));
			char ip[NET_MAX_ADDRESS];
			addr.str_ip_only(ip);
			db_bind_text(stmt, 2, ip);
			if (key)
				db_bind_int(stmt, 3, key->id);
			else
				db_bind_null(stmt, 3);
			db_exec(stmt);
		}

		using Stream = StreamWrite;
		StreamWrite p;
		packet_init(&p);
		global.messenger.add_header(&p, addr, Message::AuthResponse);

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
		global.messenger.send(p, global_timestamp, addr, &global.sock);
		return true;
	}

	b8 send_reauth_required(const Sock::Address& addr)
	{
		using Stream = StreamWrite;
		StreamWrite p;
		packet_init(&p);
		global.messenger.add_header(&p, addr, Message::ReauthRequired);
		packet_finalize(&p);
		global.messenger.send(p, global_timestamp, addr, &global.sock);
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
					sqlite3_stmt* stmt = db_query("insert into User (token, token_timestamp, itch_id, username, banned, vip, completed_campaign) values (?, ?, ?, ?, 0, 0, 0);");
					db_bind_int(stmt, 0, key.token);
					db_bind_int(stmt, 1, platform::timestamp());
					db_bind_int(stmt, 2, itch_id);
					db_bind_text(stmt, 3, username);
					key.id = s32(db_exec(stmt));
				}
				db_finalize(stmt);

				if (success)
				{
					node->client.user_key = key;
					send_auth_response(node->addr, AuthType::Itch, &key, username);
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

						node->client.user_key = key;
						send_auth_response(node->addr, AuthType::Itch, &key, username);
						return;
					}
				}
#endif
				send_auth_response(node->addr, AuthType::Itch, nullptr, nullptr);
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
						s64 itch_id = s64(cJSON_GetObjectItem(owner, "id")->valuedouble);
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
						itch_auth_result(user_data, true, s64(id->valuedouble), username);
#endif
					}
				}
				cJSON_Delete(json);
			}
		}

		if (!success)
			itch_auth_result(user_data, false, 0, nullptr);
	}

	void gamejolt_sign_url(char* url) // assumes MAX_PATH_LENGTH
	{
		char url_with_api_key[MAX_PATH_LENGTH + 1] = {};
		snprintf(url_with_api_key, MAX_PATH_LENGTH, "%s%s", url, Settings::gamejolt_api_key);
		char signature[41];
		sha1::hash(url_with_api_key, signature);
		snprintf(url_with_api_key, MAX_PATH_LENGTH, "%s&signature=%s", url, signature);
		strncpy(url, url_with_api_key, MAX_PATH_LENGTH);
	}

	void gamejolt_auth_result(u64 addr_hash, b8 success, s64 gamejolt_id, const char* username)
	{
		Node* node = node_for_hash(addr_hash);
		if (node)
		{
			if (success)
			{
				UserKey key;
				key.token = u32(mersenne::rand());

				// save user in database

				sqlite3_stmt* stmt = db_query("select id, banned from User where gamejolt_id=? limit 1;");
				db_bind_int(stmt, 0, gamejolt_id);
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
					sqlite3_stmt* stmt = db_query("insert into User (token, token_timestamp, gamejolt_id, username, banned, vip, completed_campaign) values (?, ?, ?, ?, 0, 0, 0);");
					db_bind_int(stmt, 0, key.token);
					db_bind_int(stmt, 1, platform::timestamp());
					db_bind_int(stmt, 2, gamejolt_id);
					db_bind_text(stmt, 3, username);
					key.id = s32(db_exec(stmt));
				}
				db_finalize(stmt);

				if (success)
				{
					node->client.user_key = key;
					send_auth_response(node->addr, AuthType::GameJolt, &key, username);
				}
			}
			else
				send_auth_response(node->addr, AuthType::GameJolt, nullptr, nullptr);
		}
	}

	void gamejolt_user_callback(s32 code, const char* data, u64 user_data)
	{
		Node* node = node_for_hash(user_data);
		if (node)
		{
			b8 success = false;
			if (code == 200)
			{
				cJSON* json = cJSON_Parse(data);
				if (json)
				{
					cJSON* response = cJSON_GetObjectItem(json, "response");
					if (response)
					{
						cJSON* users = cJSON_GetObjectItem(response, "users");
						if (users)
						{
							cJSON* user = users->child;
							if (user)
							{
								cJSON* id = cJSON_GetObjectItem(user, "id");
								if (id)
								{
									success = true;
									gamejolt_auth_result(user_data, true, atol(id->valuestring), node->client.username);
								}
							}
						}
					}
					cJSON_Delete(json);
				}
			}

			if (!success)
				gamejolt_auth_result(user_data, false, 0, nullptr);
		}
	}

	void gamejolt_auth_callback(s32 code, const char* data, u64 user_data)
	{
		Node* node = node_for_hash(user_data);
		if (node)
		{
			b8 success = false;
			if (code == 200)
			{
				cJSON* json = cJSON_Parse(data);
				if (json)
				{
					cJSON* response = cJSON_GetObjectItem(json, "response");
					if (response && strcmp(Json::get_string(response, "success"), "true") == 0)
					{
						success = true;

						CURL* curl = curl_easy_init();
						char* escaped_username = curl_easy_escape(curl, node->client.username, 0);

						char url[MAX_PATH_LENGTH + 1] = {};
						snprintf(url, MAX_PATH_LENGTH, "https://gamejolt.com/api/game/v1/users/?game_id=284977&format=json&username=%s", escaped_username);
						gamejolt_sign_url(url);
						Http::get(url, &gamejolt_user_callback, nullptr, user_data);

						curl_free(escaped_username);
						curl_easy_cleanup(curl);
					}
					cJSON_Delete(json);
				}
			}

			if (!success)
				itch_auth_result(user_data, false, 0, nullptr);
		}
	}

	void steam_auth_result(u64 addr_hash, b8 success, s64 steam_id, const char* username)
	{
		Node* node = node_for_hash(addr_hash);
		if (node)
		{
			if (success)
			{
				UserKey key;
				key.token = u32(mersenne::rand());

				// save user in database

				sqlite3_stmt* stmt = db_query("select id, banned from User where steam_id=? limit 1;");
				db_bind_int(stmt, 0, steam_id);
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
					sqlite3_stmt* stmt = db_query("insert into User (token, token_timestamp, steam_id, username, banned, vip, completed_campaign) values (?, ?, ?, ?, 0, 0, 0);");
					db_bind_int(stmt, 0, key.token);
					db_bind_int(stmt, 1, platform::timestamp());
					db_bind_int(stmt, 2, steam_id);
					db_bind_text(stmt, 3, username);
					key.id = s32(db_exec(stmt));
				}
				db_finalize(stmt);

				if (success)
				{
					node->client.user_key = key;
					send_auth_response(node->addr, AuthType::Steam, &key, username);
				}
			}
			else
				send_auth_response(node->addr, AuthType::Steam, nullptr, nullptr);
		}
	}

	void steam_ownership_callback(s32 code, const char* data, u64 user_data)
	{
		Node* node = node_for_hash(user_data);
		if (node)
		{
			b8 success = false;
			if (code == 200)
			{
				cJSON* json = cJSON_Parse(data);
				if (json)
				{
					cJSON* appownership = cJSON_GetObjectItem(json, "appownership");
					if (appownership)
					{
						if (appownership && strcmp(Json::get_string(appownership, "result"), "OK") == 0)
						{
							cJSON* ownsapp = cJSON_GetObjectItem(appownership, "ownsapp");
							if (ownsapp && ownsapp->valueint)
							{
								cJSON* steam_id = cJSON_GetObjectItem(appownership, "ownersteamid");
								if (steam_id)
								{
									success = true;
									steam_auth_result(user_data, true, strtoull(steam_id->valuestring, nullptr, 10), node->client.username);
								}
							}
						}
					}
				}
			}
			if (!success)
				steam_auth_result(user_data, false, 0, nullptr);
		}
	}

	void steam_auth_callback(s32 code, const char* data, u64 user_data)
	{
		Node* node = node_for_hash(user_data);
		if (node)
		{
			b8 success = false;
			if (code == 200)
			{
				cJSON* json = cJSON_Parse(data);
				if (json)
				{
					cJSON* response = cJSON_GetObjectItem(json, "response");
					if (response)
					{
						cJSON* params = cJSON_GetObjectItem(response, "params");
						if (params && strcmp(Json::get_string(params, "result"), "OK") == 0)
						{
							cJSON* steam_id = cJSON_GetObjectItem(params, "steamid");
							if (steam_id)
							{
								success = true;

								u64 steam_id_int = strtoull(steam_id->valuestring, nullptr, 10);
#if AUTHENTICATE_DOWNLOAD_KEYS
								char url[MAX_PATH_LENGTH + 1] = {};
								snprintf(url, MAX_PATH_LENGTH, "https://api.steampowered.com/ISteamUser/CheckAppOwnership/v1/?appid=728100&key=%s&steamid=%llu", Settings::steam_api_key, steam_id_int);
								Http::get(url, &steam_ownership_callback, nullptr, user_data);
#else
								steam_auth_result(user_data, true, strtoull(steam_id->valuestring, nullptr, 10), node->client.username);
#endif
							}
						}
					}
					cJSON_Delete(json);
				}
			}

			if (!success)
				steam_auth_result(user_data, false, 0, nullptr);
		}
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
				global.server_config_map.erase(node->server_state.id);
			}

			{
				u64 hash = addr.hash();
				for (s32 i = 0; i < global.servers.length; i++)
				{
					if (global.servers[i] == hash)
					{
						global.servers.remove(i);
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
				for (s32 i = 0; i < global.clients_waiting.length; i++)
				{
					if (global.clients_waiting[i] == hash)
					{
						global.clients_waiting.remove(i);
						i--;
					}
				}
			}
		}
		global.nodes.erase(addr.hash());
		global.messenger.remove(addr);
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
				db_step(stmt);
				plays = db_column_int(stmt, 0) + 1;
				db_finalize(stmt);
			}

			{
				sqlite3_stmt* stmt = db_query("update ServerConfig set plays=?, score=? where id=?;");
				db_bind_int(stmt, 0, plays);
				db_bind_int(stmt, 1, server_config_score(plays, platform::timestamp()));
				db_bind_int(stmt, 2, server_id);
				db_exec(stmt);
			}
		}

		return role;
	}

	b8 send_ascension(Node* client, char* username)
	{
		using Stream = StreamWrite;
		StreamWrite p;
		packet_init(&p);
		global.messenger.add_header(&p, client->addr, Message::Ascension);

		s32 length = s32(strlen(username));
		serialize_int(&p, s32, length, 0, MAX_USERNAME);
		serialize_bytes(&p, (u8*)(username), length);

		packet_finalize(&p);
		global.messenger.send(p, global_timestamp, client->addr, &global.sock);
		return true;
	}

	b8 send_server_expect_client(Node* server, UserKey* user)
	{
		using Stream = StreamWrite;

		Role role = Role::None;
		if (server->server_state.id != 0) // 0 = story mode
			role = update_user_server_linkage(user->id, server->server_state.id);

		StreamWrite p;
		packet_init(&p);
		global.messenger.add_header(&p, server->addr, Message::ExpectClient);
		serialize_u32(&p, user->id);
		serialize_u32(&p, user->token);
		{
			b8 is_admin = role == Role::Admin;
			serialize_bool(&p, is_admin);
		}
		packet_finalize(&p);
		global.messenger.send(p, global_timestamp, server->addr, &global.sock);
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
		global.messenger.add_header(&p, server->addr, Message::ServerLoad);
		
		ServerConfig config;
		if (client_desired_server_config(client, &config))
		{
			if (!serialize_server_config(&p, &config))
				net_error();
			if (config.id == 0) // story mode
			{
				serialize_s16(&p, client->server_state.level); // desired level
				serialize_enum(&p, StoryModeTeam, client->server_state.story_mode_team);
			}
		}
		else
			net_error();

		packet_finalize(&p);
		global.messenger.send(p, global_timestamp, server->addr, &global.sock);
		return true;
	}

	b8 send_server_config_saved(Node* client, u32 config_id, u32 request_id)
	{
		using Stream = StreamWrite;
		StreamWrite p;
		packet_init(&p);
		global.messenger.add_header(&p, client->addr, Message::ServerConfigSaved);
		serialize_u32(&p, config_id);
		serialize_u32(&p, request_id);
		packet_finalize(&p);
		global.messenger.send(p, global_timestamp, client->addr, &global.sock);
		return true;
	}

	b8 send_server_config(const Sock::Address& addr, const ServerConfig& config, u32 request_id = 0)
	{
		using Stream = StreamWrite;
		StreamWrite p;
		packet_init(&p);
		global.messenger.add_header(&p, addr, Message::ServerConfig);

		serialize_u32(&p, request_id);

		{
			ServerConfig c = config;
			if (!serialize_server_config(&p, &c))
				net_error();
		}

		packet_finalize(&p);
		global.messenger.send(p, global_timestamp, addr, &global.sock);
		return true;
	}

	b8 send_server_details(const Sock::Address& addr, const ServerDetails& details, u32 request_id)
	{
		using Stream = StreamWrite;
		StreamWrite p;
		packet_init(&p);
		global.messenger.add_header(&p, addr, Message::ServerDetails);

		serialize_u32(&p, request_id);

		{
			ServerDetails d = details;
			if (!serialize_server_details(&p, &d))
				net_error();
		}

		packet_finalize(&p);
		global.messenger.send(p, global_timestamp, addr, &global.sock);
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
				db_bind_int(stmt, 0, client->client.user_key.id);
				db_bind_int(stmt, 1, s64(region));
				db_bind_int(stmt, 2, offset);
				break;
			}
			case ServerListType::Recent:
			{
				stmt = db_query("select ServerConfig.id, ServerConfig.name, User.username, ServerConfig.max_players, ServerConfig.team_count, ServerConfig.game_type from ServerConfig inner join UserServer on UserServer.server_id=ServerConfig.id left join User on User.id=ServerConfig.creator_id where UserServer.user_id=? and UserServer.role!=1 order by ServerConfig.online desc, UserServer.timestamp desc limit ?,24");
				db_bind_int(stmt, 0, client->client.user_key.id);
				db_bind_int(stmt, 1, offset);
				break;
			}
			case ServerListType::Mine:
			{
				stmt = db_query("select ServerConfig.id, ServerConfig.name, User.username, ServerConfig.max_players, ServerConfig.team_count, ServerConfig.game_type from ServerConfig inner join UserServer on UserServer.server_id=ServerConfig.id left join User on User.id=ServerConfig.creator_id where UserServer.user_id=? and UserServer.role>=3 order by ServerConfig.online desc, UserServer.timestamp desc limit ?,24");
				db_bind_int(stmt, 0, client->client.user_key.id);
				db_bind_int(stmt, 1, offset);
				break;
			}
			default:
				vi_assert(false);
				break;
		}

		return stmt;
	}

	b8 send_server_list_fragment(Node* client, Region region, ServerListType type, sqlite3_stmt* stmt, s32* offset, b8* done)
	{
		using Stream = StreamWrite;

		StreamWrite p;
		packet_init(&p);
		global.messenger.add_header(&p, client->addr, Message::ServerList);

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

			u32 id = u32(db_column_int(stmt, 0));
			memset(entry.name, 0, sizeof(entry.name));
			strncpy(entry.name, db_column_text(stmt, 1), MAX_SERVER_CONFIG_NAME);
			memset(entry.creator_username, 0, sizeof(entry.creator_username));
			strncpy(entry.creator_username, db_column_text(stmt, 2), MAX_USERNAME);
			entry.max_players = s8(db_column_int(stmt, 3));
			entry.team_count = s8(db_column_int(stmt, 4));
			entry.game_type = GameType(db_column_int(stmt, 5));

			server_state_for_config_id(id, entry.max_players, region, &entry.server_state, client->addr.host.type, &entry.addr);

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
		global.messenger.send(p, global_timestamp, client->addr, &global.sock);
		return true;
	}

	b8 send_server_list(Node* client, Region region, ServerListType type, s32 offset)
	{
		offset = vi_max(offset - 12, 0);
		sqlite3_stmt* stmt = server_list_query(client, region, type, offset);
		while (true)
		{
			b8 done;
			send_server_list_fragment(client, region, type, stmt, &offset, &done);
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
		global.messenger.add_header(&p, addr, Message::ClientConnect);

		Sock::Address server_addr = addr.host.type == Sock::Host::Type::IPv4 ? server->server.public_ipv4 : server->server.public_ipv6;
		if (!Sock::Address::serialize(&p, &server_addr))
			net_error();

		packet_finalize(&p);
		global.messenger.send(p, global_timestamp, addr, &global.sock);
		return true;
	}

	s8 server_client_slots_connecting(Node* server)
	{
		s8 slots = 0;
		for (s32 i = 0; i < global.clients_connecting.length; i++)
		{
			const ClientConnection& connection = global.clients_connecting[i];
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
			global.servers.add(server->addr.hash());
		}

		server->transition(s.level == AssetNull ? Node::State::ServerIdle : Node::State::ServerActive);

		if (server->server_state.id && s.level == AssetNull)
		{
			db_set_server_online(server->server_state.id, false);
			global.server_config_map.erase(server->server_state.id);
		}

		if (s.id)
		{
			vi_assert(s.level != AssetNull);
			db_set_server_online(s.id, true);
			global.server_config_map[s.id] = server->addr;
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
			if (node->client.user_key.equals(key)) // already authenticated
			{
				if (node->state == Node::State::Invalid)
					node->state = Node::State::ClientIdle;
				return true;
			}
			else if (node->client.user_key.id == 0 && node->client.user_key.token == 0) // user hasn't been authenticated yet
			{
				sqlite3_stmt* stmt = db_query("select token, token_timestamp from User where id=? limit 1;");
				db_bind_int(stmt, 0, key.id);
				if (db_step(stmt))
				{
					s64 token = db_column_int(stmt, 0);
					s64 token_timestamp = db_column_int(stmt, 1);
					if (u32(token) == key.token && platform::timestamp() - token_timestamp < MASTER_TOKEN_TIMEOUT)
					{
						node->client.user_key = key;
						if (node->state == Node::State::Invalid)
							node->state = Node::State::ClientIdle;
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
		for (s32 i = 0; i < global.clients_waiting.length; i++)
		{
			if (global.clients_waiting[i] == client->addr.hash())
			{
				global.clients_waiting.remove(i);
				break;
			}
		}

		ClientConnection* connection = global.clients_connecting.add();
		connection->timestamp = global_timestamp;
		connection->server = server->addr;
		connection->client = client->addr;
		connection->slots = client->server_state.player_slots;

		client->transition(Node::State::ClientConnecting);
	}

	void client_connect_to_existing_server(Node* client, Node* server)
	{
		send_server_expect_client(server, &client->client.user_key);
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
		global.messenger.add_header(&p, node->addr, Message::FriendshipState);
		serialize_u32(&p, friend_id);
		serialize_bool(&p, state);
		packet_finalize(&p);
		global.messenger.send(p, global_timestamp, node->addr, &global.sock);
		return true;
	}

	char hex_char(u8 c)
	{
		if (c < 10)
			return '0' + char(c);
		else
			return 'A' + char(c - 10);
	}

	b8 packet_handle(StreamRead* p, const Sock::Address& addr)
	{
		using Stream = StreamRead;
		{
			s16 version;
			serialize_s16(p, version);
			if (version != GAME_VERSION)
			{
				using Stream = StreamWrite;
				StreamWrite p;
				packet_init(&p);
				global.messenger.add_header(&p, addr, Message::WrongVersion);
				packet_finalize(&p);
				Sock::udp_send(&global.sock, addr, p.data.data, p.bytes_written());
				return false; // ignore this packet
			}
		}
		SequenceID seq;
		serialize_int(p, SequenceID, seq, 0, NET_SEQUENCE_COUNT - 1);
		Message type;
		serialize_enum(p, Message, type);
		global.messenger.received(type, seq, addr, &global.sock);

		Node* node = node_add_or_get(addr);
		node->last_message_timestamp = global_timestamp;

		switch (type)
		{
			case Message::Ack:
			case Message::Keepalive:
				break;
			case Message::Disconnect:
			{
				disconnected(addr);
				break;
			}
			case Message::Auth:
			{
				AuthType auth_type;
				serialize_enum(p, AuthType, auth_type);

				s32 username_length = 0;
				char username[MAX_PATH_LENGTH + 1];
				if (auth_type == AuthType::GameJolt || auth_type == AuthType::Steam)
				{
					serialize_int(p, s32, username_length, 0, MAX_PATH_LENGTH);
					serialize_bytes(p, (u8*)username, username_length);
				}
				username[username_length] = '\0';

				s32 auth_key_length;
				serialize_int(p, s32, auth_key_length, 0, MAX_AUTH_KEY);
				u8 auth_key[MAX_AUTH_KEY + 1];

				serialize_bytes(p, (u8*)auth_key, auth_key_length);
				auth_key[auth_key_length] = '\0';

				CURL* curl = curl_easy_init();

				switch (auth_type)
				{
					case AuthType::None:
						itch_auth_result(addr.hash(), false, 0, nullptr); // failed
						break;
					case AuthType::Itch:
					case AuthType::ItchOAuth:
					{
						u64 hash = node->addr.hash();
						if (!Http::request_for_user_data(hash)) // make sure we're not already trying to authenticate this user
						{
							char* escaped_auth_key = curl_easy_escape(curl, (char*)(auth_key), 0);
							char header[MAX_PATH_LENGTH + 1] = {};
							snprintf(header, MAX_PATH_LENGTH, "Authorization: Bearer %s", escaped_auth_key);
							curl_free(escaped_auth_key);
							const char* url = auth_type == AuthType::Itch ? "https://itch.io/api/1/jwt/me" : "https://itch.io/api/1/key/me";
							Http::get(url, &itch_auth_callback, header, hash);
						}
						break;
					}
					case AuthType::GameJolt:
					{
						u64 hash = node->addr.hash();
						if (!Http::request_for_user_data(hash)) // make sure we're not already trying to authenticate this user
						{
							strncpy(node->client.username, username, MAX_USERNAME);
							char* escaped_username = curl_easy_escape(curl, username, 0);
							char* escaped_auth_key = curl_easy_escape(curl, (char*)(auth_key), 0);
							char url[MAX_PATH_LENGTH + 1] = {};
							snprintf(url, MAX_PATH_LENGTH, "https://gamejolt.com/api/game/v1/users/auth/?game_id=284977&format=json&username=%s&user_token=%s", escaped_username, escaped_auth_key);
							curl_free(escaped_username);
							curl_free(escaped_auth_key);
							gamejolt_sign_url(url);
							Http::get(url, &gamejolt_auth_callback, nullptr, hash);
						}
						break;
					}
					case AuthType::Steam:
					{
						u64 hash = node->addr.hash();
						if (!Http::request_for_user_data(hash)) // make sure we're not already trying to authenticate this user
						{
							strncpy(node->client.username, username, MAX_USERNAME);

							char hex_auth_key[(MAX_AUTH_KEY * 2) + 1];

							for (s32 i = 0; i < auth_key_length; i++)
							{
								hex_auth_key[i * 2] = hex_char((auth_key[i] & (0xf0)) >> 4);
								hex_auth_key[(i * 2) + 1] = hex_char(auth_key[i] & (0xf));
							}
							hex_auth_key[auth_key_length * 2] = '\0';

							char url[(MAX_PATH_LENGTH * 3) + 1] = {};
							snprintf(url, MAX_PATH_LENGTH * 3, "https://api.steampowered.com/ISteamUserAuth/AuthenticateUserTicket/v1/?appid=728100&key=%s&ticket=%s", Settings::steam_api_key, hex_auth_key);
							Http::get(url, &steam_auth_callback, nullptr, hash);
						}
						break;
					}
					default:
						vi_assert(false);
						break;
				}

				curl_easy_cleanup(curl);

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
						serialize_enum(p, StoryModeTeam, node->server_state.story_mode_team);
						node->server_state.level = level;
						node->server_state.region = region;
						node->server_state.player_slots = 1;
					}
					else // multiplayer
					{
						serialize_int(p, s8, node->server_state.player_slots, 1, MAX_GAMEPADS);

						// check if requested config exists, and find out whether it is private and what region it should be launched in
						sqlite3_stmt* stmt = db_query("select is_private, region from ServerConfig where id=? limit 1;");
						db_bind_int(stmt, 0, requested_server_id);
						if (db_step(stmt))
						{
							b8 is_private = b8(db_column_int(stmt, 0));
							Region region = Region(db_column_int(stmt, 1));
							db_finalize(stmt);

							// check if user has privileges on this server
							Role role = user_role(node->client.user_key.id, requested_server_id);
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
						global.clients_waiting.add(node->addr.hash());
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
					if (server_details_get(config_id, node->client.user_key.id, &details, node->addr.host.type))
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
					db_bind_int(stmt, 0, node->client.user_key.id);
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
					config_id = u32(db_exec(stmt));

					// give friends access to new server
					{
						sqlite3_stmt* stmt = db_query("select user2_id from Friendship where user1_id=?;");
						db_bind_int(stmt, 0, node->client.user_key.id);
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
					if (user_role(node->client.user_key.id, config.id) != Role::Admin)
						net_error();
					sqlite3_stmt* stmt = db_query("update ServerConfig set name=?, config=?, max_players=?, team_count=?, game_type=?, is_private=?, region=? where id=?;");
					db_bind_text(stmt, 0, config.name);
					char* json = server_config_stringify(config);
					db_bind_text(stmt, 1, json);
					free(json);
					db_bind_int(stmt, 2, config.max_players);
					db_bind_int(stmt, 3, config.team_count);
					db_bind_int(stmt, 4, s32(config.game_type));
					db_bind_int(stmt, 5, config.is_private);
					db_bind_int(stmt, 6, s64(config.region));
					db_bind_int(stmt, 7, config.id);
					db_exec(stmt);
					config_id = config.id;
				}

				update_user_server_linkage(node->client.user_key.id, config_id, Role::Admin); // make them an admin

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
				u64 secret;
				serialize_u64(p, secret);
				if (secret != Settings::secret)
					net_error();
				ServerState s;
				if (!serialize_server_state(p, &s))
					net_error();
				if (!Sock::Address::serialize(p, &node->server.public_ipv4))
					net_error();
				if (!Sock::Address::serialize(p, &node->server.public_ipv6))
					net_error();
				if (node->state == Node::State::ServerLoading)
				{
					if (node->state_change_timestamp > global_timestamp - MASTER_SERVER_LOAD_TIMEOUT)
					{
						if (s.id == node->server_state.id) // done loading
						{
							server_update_state(node, s);

							// tell clients to connect to this server
							for (s32 i = 0; i < global.clients_connecting.length; i++)
							{
								const ClientConnection& connection = global.clients_connecting[i];
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

				send_friendship_state(node, friend_id, friendship_get(node->client.user_key.id, friend_id));
				break;
			}
			case Message::FriendAdd:
			{
				if (!check_user_key(p, node))
					return false;

				u32 friend_id;
				serialize_u32(p, friend_id);

				if (user_exists(friend_id) && !friendship_get(node->client.user_key.id, friend_id))
				{
					friendship_add(node->client.user_key.id, friend_id);
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
					friendship_remove(node->client.user_key.id, friend_id);
				send_friendship_state(node, friend_id, false);
				break;
			}
			case Message::UserRoleSet:
			{
				b8 is_server;
				serialize_bool(p, is_server);

				if (is_server)
				{
					u64 secret;
					serialize_u64(p, secret);
					if (secret != Settings::secret)
						net_error();
				}
				else if (!check_user_key(p, node))
					return false;

				u32 config_id;
				serialize_u32(p, config_id);

				u32 friend_id;
				serialize_u32(p, friend_id);

				Role desired_role;
				serialize_enum(p, Role, desired_role);

				if (user_exists(friend_id)
					&& (is_server || user_role(node->client.user_key.id, config_id) == Role::Admin))
					update_user_server_linkage(friend_id, config_id, desired_role);
				break;
			}
			case Message::ClientRequestAscension:
			{
				sqlite3_stmt* stmt = db_query("select username from User where completed_campaign=1 order by random() limit 1;");
				while (db_step(stmt))
				{
					char* username = (char*)(db_column_text(stmt, 0));
					send_ascension(node, username);
				}
				db_finalize(stmt);
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
		mersenne::srand(u32(platform::timestamp()));

		Sock::init();

		if (Sock::udp_open(&global.sock, NET_MASTER_PORT))
		{
			fprintf(stderr, "%s\n", Sock::get_error());
			return 1;
		}

		Http::init();

		CrashReport::init();

		// open sqlite database
		{
			b8 init_db = !platform::file_exists("deceiver.db");
			
			if (sqlite3_open("deceiver.db", &global.db))
			{
				fprintf(stderr, "Can't open sqlite database: %s", sqlite3_errmsg(global.db));
				return 1;
			}

			if (init_db)
			{
				db_exec("create table User (id integer primary key autoincrement, token integer not null, token_timestamp integer not null, itch_id integer, steam_id integer, gamejolt_id integer, username varchar(256) not null, banned boolean not null, vip boolean not null, completed_campaign boolean not null, unique(itch_id), unique(steam_id));");
				db_exec("create table ServerConfig (id integer primary key autoincrement, creator_id integer not null, name text not null, config text, max_players integer not null, team_count integer not null, game_type integer not null, is_private boolean not null, online boolean not null, region integer not null, plays integer not null, score integer not null, foreign key (creator_id) references User(id));");
				db_exec("create table UserServer (user_id integer not null, server_id integer not null, timestamp integer not null, role integer not null, foreign key (user_id) references User(id), foreign key (server_id) references ServerConfig(id), primary key (user_id, server_id));");
				db_exec("create table Friendship (user1_id integer not null, user2_id integer not null, foreign key (user1_id) references User(id), foreign key (user2_id) references User(id), primary key (user1_id, user2_id));");
				db_exec("create table AuthAttempt (timestamp integer not null, type integer not null, ip text not null, user_id integer, foreign key (user_id) references User(id));");
				db_exec("create table DiscordUser (id integer primary key, time_offset_half_hour integer, member_available_role boolean not null);");
				db_exec("create table DiscordPlaytime (user_id integer, start integer not null, end integer not null);");
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
					{
						cJSON* s = cJSON_GetObjectItem(json, "secret");
						Settings::secret = s ? s->valueint : 0;
					}
					{
						const char* itch_api_key = Json::get_string(json, "itch_api_key");
						if (itch_api_key)
							strncpy(Settings::itch_api_key, itch_api_key, MAX_AUTH_KEY);
					}
					{
						const char* steam_api_key = Json::get_string(json, "steam_api_key");
						if (steam_api_key)
							strncpy(Settings::steam_api_key, steam_api_key, MAX_AUTH_KEY);
					}
					{
						const char* gamejolt_api_key = Json::get_string(json, "gamejolt_api_key");
						if (gamejolt_api_key)
							strncpy(Settings::gamejolt_api_key, gamejolt_api_key, MAX_AUTH_KEY);
					}
					{
						const char* dashboard_username = Json::get_string(json, "dashboard_username", "test");
						if (dashboard_username)
							strncpy(Settings::dashboard_username, dashboard_username, MAX_USERNAME);
					}
					{
						const char* discord_webhook = Json::get_string(json, "discord_webhook");
						if (discord_webhook)
							strncpy(Settings::discord_webhook, discord_webhook, MAX_PATH_LENGTH);
					}
					{
						const char* discord_bot_token = Json::get_string(json, "discord_bot_token");
						if (discord_bot_token)
							strncpy(Settings::discord_bot_token, discord_bot_token, MAX_AUTH_KEY);
					}
					{
						const char* discord_channel_id = Json::get_string(json, "discord_channel_id");
						if (discord_channel_id)
							strncpy(Settings::discord_channel_id, discord_channel_id, MAX_DISCORD_ID_LENGTH);
					}
					{
						const char* discord_bot_user_id = Json::get_string(json, "discord_bot_user_id");
						if (discord_bot_user_id)
							strncpy(Settings::discord_bot_user_id, discord_bot_user_id, MAX_DISCORD_ID_LENGTH);
					}
					{
						const char* discord_guild_id = Json::get_string(json, "discord_guild_id");
						if (discord_guild_id)
							strncpy(Settings::discord_guild_id, discord_guild_id, MAX_DISCORD_ID_LENGTH);
					}
					{
						const char* discord_available_role_id = Json::get_string(json, "discord_available_role_id");
						if (discord_available_role_id)
							strncpy(Settings::discord_available_role_id, discord_available_role_id, MAX_DISCORD_ID_LENGTH);
					}
					{
						const char* dashboard_password_hash = Json::get_string(json, "dashboard_password_hash", "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3"); // "test"
						if (dashboard_password_hash)
							strncpy(Settings::dashboard_password_hash, dashboard_password_hash, MAX_AUTH_KEY);
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
				global_timestamp += vi_min(0.25, t - last_update);
				last_update = t;
			}

			DiscordBot::update();

			Http::update();

			CrashReport::update();

			global.messenger.update(global_timestamp, &global.sock);

			// remove timed out client connection attempts
			{
				r64 threshold = global_timestamp - MASTER_CLIENT_CONNECTION_TIMEOUT;
				for (s32 i = 0; i < global.clients_connecting.length; i++)
				{
					const ClientConnection& c = global.clients_connecting[i];
					if (c.timestamp < threshold)
					{
						Node* client = node_for_address(c.client);
						if (client && client->state == Node::State::ClientConnecting)
						{
							client->transition(Node::State::ClientWaiting); // give up connecting, go back to matchmaking
							global.clients_waiting.add(client->addr.hash());
						}

						Node* server = node_for_address(c.server);
						if (server)
						{
							// since there are clients connecting to this server, we've been ignoring its updates telling us how many open slots it has
							// we need to manually update the open slot count until the server gives us a fresh count
							// don't try to fill these slots until the server tells us for sure they're available
							server->server_state.player_slots -= c.slots;
						}

						global.clients_connecting.remove(i);
						i--;
					}
				}
			}

			// remove inactive nodes
			if (global_timestamp - last_audit > MASTER_AUDIT_INTERVAL)
			{
				r64 threshold = global_timestamp - MASTER_INACTIVE_THRESHOLD;
				Array<Sock::Address> removals;
				for (auto i = global.nodes.begin(); i != global.nodes.end(); i++)
				{
					if (i->second.last_message_timestamp < threshold)
						removals.add(i->second.addr);
				}
				for (s32 i = 0; i < removals.length; i++)
					disconnected(removals[i]);
				last_audit = global_timestamp;
			}

			if (global_timestamp - last_match > MASTER_MATCH_INTERVAL)
			{
				last_match = global_timestamp;

				for (s32 i = 0; i < global.clients_waiting.length; i++)
				{
					Node* client = node_for_hash(global.clients_waiting[i]);
					Node* server = client_requested_server(client);
					if (server)
					{
						if (server_open_slots(server) >= client->server_state.player_slots)
						{
							client_connect_to_existing_server(client, server);
							i--; // client has been removed from clients_waiting
						}
					}
					else
					{
						// allocate an idle server for this client
						Node* idle_server = nullptr;
						for (s32 i = 0; i < global.servers.length; i++)
						{
							Node* server = node_for_hash(global.servers[i]);
							if (server->state == Node::State::ServerIdle && server->server_state.region == client->server_state.region)
							{
								idle_server = server;
								break;
							}
						}

						if (idle_server)
						{
							send_server_load(idle_server, client);
							send_server_expect_client(idle_server, &client->client.user_key);
							client_queue_join(idle_server, client);
							i--; // client has been removed from clients_waiting
						}
					}
				}
			}

			Sock::Address addr;
			StreamRead packet;
			s32 bytes_read = Sock::udp_receive(&global.sock, &addr, packet.data.data, NET_MAX_PACKET_SIZE);
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

		sqlite3_close(global.db);

		Http::term();

		CrashReport::term();

		return 0;
	}

namespace DiscordBot
{
	const int max_command = 32;

	struct State
	{
		r64 last_poll;
		r64 last_stat_mention_timestamp;
		s32 last_stat_mention_total_players;
		char last_poll_message_id[MAX_DISCORD_ID_LENGTH + 1];
	};
	State state;

	struct curl_slist* auth_headers(CURL* curl)
	{
		struct curl_slist* headers;

		{
			char auth_header[MAX_PATH_LENGTH + 1] = {};
			char* escaped_bot_token = curl_easy_escape(curl, (char*)(Settings::discord_bot_token), 0);
			snprintf(auth_header, MAX_PATH_LENGTH, "Authorization: Bot %s", escaped_bot_token);
			curl_free(escaped_bot_token);
			headers = curl_slist_append(nullptr, auth_header);
		}

		headers = curl_slist_append(headers, "User-Agent: DiscordBot (http://deceivergame.com, " BUILD_ID ")");

		return headers;
	}

	void msg_post(const char* msg)
	{
		CURL* curl = curl_easy_init();
		curl_easy_setopt(curl, CURLOPT_URL, Settings::discord_webhook);

		{
			cJSON* post = cJSON_CreateObject();
			cJSON_AddStringToObject(post, "content", msg);
			char* post_string = cJSON_Print(post);
			curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, post_string);
			free(post_string);
			Json::json_free(post);
		}

		struct curl_slist* headers = auth_headers(curl);
		headers = curl_slist_append(headers, "Content-Type: application/json");
		headers = curl_slist_append(headers, "Expect:"); // initialize custom header list stating that Expect: 100-continue is not wanted
		Http::add(curl, nullptr, headers);
	}

	void cmd_acknowledge(cJSON* msg)
	{
		CURL* curl = curl_easy_init();

		char url[MAX_PATH_LENGTH + 1];
		const char* msg_id = Json::get_string(msg, "id");
		snprintf(url, MAX_PATH_LENGTH, u8"https://discordapp.com/api/v6/channels/%s/messages/%s/reactions/\x2705/@me", Settings::discord_channel_id, msg_id);
		curl_easy_setopt(curl, CURLOPT_URL, url);

		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT"); 
		struct curl_slist* headers = auth_headers(curl);
		headers = curl_slist_append(headers, "Content-Length: 0");
		headers = curl_slist_append(headers, "Expect:"); // initialize custom header list stating that Expect: 100-continue is not wanted
		Http::add(curl, nullptr, headers);
	}

	b8 user_exists(const char* id)
	{
		sqlite3_stmt* stmt = db_query("select count(1) from DiscordUser where id=?;");
		db_bind_text(stmt, 0, id);
		db_step(stmt);
		b8 exists = db_column_int(stmt, 0) > 0;
		db_finalize(stmt);
		return exists;
	}

	// returns true if user has set their time offset
	b8 user_time_offset_half_hour(const char* id, s32* time_offset_half_hour = nullptr)
	{
		sqlite3_stmt* stmt = db_query("select time_offset_half_hour from DiscordUser where id=?;");
		db_bind_text(stmt, 0, id);
		b8 time_offset_set = false;
		if (db_step(stmt))
		{
			if (!db_column_null(stmt, 0))
			{
				if (time_offset_half_hour)
					*time_offset_half_hour = s32(db_column_int(stmt, 0));
				time_offset_set = true;
			}
		}
		db_finalize(stmt);
		return time_offset_set;
	}

	// end is the last character parsed
	b8 parse_time(const char* input, s32* hour = nullptr, s32* minute = nullptr, const char** end = nullptr)
	{
		// formats:
		// 13
		// 13:30
		// 1330
		// 1pm
		b8 pm = false;
		b8 am = false;

		if (end)
			*end = input;

		char time_str[max_command + 1] = {};
		{
			s32 index = 0;
			char c;
			while ((c = *input) && index < max_command)
			{
				if (c == ':')
				{
					// skip
				}
				else if (c >= '0' && c <= '9')
				{
					time_str[index] = c;
					index++;
				}
				else
				{
					if (c == 'a')
						am = true;
					else if (c == 'p')
						pm = true;
					break;
				}
				if (end)
					*end = input;
				input++;
			}
		}

		s32 user_hour;
		s32 user_minute;

		switch (strlen(time_str))
		{
			case 1:
			case 2:
			{
				user_hour = atoi(time_str);
				user_minute = 0;
				break;
			}
			case 3:
			{
				user_hour = time_str[0] - '0';
				user_minute = atoi(&time_str[1]);
				break;
			}
			case 4:
			{
				const char hour_str[3] = { time_str[0], time_str[1], 0 };
				const char minute_str[3] = { time_str[2], time_str[3], 0 };
				user_hour = atoi(hour_str);
				user_minute = atoi(minute_str);
				break;
			}
			default:
				return false;
		}

		if (pm)
		{
			if (user_hour != 12)
				user_hour += 12;
		}
		else if (am)
		{
			if (user_hour == 12)
				user_hour = 0;
		}

		if (user_hour >= 0 && user_hour <= 23
			&& user_minute >= 0 && user_minute <= 59)
		{
			if (hour)
				*hour = user_hour;
			if (minute)
				*minute = user_minute;
			return true;
		}
		else
			return false;
	}

	const char* first_digit(const char* str)
	{
		if (!str)
			return nullptr;

		while (char c = *str)
		{
			if (c >= '0' && c <= '9')
				return str;
			str++;
		}
		return nullptr;
	}

	b8 parse_time_range(const char* time_str, std::tm* start, std::tm* end)
	{
		const char* end_of_start_time;
		if (parse_time(time_str, &start->tm_hour, &start->tm_min, &end_of_start_time))
		{
			// do we have an end time?
			end_of_start_time++;
			const char* end_str = first_digit(end_of_start_time);
			if (end_str)
			{
				if (parse_time(end_str, &end->tm_hour, &end->tm_min))
					return true;
			}
			else
			{
				// no end time specified
				*end = *start;
				end->tm_min += 30;
				return true;
			}
		}

		return false;
	}

	void stats(s32* playing, s32* in_lobby, s32* available)
	{
		*playing = 0;
		for (s32 i = 0; i < global.servers.length; i++)
		{
			Node* node = node_for_hash(global.servers[i]);
			if (node->server_state.id) // multiplayer
			{
				ServerConfig config;
				server_config_get(node->server_state.id, &config);
				*playing += config.max_players - node->server_state.player_slots;
			}
		}

		*in_lobby = 0;
		for (auto i = global.nodes.begin(); i != global.nodes.end(); i++)
		{
			Node* node = &i->second;
			if (node->state == Node::State::ClientWaiting
				|| node->state == Node::State::ClientConnecting
				|| node->state == Node::State::ClientIdle)
				(*in_lobby)++;
		}

		{
			sqlite3_stmt* stmt = db_query("select count(1) from DiscordPlaytime where start < ? and end > ?");
			s64 t = s64(platform::timestamp());
			db_bind_int(stmt, 0, t);
			db_bind_int(stmt, 1, t);
			db_step(stmt);
			*available = db_column_int(stmt, 0);
			db_finalize(stmt);
		}
	}

	void msg_handle(cJSON* msg)
	{
		cJSON* author = cJSON_GetObjectItem(msg, "author");
		if (!author)
			return;

		const char* author_id = Json::get_string(author, "id");
		if (!author_id)
			return;

		const char* content = Json::get_string(msg, "content");
		if (*content == '!')
		{
			char cmd[max_command + 1] = {};
			{
				s32 index = 0;
				b8 last_char_was_whitespace = false;
				char c;
				while ((c = *content) && index < max_command)
				{
					if (isspace(c))
					{
						if (last_char_was_whitespace)
						{
							// skip consecutive whitespace
							content++;
							continue;
						}
						else
							last_char_was_whitespace = true;
					}
					else
					{
						last_char_was_whitespace = false;
						if (c >= 'A' && c <= 'Z')
							c |= ' '; // convert to lowercase
					}

					cmd[index] = c;
					index++;
					content++;
				}
			}

			if (strstr(cmd, "!play ") == cmd || strcmp(cmd, "!play") == 0
				|| strstr(cmd, "!p ") == cmd || strcmp(cmd, "!p") == 0)
			{
				s32 time_offset_half_hour;
				if (user_time_offset_half_hour(author_id, &time_offset_half_hour))
				{
					// user has set local time offset

					std::time_t now = std::time(nullptr);
					std::tm now_local = *std::localtime(&now);
					std::tm start = now_local;
					std::tm end = now_local;

					if (strcmp(cmd, "!play") == 0 || strcmp(cmd, "!p") == 0)
					{
						// play now
						end.tm_min += 30;
					}
					else
					{
						// play later
						// formats:
						// 7pm
						// Thursday 7pm
						// Thursday 7pm-9pm

						// split argument away from command
						char* time_str = strchr(cmd, ' ') + 1;
						if (!(*time_str))
							return;

						if (!parse_time_range(time_str, &start, &end))
						{
							// some other day?
							start = now_local;
							end = now_local;

							const char* date_str = time_str;
							time_str = strchr(time_str, ' ');
							if (time_str)
							{
								time_str++;

								{
									s32 weekday;
									if (strstr(date_str, "sun"))
										weekday = 0;
									else if (strstr(date_str, "mon"))
										weekday = 1;
									else if (strstr(date_str, "tue"))
										weekday = 2;
									else if (strstr(date_str, "wed"))
										weekday = 3;
									else if (strstr(date_str, "thu"))
										weekday = 4;
									else if (strstr(date_str, "fri"))
										weekday = 5;
									else if (strstr(date_str, "sat"))
										weekday = 6;
									else
										weekday = -1;

									if (weekday == -1)
									{
										if (strstr(date_str, "tom") || strstr(date_str, "tmw")) // tomorrow
											start.tm_mday++;
									}
									else
									{
										// day of the week
										start.tm_mday += weekday - now_local.tm_wday;
										if (weekday <= now_local.tm_wday)
											start.tm_mday += 7;
									}
								}

								end.tm_mday = start.tm_mday;

								if (!parse_time_range(time_str, &start, &end))
									return;
							}
						}

						// apply user's timezone
						start.tm_min += time_offset_half_hour * 30;
						end.tm_min += time_offset_half_hour * 30;
					}
					std::time_t start_timestamp = std::mktime(&start);
					std::time_t end_timestamp = std::mktime(&end);

					if (start_timestamp < now)
						start_timestamp += 24 * 60 * 60;
					while (end_timestamp < start_timestamp)
						end_timestamp += 24 * 60 * 60;

					sqlite3_stmt* stmt = db_query("insert into DiscordPlaytime (user_id, start, end) values (?, ?, ?);");
					db_bind_text(stmt, 0, author_id);
					db_bind_int(stmt, 1, s64(start_timestamp));
					db_bind_int(stmt, 2, s64(end_timestamp));
					db_exec(stmt);

					cmd_acknowledge(msg);
				}
				else
					msg_post("Set your local time first using !time.");
			}
			else if (strstr(cmd, "!time ") == cmd || strstr(cmd, "!t ") == cmd)
			{
				// split argument away from command
				char* time_str = strchr(cmd, ' ') + 1;
				if (!(*time_str))
					return;

				s32 user_hour;
				s32 user_minute;
				if (parse_time(time_str, &user_hour, &user_minute))
				{
					std::time_t now = std::time(nullptr);
					std::tm now_local = *std::localtime(&now);
					s32 offset_hour = now_local.tm_hour - user_hour;
					s32 offset_minute = now_local.tm_min - user_minute;
					s32 offset_half_hour = offset_hour * 2 + (offset_minute >= 30 ? 1 : 0);

					if (user_exists(author_id))
					{
						// update existing user
						sqlite3_stmt* stmt = db_query("update DiscordUser set time_offset_half_hour=? where id=?;");
						db_bind_int(stmt, 0, offset_half_hour);
						db_bind_text(stmt, 1, author_id);
						db_exec(stmt);
					}
					else
					{
						// create user
						sqlite3_stmt* stmt = db_query("insert into DiscordUser (id, time_offset_half_hour) values (?, ?);");
						db_bind_text(stmt, 0, author_id);
						db_bind_int(stmt, 1, offset_half_hour);
						db_exec(stmt);
					}
					cmd_acknowledge(msg);
				}
			}
			else if (strcmp(cmd, "!clear") == 0 || strcmp(cmd, "!c") == 0)
			{
				sqlite3_stmt* stmt = db_query("delete from DiscordPlaytime where user_id=?;");
				db_bind_text(stmt, 0, author_id);
				db_exec(stmt);
				cmd_acknowledge(msg);
			}
			else if (strcmp(cmd, "!stats") == 0)
			{
				s32 playing;
				s32 in_lobby;
				s32 available;
				stats(&playing, &in_lobby, &available);
				char response[MAX_PATH_LENGTH + 1] = {};
				snprintf(response, MAX_PATH_LENGTH, "Playing: %d\nIn lobby: %d\nLooking to play: %d\n", playing, in_lobby, available);
				msg_post(response);
			}
			else if (strcmp(cmd, "!help") == 0 || strcmp(cmd, "!h") == 0)
			{
				msg_post
				(
					"!play - indicate you want to play now\n"
					"!play <time range> - indicate desired play time, ex: Tue 2pm-3pm\n"
					"!time <time> - set your local time zone\n"
					"!clear - clear play times\n"
					"!stats - see who's online\n"
					"!schedule - see when people are playing\n"
				);
			}
			else if (strcmp(cmd, "!schedule") == 0 || strcmp(cmd, "!sched") == 0)
			{
				s32 time_offset_half_hour;
				if (user_time_offset_half_hour(author_id, &time_offset_half_hour))
				{
					Array<char> response;
					sqlite3_stmt* stmt = db_query("select start, end from DiscordPlaytime;");
					b8 has_entry = false;
					s64 offset_seconds = time_offset_half_hour * 30 * 60;
					s64 timestamp = s64(platform::timestamp());
					while (db_step(stmt))
					{
						char line[MAX_PATH_LENGTH + 1] = {};
						s64 start = db_column_int(stmt, 0);
						s64 end = db_column_int(stmt, 1);

						std::tm tm;

						char start_str[129] = {};
						if (start < timestamp)
							strcpy(start_str, "Now");
						else
						{
							start += offset_seconds;
							tm = *localtime((const time_t*)(&start));
							strftime(start_str, 128, "%A %I:%M%p", &tm);
						}

						char end_str[129] = {};
						end += offset_seconds;
						tm = *localtime((const time_t*)(&end));
						strftime(end_str, 128, "%A %I:%M%p", &tm);

						s32 length = snprintf(line, MAX_PATH_LENGTH, "%s - %s\n", start_str, end_str);

						s32 destination = response.length;
						response.resize(response.length + length);
						strncpy(&response[destination], line, length);
						has_entry = true;
					}

					if (!has_entry)
					{
						const char* msg = "Nothing scheduled.";
						s32 destination = response.length;
						response.resize(response.length + strlen(msg));
						strncpy(&response[destination], msg, strlen(msg));
					}
					response.add('\0');
					msg_post(response.data);
					db_finalize(stmt);
				}
			}
		}
	}

	void available_role_update_db(u64 user_id, b8 value)
	{
		sqlite3_stmt* stmt = db_query("update DiscordUser set member_available_role=? where id=?;");
		db_bind_int(stmt, 0, s64(value));
		db_bind_int(stmt, 1, s64(user_id));
		db_exec(stmt);
	}

	void available_role_callback_add(s32 code, const char* data, u64 user_data)
	{
		if (code == 204)
			available_role_update_db(user_data, true);
	}

	void available_role_callback_remove(s32 code, const char* data, u64 user_data)
	{
		if (code == 204)
			available_role_update_db(user_data, false);
	}

	void available_role_request(const char* member_id, const char* method, Http::Callback* callback)
	{
		CURL* curl = curl_easy_init();
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method); 
		struct curl_slist* headers = auth_headers(curl);
		headers = curl_slist_append(headers, "Content-Length: 0");
		headers = curl_slist_append(headers, "Expect:"); // initialize custom header list stating that Expect: 100-continue is not wanted
		char url[MAX_PATH_LENGTH + 1] = {};
		snprintf(url, MAX_PATH_LENGTH, "https://discordapp.com/api/v6/guilds/%s/members/%s/roles/%s", Settings::discord_guild_id, member_id, Settings::discord_available_role_id);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		u64 id = u64(atoll(member_id));
		Http::add(curl, callback, headers, id);
	}

	void poll_callback(s32 code, const char* data, u64 user_data)
	{
		if (code == 200 && data)
		{
			cJSON* json = cJSON_Parse(data);
			if (json)
			{
				// process messages
				{
					cJSON* msg = json->child;
					while (msg)
					{
						cJSON* author = cJSON_GetObjectItem(msg, "author");
						if (author && strncmp(Json::get_string(author, "id"), Settings::discord_bot_user_id, MAX_DISCORD_ID_LENGTH) != 0) // ignore messages from ourselves
						{
							if (state.last_poll_message_id[0]) // ignore all messages if this is the first poll we're doing
								msg_handle(msg);
						}
						msg = msg->next;
					}
				}

				// delete old playtime records
				s64 timestamp = s64(platform::timestamp());
				{
					sqlite3_stmt* stmt = db_query("delete from DiscordPlaytime where end < ?;");
					db_bind_int(stmt, 0, timestamp);
					db_exec(stmt);
				}

				// add members to available role
				{
					sqlite3_stmt* stmt = db_query("select DiscordPlaytime.user_id from DiscordPlaytime left join DiscordUser on DiscordPlaytime.user_id=DiscordUser.id where DiscordPlaytime.start <= ? and DiscordPlaytime.end > ? and DiscordUser.member_available_role=0;");
					db_bind_int(stmt, 0, timestamp);
					db_bind_int(stmt, 1, timestamp);
					while (db_step(stmt))
					{
						const char* member_id = db_column_text(stmt, 0);
						available_role_request(member_id, "PUT", &available_role_callback_add);
					}
					db_finalize(stmt);
				}

				// remove members from available role
				{
					sqlite3_stmt* stmt = db_query("select DiscordUser.id from DiscordUser where DiscordUser.member_available_role=1 and (select count(1) from DiscordPlaytime where DiscordPlaytime.user_id=DiscordUser.id and DiscordPlaytime.start <= ? and DiscordPlaytime.end > ?)=0;");
					db_bind_int(stmt, 0, timestamp);
					db_bind_int(stmt, 1, timestamp);
					while (db_step(stmt))
					{
						const char* member_id = db_column_text(stmt, 0);
						available_role_request(member_id, "DELETE", &available_role_callback_remove);
					}
					db_finalize(stmt);
				}

				// stat mention
				if (timestamp > state.last_stat_mention_timestamp + 60.0)
				{
					s32 playing;
					s32 in_lobby;
					s32 available;
					stats(&playing, &in_lobby, &available);

					s32 total = playing + in_lobby + available;
					if (total >= vi_max(1, state.last_stat_mention_total_players * 2)
						|| timestamp > state.last_stat_mention_timestamp + 30.0 * 60.0)
					{
						if (state.last_poll_message_id[0]) // if this is the first time we're polling, we just rebooted; don't send out the mention
						{
							char mention[MAX_PATH_LENGTH + 1] = {};
							snprintf(mention, MAX_PATH_LENGTH, "<@&%s>\nPlaying: %d\nIn lobby: %d\nLooking to play: %d\n", Settings::discord_available_role_id, playing, in_lobby, available);
							msg_post(mention);
						}
						state.last_stat_mention_timestamp = timestamp;
						state.last_stat_mention_total_players = total;
					}
				}

				// remember most recent message id
				if (json->child)
				{
					const char* id = Json::get_string(json->child, "id");
					if (id)
						strncpy(state.last_poll_message_id, id, MAX_DISCORD_ID_LENGTH);
				}
			}
		}
	}

	void poll()
	{
		CURL* curl = curl_easy_init();
		char url[MAX_PATH_LENGTH + 1] = {};
		if (state.last_poll_message_id[0])
			snprintf(url, MAX_PATH_LENGTH, "https://discordapp.com/api/v6/channels/%s/messages?after=%s", Settings::discord_channel_id, state.last_poll_message_id);
		else
			snprintf(url, MAX_PATH_LENGTH, "https://discordapp.com/api/v6/channels/%s/messages", Settings::discord_channel_id);
		Http::get_headers(url, &poll_callback, auth_headers(curl));
	}

	void update()
	{
		if (Master::global_timestamp - state.last_poll > 5.0)
		{
			state.last_poll = Master::global_timestamp;
			poll();
		}
	}
}


namespace Dashboard
{

b8 auth_failed(mg_connection* conn)
{
	mg_printf
	(
		conn, "%s",
		"HTTP/1.1 401 Access Denied\r\n"
		"WWW-Authenticate: Basic realm=\"Deceiver\"\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
	);
	mg_printf_http_chunk(conn, "%s", "Incorrect login");
	mg_send_http_chunk(conn, "", 0);
	return false;
}

b8 auth(mg_connection* conn, http_message* msg)
{
	char username[MAX_USERNAME + 1];
	char password[MAX_AUTH_KEY + 1];
	if (mg_get_http_basic_auth(msg, username, MAX_USERNAME, password, MAX_AUTH_KEY))
		return auth_failed(conn);
	if (strcmp(username, Settings::dashboard_username))
		return auth_failed(conn);
	char password_hash[41];
	sha1::hash(password, password_hash);
	if (strcmp(password_hash, Settings::dashboard_password_hash))
		return auth_failed(conn);
	return true;
}

void handle_static(mg_connection* conn, int ev, void* ev_data)
{
	if (ev == MG_EV_HTTP_REQUEST)
	{
		http_message* msg = (http_message*)ev_data;

		if (!auth(conn, msg))
			return;

		mg_http_serve_file(conn, msg, "dashboard.html", mg_mk_str("text/html"), mg_mk_str(""));
	}
}

void handle_api(mg_connection* conn, int ev, void* ev_data)
{
	if (ev == MG_EV_HTTP_REQUEST)
	{
		// GET
		http_message* msg = (http_message*)(ev_data);

		if (!auth(conn, msg))
			return;

		mg_printf
		(
			conn, "%s",
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: application/json\r\n"
			"Transfer-Encoding: chunked\r\n"
			"\r\n"
		);

		// JSON
		{
			cJSON* json = cJSON_CreateObject();

			// servers
			{
				cJSON* servers = cJSON_CreateArray();
				cJSON_AddItemToObject(json, "servers", servers);
				for (s32 i = 0; i < global.servers.length; i++)
				{
					Node* node = node_for_hash(global.servers[i]);
					cJSON* server = cJSON_CreateObject();
					cJSON_AddNumberToObject(server, "state", s32(node->state));
					{
						char addr[NET_MAX_ADDRESS];
						node->addr.str(addr);
						cJSON_AddStringToObject(server, "addr", addr);
					}
					if (node->server_state.id)
					{
						// multiplayer
						ServerConfig config;
						server_config_get(node->server_state.id, &config);
						cJSON_AddStringToObject(server, "config", config.name);
						cJSON_AddNumberToObject(server, "max_players", config.max_players);
						cJSON_AddNumberToObject(server, "players", config.max_players - node->server_state.player_slots);
					}
					else
					{
						// story mode
						cJSON_AddItemToObject(server, "config", nullptr);
						cJSON_AddNumberToObject(server, "max_players", 1);
						cJSON_AddNumberToObject(server, "players", 1 - node->server_state.player_slots);
					}
					cJSON_AddNumberToObject(server, "level_id", node->server_state.level);
					cJSON_AddNumberToObject(server, "region", s32(node->server_state.region));
					cJSON_AddItemToArray(servers, server);
				}
			}

			// clients
			{
				cJSON* clients = cJSON_CreateArray();
				cJSON_AddItemToObject(json, "clients", clients);
				for (auto i = global.nodes.begin(); i != global.nodes.end(); i++)
				{
					Node* node = &i->second;
					if (node->state == Node::State::ClientWaiting
						|| node->state == Node::State::ClientConnecting
						|| node->state == Node::State::ClientIdle)
					{
						cJSON* client = cJSON_CreateObject();
						cJSON_AddItemToArray(clients, client);
						cJSON_AddNumberToObject(client, "state", s32(node->state));
						if (node->client.user_key.id)
						{
							char username[MAX_USERNAME];
							username_get(node->client.user_key.id, username);
							cJSON_AddStringToObject(client, "username", username);
						}
						else
							cJSON_AddItemToObject(client, "username", nullptr);
					}
				}
			}

			{
				char* json_str = cJSON_Print(json);
				mg_printf_http_chunk(conn, "%s", json_str);
				free(json_str);
			}
			Json::json_free(json);
		}

		mg_send_http_chunk(conn, "", 0);
	}
	else if (ev == MG_EV_HTTP_MULTIPART_REQUEST)
	{
		http_message* msg = (http_message*)ev_data;

		if (!auth(conn, msg))
		{
			conn->flags |= MG_F_CLOSE_IMMEDIATELY;
			return;
		}
	}
	else if (ev == MG_EV_HTTP_PART_DATA)
	{
		mg_http_multipart_part* part = (mg_http_multipart_part*)ev_data;
		if (strcmp(part->var_name, "sql") == 0)
		{
			// execute SQL
			mg_printf
			(
				conn, "%s",
				"HTTP/1.1 200 OK\r\n"
				"Content-Type: text/plain\r\n"
				"Transfer-Encoding: chunked\r\n"
				"\r\n"
			);

			sqlite3_stmt* stmt;
			if (sqlite3_prepare_v2(global.db, part->data.p, s32(part->data.len), &stmt, nullptr))
				mg_printf_http_chunk(conn, "%s", sqlite3_errmsg(global.db));
			else
			{
				Array<char> line;
				while (true)
				{
					s32 result = sqlite3_step(stmt);
					if (result == SQLITE_ROW)
					{
						line.length = 0;
						s32 column_count = sqlite3_column_count(stmt);
						s32 length = 0;
						for (s32 i = 0; i < column_count; i++)
						{
							const char* value = (const char*)(sqlite3_column_text(stmt, i));
							if (!value)
								value = "[null]";
							length += s32(strlen(value)) + 1;
						}
						line.reserve(length + 1);
						for (s32 i = 0; i < column_count; i++)
						{
							const char* value = (const char*)(sqlite3_column_text(stmt, i));
							if (!value)
								value = "[null]";
							s32 length = s32(strlen(value)) + 1;
							sprintf(&line.data[line.length], "%s\t", value);
							line.length += length;
						}

						// add newline and null terminate
						line.add('\n');
						line.add('\0');

						mg_printf_http_chunk(conn, "%s", line.data);
					}
					else
						break;
				}

				if (sqlite3_finalize(stmt))
					mg_printf_http_chunk(conn, "%s", sqlite3_errmsg(global.db));
			}

			mg_send_http_chunk(conn, "", 0);
		}
		else
		{
			mg_printf
			(
				conn, "%s",
				"HTTP/1.1 400 Bad Request\r\n"
				"Content-Type: text/html\r\n"
				"Transfer-Encoding: chunked\r\n"
				"\r\n"
			);
			mg_printf_http_chunk(conn, "%s", "Bad Request");
			mg_send_http_chunk(conn, "", 0);
		}
	}
}

}

}

}

}

int main(int argc, char** argv)
{
	return VI::Net::Master::proc();
}
