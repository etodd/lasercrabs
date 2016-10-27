#pragma once

#include "types.h"
#include "data/array.h"
#include "vi_assert.h"

namespace VI
{

#define NET_TICK_RATE (1.0f / 60.0f)
#define NET_SYNC_TOLERANCE_POS 0.3f
#define NET_SYNC_TOLERANCE_ROT 0.1f
#define NET_INTERPOLATION_DELAY ((NET_TICK_RATE * 5.0f) + 0.02f)

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

enum class MessageType
{
	Noop,
	EntityCreate,
	EntityRemove,
	InitDone,
	Awk,
	PlayerControlHuman,
	count,
};

enum class MessageSource
{
	Remote,
	Loopback,
	count,
};

#define MAX_PACKET_SIZE 2000

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

	extern Mode mode;
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

	extern Mode mode;

	void connect(const char*, u16);
}
#endif

void term();

StreamWrite* msg_new(MessageType);
StreamWrite* msg_new_local(MessageType);
b8 msg_finalize(StreamWrite*);
r32 rtt(const PlayerHuman*);

}


}