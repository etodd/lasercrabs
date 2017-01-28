#pragma once
#include "types.h"

namespace VI
{

namespace Sock
{


struct Address
{
	u32 host;
	u16 port;
	b8 equals(const Address&) const;
};

typedef u64 TCPClient;

struct Handle
{
	u64 handle;
	s32 non_blocking;
	s32 ready;
};

const char* get_error(void);
s32 init(void);
void netshutdown(void);
s32 get_address(Address*, const char*, u16);
const char* host_to_str(u32);

void close(Handle*);

s32 udp_open(Handle*, u32, u32);
s32 udp_send(Handle*, Address, const void*, s32);
s32 udp_receive(Handle*, Address*, void*, s32);

s32 tcp_listen(Handle*, u16, u32);
s32 tcp_connect(Handle*, Address, u32);
TCPClient tcp_accept(Handle*);
s32 tcp_send(Handle*, const void*, s32);
s32 tcp_send(Handle*, TCPClient, const void*, s32);
s32 tcp_receive(Handle*, void*, s32);
s32 tcp_receive(Handle*, TCPClient*, void*, s32);


}


}
