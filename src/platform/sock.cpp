#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "sock.h"
#ifdef _WIN32
#include <WinSock2.h>
#pragma comment(lib, "wsock32.lib")
#else
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#endif

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
	{
		return error("Windows Sockets failed to start");
	}

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

b8 Address::equals(const Address& other) const
{
	return host == other.host && port == other.port;
}

s32 get_address(Address* address, const char* host, u16 port)
{
	if (host == NULL)
		address->host = INADDR_ANY;
	else
	{
		address->host = inet_addr(host);
		if (address->host == INADDR_NONE)
		{
			struct hostent* hostent = gethostbyname(host);
			if (hostent)
				memcpy(&address->host, hostent->h_addr, hostent->h_length);
			else
				return error("Invalid host name");
		}
	}

	address->port = port;

	return 0;
}

const char* host_to_str(u32 host)
{
	struct in_addr in;
	in.s_addr = host;

	return inet_ntoa(in);
}

s32 udp_open(Handle* sock, u32 port, u32 non_blocking)
{
	if (!sock)
		return error("Socket is NULL");

	// Create the socket
	sock->handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock->handle <= 0)
	{
		close(sock);
		return error("Failed to create socket");
	}

	// Bind the socket to the port
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	if (bind(sock->handle, (const struct sockaddr*)&address, sizeof(struct sockaddr_in)) != 0)
	{
		close(sock);
		return error("Failed to bind socket");
	}

	// Set the socket to non-blocking if neccessary
	if (non_blocking)
	{
#ifdef _WIN32
		if (ioctlsocket(sock->handle, FIONBIO, (unsigned long*)&non_blocking) != 0)
		{
			close(sock);
			return error("Failed to set socket to non-blocking");
		}
#else
		if (fcntl(sock->handle, F_SETFL, O_NONBLOCK, non_blocking) != 0)
		{
			close(sock);
			return error("Failed to set socket to non-blocking");
		}
#endif
	}

	sock->non_blocking = non_blocking;

	return 0;
}

s32 tcp_listen(Handle* sock, u16 port, u32 non_blocking)
{
	if (!sock)
		return error("Socket is NULL");

	// Create the socket
	sock->handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock->handle <= 0)
	{
		close(sock);
		return error("Failed to create socket");
	}

	// Bind the socket to the port
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	if (bind(sock->handle, (const struct sockaddr*)&address, sizeof(struct sockaddr_in)) != 0)
	{
		close(sock);
		return error("Failed to bind socket");
	}

	listen(sock->handle, 5); // backlog of 5 for reasons

	// Set the socket to non-blocking if neccessary
	if (non_blocking)
	{
#ifdef _WIN32
		if (ioctlsocket(sock->handle, FIONBIO, (unsigned long*)&non_blocking) != 0)
		{
			close(sock);
			return error("Failed to set socket to non-blocking");
		}
#else
		if (fcntl(sock->handle, F_SETFL, O_NONBLOCK, non_blocking) != 0)
		{
			close(sock);
			return error("Failed to set socket to non-blocking");
		}
#endif
	}

	sock->non_blocking = non_blocking;

	return 0;
}

TCPClient tcp_accept(Handle* sock)
{
	return accept(sock->handle, nullptr, nullptr);
}

s32 tcp_connect(Handle* sock, Address address, u32 non_blocking)
{
	if (!sock)
		return error("Socket is NULL");

	// create the socket
	sock->handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock->handle <= 0)
	{
		close(sock);
		return error("Failed to create socket");
	}

	struct sockaddr_in server_address;
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = address.host;
	server_address.sin_port = htons(address.port);

	if (connect(sock->handle, (const struct sockaddr*)&address, sizeof(struct sockaddr_in)) != 0)
	{
		close(sock);
		return error("Failed to bind socket");
	}

	// Set the socket to non-blocking if neccessary
	if (non_blocking)
	{
#ifdef _WIN32
		if (ioctlsocket(sock->handle, FIONBIO, (unsigned long*)&non_blocking) != 0)
		{
			close(sock);
			return error("Failed to set socket to non-blocking");
		}
#else
		if (fcntl(sock->handle, F_SETFL, O_NONBLOCK, non_blocking) != 0)
		{
			close(sock);
			return error("Failed to set socket to non-blocking");
		}
#endif
	}

	sock->non_blocking = non_blocking;

	return 0;
}

void close(Handle* socket)
{
	if (!socket)
		return;

	if (socket->handle)
	{
#ifdef _WIN32
		closesocket(socket->handle);
#else
		::close(socket->handle);
#endif
	}
}

s32 udp_send(Handle* socket, Address destination, const void* data, s32 size)
{
	if (!socket)
		return error("Socket is NULL");

	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = destination.host;
	address.sin_port = htons(destination.port);

	s32 sent_bytes = sendto(socket->handle, (const char*)data, size, 0, (const struct sockaddr*)&address, sizeof(struct sockaddr_in));
	if (sent_bytes != size)
		return error("Failed to send data");

	return 0;
}

s32 udp_receive(Handle* socket, Address* sender, void* data, s32 size)
{
	if (!socket)
		return error("Socket is NULL");

#ifdef _WIN32
	typedef s32 socklen_t;
#endif

	struct sockaddr_in from;
	socklen_t from_length = sizeof(from);

	s32 received_bytes = recvfrom(socket->handle, (char*)data, size, 0, (struct sockaddr*)&from, &from_length);
	if (received_bytes <= 0)
		return 0;

	sender->host = from.sin_addr.s_addr;
	sender->port = ntohs(from.sin_port);

	return received_bytes;
}

s32 tcp_send(Handle* sock, const void* data, s32 size)
{
	return send(sock->handle, (char*)data, size, 0);
}

s32 tcp_send(Handle* sock, TCPClient client, const void* data, s32 size)
{
	return send(client, (char*)data, size, 0);
}

s32 tcp_receive(Handle* sock, void* data, s32 size)
{
	return recv(sock->handle, (char*)data, size, 0);
}

s32 tcp_receive(Handle*, TCPClient*, void*, s32)
{
	// TODO
	return 0;
}

}


}