#pragma once

#include "types.h"
#include "data/array.h"
#include "vi_assert.h"
#include "game/team.h"

namespace VI
{

#define NET_TICK_RATE (1.0f / 60.0f)
#define NET_SYNC_TOLERANCE_POS 0.3f
#define NET_SYNC_TOLERANCE_ROT 0.1f
#define NET_INTERPOLATION_DELAY ((NET_TICK_RATE * 5.0f) + 0.02f)
#define NET_MAX_PACKET_SIZE 2000

struct Entity;
struct Awk;
struct PlayerHuman;

namespace Sock
{
	struct Address;
}

namespace Net
{

struct StreamRead;
struct StreamWrite;

// borrows heavily from https://github.com/networkprotocol/libyojimbo

typedef u8 SequenceID;

enum class Resolution
{
	Low,
	High,
	count,
};

enum class MessageType
{
	Noop,
	EntityCreate,
	EntityRemove,
	InitDone,
	LoadingDone,
	Awk,
	PlayerControlHuman,
	count,
};

enum class MessageSource
{
	Remote,
	Loopback,
	Invalid, // message is malicious or something; deserialize it but ignore
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

struct PlayerManagerState
{
	r32 spawn_timer;
	r32 state_timer;
	s32 upgrades;
	Ability abilities[MAX_ABILITIES] = { Ability::None, Ability::None, Ability::None };
	Upgrade current_upgrade = Upgrade::None;
	Ref<Entity> entity;
	s16 credits;
	s16 kills;
	s16 respawns;
	b8 active;
};

struct AwkState
{
	s8 charges;
	b8 active;
};

struct StateFrame
{
	TransformState transforms[MAX_ENTITIES];
	PlayerManagerState players[MAX_PLAYERS];
	r32 timestamp;
	Bitmask<MAX_ENTITIES> transforms_active;
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
		Waiting,
		Loading,
		Active,
		count,
	};

	Mode mode();
	s32 expected_clients();
}
#else
namespace Client
{
	enum class Mode
	{
		Disconnected,
		Connecting,
		Acking,
		Loading,
		Connected,
	};

	Mode mode();

	void connect(const char*, u16);
}
#endif

void term();
void reset();

StreamWrite* msg_new(MessageType);
StreamWrite* msg_new_local(MessageType);
b8 msg_finalize(StreamWrite*);
r32 rtt(const PlayerHuman*);
const StateFrame* state_frame_by_timestamp(r32);
void transform_absolute(const StateFrame&, s32, Vec3*, Quat*);

}


}
