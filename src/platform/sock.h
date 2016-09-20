#pragma once
#include "types.h"

namespace VI
{

namespace Sock
{


typedef struct {
	u32 host;
	u16 port;
} Address;

typedef struct {
	int handle;
	int non_blocking;
	int ready;
} Handle;

const char* get_error(void);
s32 init(void);
void netshutdown(void);
s32 get_address(Address* address, const char* host, u16 port);
const char* host_to_str(u32 host);

void close(Handle* socket);

int udp_open(Handle* socket, u32 port, u32 non_blocking);
int udp_send(Handle* socket, Address destination, const void* data, s32 size);
int udp_receive(Handle* socket, Address* sender, void* data, s32 size);


}


}
