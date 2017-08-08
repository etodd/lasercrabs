#pragma once
#include "types.h"
#include "net_serialize.h"

namespace VI
{

namespace Sock
{

#define MAX_ADDRESS_STRING 66

struct Host
{
	enum class Type : s8
	{
		IPv4,
		IPv6,
		count,
	};

	union
	{
		u32 ipv4;
		char ipv6[16];
	};
	Type type;

	b8 equals(const Host&) const;
};

struct Address
{
	template<typename Stream> static b8 serialize(Stream* p, Address* a)
	{
		serialize_enum(p, Host::Type, a->host.type);
		switch (a->host.type)
		{
			case Host::Type::IPv4:
				serialize_u32(p, a->host.ipv4);
				break;
			case Host::Type::IPv6:
				serialize_bytes(p, (u8*)a->host.ipv6, 16);
				break;
			default:
			{
				vi_assert(false);
				break;
			}
		}
		serialize_u16(p, a->port);
		return true;
	}

	Host host;
	u16 port;

	b8 equals(const Address&) const;
	b8 operator==(const Address&) const;
	void str(char*) const; // needs MAX_ADDRESS_STRING space
	u64 hash() const;
};

struct Handle
{
	u64 ipv4;
	u64 ipv6;
};

const char* get_error(void);
s32 init(void);
void netshutdown(void);
s32 get_address(Address*, const char*, u16);

void close(Handle*);

s32 udp_open(Handle*, u32);
s32 udp_send(Handle*, const Address&, const void*, s32);
s32 udp_receive(Handle*, Address*, void*, s32);


}


}