#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "sock.h"
#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "wsock32.lib")
#pragma comment(lib, "Ws2_32.lib")
#else
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#endif
#include <functional>

namespace VI
{


namespace Sock
{


static const char* g_error;

static s32 error(const char* message)
{
	g_error = message;

	return -1;
}

const char* get_error(void)
{
	return g_error;
}

s32 init(void)
{
#ifdef _WIN32
	WSADATA wsa_data;
	if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
		return error("Windows Sockets failed to start");

	return 0;
#else
	return 0;
#endif
}

void netshutdown(void)
{
#ifdef _WIN32
	WSACleanup();
#endif
}

b8 Host::equals(const Host& other) const
{
	if (type == other.type)
	{
		switch (type)
		{
			case Type::IPv4:
				return ipv4 == other.ipv4;
				break;
			case Type::IPv6:
			{
				for (s32 i = 0; i < 16; i++)
				{
					if (ipv6[i] != other.ipv6[i])
						return false;
				}
				return true;
				break;
			}
			default:
				vi_assert(false);
				break;
		}
	}
	return false;
}

b8 Address::equals(const Address& other) const
{
	return port == other.port && host.equals(other.host);
}

b8 Address::operator==(const Address& other) const
{
	return equals(other);
}

u64 Address::hash() const
{
	u64 result = 17;
	result = result * 31 + std::hash<s8>()(s8(host.type));
	switch (host.type)
	{
		case Sock::Host::Type::IPv4:
			result = result * 31 + std::hash<u32>()(host.ipv4);
			break;
		case Sock::Host::Type::IPv6:
		{
			u64 low = *((u64*)&host.ipv6[0]);
			u64 high = *((u64*)&host.ipv6[8]);
			result = result * 31 + std::hash<u64>()(low);
			result = result * 31 + std::hash<u64>()(high);
			break;
		}
		default:
			vi_assert(false);
			break;
	}
	result = result * 31 + std::hash<u16>()(port);
	return result;
}

void Address::str(char* out) const
{
	char buffer[MAX_ADDRESS_STRING];
	switch (host.type)
	{
		case Host::Type::IPv4:
		{
			struct in_addr in;
			in.s_addr = host.ipv4;
			inet_ntop(AF_INET, &in, buffer, MAX_ADDRESS_STRING);
			break;
		}
		case Host::Type::IPv6:
		{
			struct in6_addr in;
			memcpy(&in, host.ipv6, sizeof(in));
			inet_ntop(AF_INET6, &in, buffer, MAX_ADDRESS_STRING);
			break;
		}
		default:
			vi_assert(false);
			break;
	}
	sprintf(out, "%s:%hu", buffer, ntohs(port));
}

s32 get_address(Address* address, const char* host, u16 port)
{
	memset(address, 0, sizeof(address));
	address->port = htons(port);
	if (host)
	{
		struct addrinfo hints;
		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
		hints.ai_protocol = IPPROTO_UDP;

		struct addrinfo* result;

		if (getaddrinfo(host, nullptr, &hints, &result) || !result)
			return error("getaddrinfo failed");

		if (result->ai_addr->sa_family == AF_INET6)
		{
			address->host.type = Host::Type::IPv6;
			struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)(result->ai_addr);
			memcpy(address->host.ipv6, &ipv6->sin6_addr, sizeof(address->host.ipv6));
		}
		else // ipv4
		{
			vi_assert(result->ai_addr->sa_family == AF_INET);
			address->host.type = Host::Type::IPv4;
			struct sockaddr_in* ipv4 = (struct sockaddr_in*)(result->ai_addr);
			memcpy(&address->host.ipv4, &ipv4->sin_addr, sizeof(address->host.ipv4));
		}

		freeaddrinfo(result);
	}

	return 0;
}

s32 socket_bind(u64* handle, u16 port, s32 family)
{
	// create the socket
	*handle = socket(family, SOCK_DGRAM, IPPROTO_UDP);
	if (*handle <= 0)
		return error("Failed to create socket");

#if defined(IPV6_V6ONLY)
	if (family == AF_INET6)
	{
		// Disable IPv4 mapped addresses.
		s32 v6only = 1;
		if (setsockopt(*handle, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&v6only, sizeof(v6only)))
			return error("Failed to set IPv6-only socket option");
	}
#endif

	// bind the socket to the port
	{
		struct addrinfo hints;
		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = family;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
		hints.ai_protocol = IPPROTO_UDP;

		struct addrinfo* addr_list;

		char port_str[8] = {};
		sprintf(port_str, "%hu", port);
		if (getaddrinfo(nullptr, port_str, &hints, &addr_list) || !addr_list)
			return error("getaddrinfo failed");

		if (bind(*handle, addr_list->ai_addr, addr_list->ai_addrlen))
			return error("Failed to bind socket");

		freeaddrinfo(addr_list);
	}

	// set the socket to non-blocking
	{
#ifdef _WIN32
		unsigned long non_blocking = 1;
		if (ioctlsocket(*handle, FIONBIO, &non_blocking) != 0)
			return error("Failed to set socket to non-blocking");
#else
		if (fcntl(*handle, F_SETFL, O_NONBLOCK, non_blocking) != 0)
			return error("Failed to set socket to non-blocking");
#endif
	}

	return 0;
}

s32 udp_open(Handle* sock, u32 port)
{
	if (socket_bind(&sock->ipv4, port, AF_INET))
		return -1;
	if (socket_bind(&sock->ipv6, port, AF_INET6))
		return -1;
	return 0;
}

void close(Handle* socket)
{
	if (!socket)
		return;

	if (socket->ipv4)
	{
#ifdef _WIN32
		closesocket(socket->ipv4);
#else
		::close(socket->ipv4);
#endif
	}

	if (socket->ipv6)
	{
#ifdef _WIN32
		closesocket(socket->ipv6);
#else
		::close(socket->ipv6);
#endif
	}
}

s32 udp_send(Handle* socket, const Address& destination, const void* data, s32 size)
{
	struct sockaddr address;
	u64 handle;
	size_t addr_length;
	memset(&address, 0, sizeof(address));
	switch (destination.host.type)
	{
		case Host::Type::IPv4:
		{
			struct sockaddr_in* ipv4 = (struct sockaddr_in*)(&address);
			ipv4->sin_family = AF_INET;
			ipv4->sin_port = destination.port;
			ipv4->sin_addr.s_addr = destination.host.ipv4;
			handle = socket->ipv4;
			addr_length = sizeof(struct sockaddr_in);
			break;
		}
		case Host::Type::IPv6:
		{
			struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)(&address);
			ipv6->sin6_family = AF_INET6;
			ipv6->sin6_port = destination.port;
			memcpy(&ipv6->sin6_addr, &destination.host.ipv6, sizeof(ipv6->sin6_addr));
			handle = socket->ipv6;
			addr_length = sizeof(struct sockaddr_in6);
			break;
		}
		default:
			vi_assert(false);
			break;
	}

	s32 sent_bytes = sendto(handle, (const char*)data, size, 0, (const struct sockaddr*)&address, addr_length);
	if (sent_bytes != size)
		return error("Failed to send data");

	return 0;
}

s32 udp_receive(Handle* socket, Address* sender, void* data, s32 size)
{
#ifdef _WIN32
	typedef s32 socklen_t;
#endif

	struct sockaddr from;
	socklen_t from_length = sizeof(from);

	s32 received_bytes = recvfrom(socket->ipv4, (char*)data, size, 0, &from, &from_length);
	if (received_bytes <= 0)
	{
		received_bytes = recvfrom(socket->ipv6, (char*)data, size, 0, &from, &from_length);
		if (received_bytes <= 0)
			return 0;
	}

	if (from.sa_family == AF_INET6)
	{
		sender->host.type = Host::Type::IPv6;
		const struct sockaddr_in6* ipv6 = (const struct sockaddr_in6*)(&from);
		memcpy(sender->host.ipv6, &ipv6->sin6_addr, sizeof(ipv6->sin6_addr));
		sender->port = ipv6->sin6_port;
	}
	else
	{
		sender->host.type = Host::Type::IPv4;
		const struct sockaddr_in* ipv4 = (const struct sockaddr_in*)(&from);
		sender->host.ipv4 = ipv4->sin_addr.s_addr;
		sender->port = ipv4->sin_port;
	}

	return received_bytes;
}

}


}