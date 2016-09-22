#pragma once

#include "types.h"
#include "data/array.h"
#include "vi_assert.h"

namespace VI
{

namespace Sock
{
	struct Address;
}

namespace Net
{

struct StreamRead;
struct StreamWrite;

// borrows heavily from https://github.com/networkprotocol/libyojimbo

#define MAX_PACKET_SIZE 1500

b8 init();
void update(const Update&);

#if SERVER
namespace Server
{
}
#else
namespace Client
{
	void connect(const char*, u16);
}
#endif

void term();


}


}