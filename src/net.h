#pragma once

#include "types.h"
#include "data/array.h"
#include "vi_assert.h"
#include "lmath.h"
#include "data/entity.h"

namespace VI
{

struct Entity;
struct Awk;
struct PlayerHuman;
struct Transform;

namespace Sock
{
	struct Address;
}

namespace Net
{

struct StreamRead;
struct StreamWrite;

// borrows heavily from https://github.com/networkprotocol/libyojimbo

typedef u16 SequenceID;

enum class Resolution
{
	Low,
	Medium,
	High,
	count,
};

enum class MessageType
{
	Noop,
	EntityCreate,
	EntityRemove,
	Awk,
	PlayerControlHuman,
	Health,
	EnergyPickup,
	PlayerManager,
	ParticleEffect,
	ControlPoint,
	Team,
	Parkour,
	Interactable,
	ClientSetup,
	InitDone,
	LoadingDone,
	TimeSync,
	Tram,
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
	r32 attack_timer;
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
	s16 credits;
	s16 kills;
	s16 respawns;
	b8 active;
};

struct AwkState
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
	AwkState awks[MAX_PLAYERS];
	SequenceID sequence_id;
};

b8 init();
void update_start(const Update&);
void update_end(const Update&);
b8 finalize(Entity*);
b8 remove(Entity*);
extern b8 show_stats;

#if SERVER
namespace Server
{
	enum Mode
	{
		Loading,
		Waiting,
		Active,
		count,
	};

	Mode mode();
	s32 expected_clients();
	void transition_level();
	void level_loading();
	void level_loaded();
}
#else
namespace Client
{
	enum class Mode
	{
		Disconnected,
		Connecting,
		Loading,
		Connected,
	};

	Mode mode();

	void connect(Sock::Address);
	void connect(const char*, u16);

	b8 lagging();

	Sock::Address server_address();

	b8 execute(const char*);
}
#endif

void term();
void reset();

b8 transform_filter(const Entity*);
StreamWrite* msg_new(MessageType);
StreamWrite* msg_new_local(MessageType);
b8 msg_finalize(StreamWrite*);
r32 rtt(const PlayerHuman*);
b8 state_frame_by_timestamp(StateFrame*, r32);
void transform_absolute(const StateFrame&, s32, Vec3*, Quat* = nullptr);
r32 timestamp();

}


}
