#pragma once

#include "types.h"
#include "data/array.h"
#include "vi_assert.h"
#include "lmath.h"
#include "data/entity.h"
#include "net_serialize.h"

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

namespace Master
{
	struct ServerState;
};

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
	Parkour,
	Interactable,
	ClientSetup,
	Bolt,
	Grenade,
	EntityName,
	InitSave,
	InitDone,
	LoadingDone,
	TimeSync,
	Tram,
	Turret,
	TransitionLevel,
	Overworld,
#if DEBUG
	DebugCommand,
#endif
	count,
};

struct TransformState
{
	Vec3 pos;
	Quat rot;
	Resolution resolution;
	Ref<Transform> parent;
	Revision revision;
};

struct MinionState
{
	r32 rotation;
	r32 animation_time;
	AssetID animation;
};

struct PlayerManagerState
{
	r32 spawn_timer;
	r32 state_timer;
	s32 upgrades;
	Ability abilities[MAX_ABILITIES] = { Ability::None, Ability::None, Ability::None };
	Upgrade current_upgrade = Upgrade::None;
	Ref<Entity> instance;
	s16 energy;
	s16 kills;
	s16 deaths;
	s16 respawns;
	b8 active;
};

struct DroneState
{
	Revision revision; // not synced over network; only stored on server
	s8 charges;
	b8 active;
};

struct StateFrame
{
	TransformState transforms[MAX_ENTITIES];
	PlayerManagerState players[MAX_PLAYERS];
	MinionState minions[MAX_ENTITIES];
	r32 timestamp;
	Bitmask<MAX_ENTITIES> transforms_active;
	Bitmask<MAX_ENTITIES> minions_active;
	DroneState drones[MAX_PLAYERS];
	SequenceID sequence_id;
};

b8 init();
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
	count,
};

#if SERVER
namespace Server
{
	enum class Mode : s8
	{
		Idle,
		Loading,
		Waiting,
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

	void connect(Sock::Address);
	void connect(const char*, u16);
	void replay(const char* = nullptr);
	void replay_file_add(const char*);
	s32 replay_file_count();
	b8 allocate_server(const Master::ServerState&);

	b8 lagging();

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
void transform_absolute(const StateFrame&, s32, Vec3*, Quat* = nullptr);
r32 timestamp();

}


}