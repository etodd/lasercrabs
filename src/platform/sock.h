#pragma once
#include "types.h"
#include "net_serialize.h"

#define NET_MAX_ADDRESS 68

namespace VI
{

namespace Sock
{

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
	u32 scope_id;
	Type type;

	b8 equals(const Host&) const;
};

struct Address
{
	template<typename Stream> static b8 serialize(Stream* p, Address* a)
	{
		serialize_enum(p, Host::Type, a->host.type);
		serialize_u16(p, a->port);
		if (a->port)
		{
			switch (a->host.type)
			{
				case Host::Type::IPv4:
					serialize_u32(p, a->host.ipv4);
					break;
				case Host::Type::IPv6:
				{
					serialize_bytes(p, (u8*)a->host.ipv6, 16);
					b8 has_scope = a->host.scope_id;
					serialize_bool(p, has_scope);
					if (has_scope)
						serialize_u32(p, a->host.scope_id);
					else if (Stream::IsReading)
						a->host.scope_id = 0;
					break;
				}
				default:
				{
					vi_assert(false);
					break;
				}
			}
		}
		else if (Stream::IsReading)
			memset(&a->host.ipv6, 0, sizeof(a->host.ipv6));
		return true;
	}

	static s32 get(Address*, const char*, u16);

	Host host;
	u16 port;

	b8 equals(const Address&) const;
	b8 operator==(const Address&) const;
	void str(char*) const; // needs NET_MAX_ADDRESS space
	void str_ip_only(char*) const; // needs NET_MAX_ADDRESS space
	u64 hash() const;
};

struct Handle
{
	u64 ipv4;
	u64 ipv6;
};

const char* get_error(void);
void init();
void netshutdown(void);

void close(Handle*);

s32 udp_open(Handle*, u32 = 0);
s32 udp_send(Handle*, const Address&, const void*, s32);
s32 udp_receive(Handle*, Address*, void*, s32);


}


}
