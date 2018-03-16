#pragma once

#include "types.h"
#include "data/array.h"
#include "vi_assert.h"
#include "lmath.h"
#include "data/entity.h"
#include "net_serialize.h"
#include "game/master.h"

namespace VI
{

struct Entity;
struct Drone;
struct PlayerHuman;
struct Transform;

namespace Sock
{
	struct Address;
}

namespace Net
{

// borrows heavily from https://github.com/networkprotocol/libyojimbo

enum class MessageType : s8
{
	Noop,
	EntityCreate,
	EntityRemove,
	Drone,
	PlayerControlHuman,
	Health,
	Battery,
	PlayerManager,
	ParticleEffect,
	Team,
	ClientSetup,
	Bolt,
	Grenade,
	UpgradeStation,
	InitDone,
	LoadingDone,
	TimeSync,
	Parkour,
	Turret,
	Glass,
	Flag,
	TransitionLevel,
	AddPlayer,
	DebugCommand,
	LowLatencyInterpolation,
	count,
};

struct TransformState
{
	Quat rot;
	Vec3 pos;
	Vec3 target_local_offset;
	Vec3 target_velocity;
	Ref<Transform> parent;
	Revision revision;
	Resolution resolution;
};

struct WalkerState
{
	r32 rotation;
	AnimationLayer animation;
	Revision revision;
};

struct PlayerManagerState
{
	r32 spawn_timer;
	s16 energy;
	b8 active;
};

struct DroneState
{
	r32 cooldown;
	r32 cooldown_ability_switch;
	DroneCollisionState collision_state;
	Revision revision;
	b8 active;
};

struct ParkourStateFrame
{
	Vec3 wall_normal;
	Vec3 model_offset;
	r32 lean;
	AnimationLayer animations[MAX_ANIMATIONS - 1];
	Revision revision;
	b8 active;
};

struct StateFrame
{
	TransformState transforms[MAX_ENTITIES];
	PlayerManagerState players[MAX_PLAYERS];
	WalkerState walkers[MAX_MINIONS * 2];
	DroneState drones[MAX_PLAYERS];
	ParkourStateFrame parkours[MAX_PLAYERS];
	r32 timestamp;
	Bitmask<MAX_ENTITIES> transforms_active;
	Bitmask<MAX_MINIONS * 2> walkers_active;
	SequenceID sequence_id;
};

void init();
r32 interpolation_delay(const PlayerHuman*);
r32 tick_rate();
void update_start(const Update&);
void update_end(const Update&);
void finalize(Entity*);
void finalize_child(Entity*);
b8 remove(Entity*);
extern b8 show_stats;

enum class DisconnectReason : s8
{
	Timeout,
	SequenceGap,
	ServerResetting,
	ServerFull,
	WrongVersion,
	AuthFailed,
	Kicked,
	count,
};

b8 master_user_role_set(u32, u32, Master::Role);

#if SERVER
namespace Server
{
	enum class Mode : s8
	{
		Idle,
		Loading,
		Active,
		count,
	};


	Mode mode();
	void transition_level();
	void level_unloading();
	void level_unloaded();
	void level_loading();
	void level_loaded();
	b8 sync_time();
	r32 rtt(const PlayerHuman*, SequenceID);
	ID client_id(const PlayerHuman*);
	void player_deleting(const PlayerHuman*);
	void client_force_disconnect(ID, DisconnectReason);
	void admin_set(PlayerHuman*, b8);
}
#else
namespace Client
{
	enum class Mode : s8
	{
		Disconnected,
		ContactingMaster,
		Connecting,
		Loading,
		Connected,
		count,
	};

	enum class MasterError : s8
	{
		None,
		WrongVersion,
		Timeout,
		count,
	};

	enum class ReplayMode : s8
	{
		None,
		Replaying,
		Recording,
		count,
	};

	Mode mode();
	
	extern MasterError master_error;
	extern DisconnectReason disconnect_reason;

	b8 master_send_auth();
	void connect(Sock::Address);
	void connect(const char*, u16);
	void replay(const char* = nullptr);
	void replay_file_add(const char*);
	s32 replay_file_count();
	b8 lagging();
	b8 master_request_ascension();
	b8 master_request_server(u32, const char* = nullptr, AssetID = AssetNull, StoryModeTeam = StoryModeTeam::Attack);
	Master::ClientConnectionStep connection_step();
	s8 wait_slot_queue_position();
	b8 master_friendship_get(u32);
	b8 master_friend_remove(u32);
	b8 master_friend_add(u32);
	b8 master_save_server_config(const Master::ServerConfig&, u32);
	b8 master_request_server_list(ServerListType, s32);
	void master_keepalive();
	void master_cancel_outgoing();
	b8 master_request_server_details(u32, u32);
	b8 ping(const Sock::Address&, u32);
	b8 add_player(s8);

	ReplayMode replay_mode();

	Sock::Address server_address();

	b8 execute(const char*);
}
#endif

void term();
void reset();

StreamWrite* msg_new(MessageType);
StreamWrite* msg_new_local(MessageType);
b8 msg_finalize(StreamWrite*);
r32 rtt(const PlayerHuman*);
b8 state_frame_by_timestamp(StateFrame*, r32);
void transform_absolute(const StateFrame&, s32, Vec3*, Quat* = nullptr, Vec3* = nullptr);
r32 timestamp();
b8 player_is_admin(const PlayerHuman*);

}


}
