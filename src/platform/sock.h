#pragma once
#include "types.h"
#include <functional>

namespace VI
{

namespace Sock
{


struct Address
{
	u32 host;
	u16 port;

	b8 equals(const Address&) const;
	b8 operator==(const Address&) const;
};

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


}


}


namespace std
{
	using namespace VI;
	template <> struct hash<Sock::Address>
	{
		std::size_t operator()(const Sock::Address& addr) const
		{
			size_t result = 17;
			result = result * 31 + std::hash<u32>()(addr.host);
			result = result * 31 + std::hash<u16>()(addr.port);
			return result;
		}
	};
}
