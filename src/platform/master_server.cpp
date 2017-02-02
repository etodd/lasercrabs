#define _AMD64_

#include "game/master.h"

#include <thread>
#if _WIN32
#include <Windows.h>
#endif
#include <time.h>
#include <chrono>
#include "sock.h"

namespace VI
{

namespace platform
{

	u64 timestamp()
	{
		time_t t;
		::time(&t);
		return u64(t);
	}

	r64 time()
	{
		return r64(std::chrono::high_resolution_clock::now().time_since_epoch().count()) / 1000000000.0;
	}

	void sleep(r32 time)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(s64(time * 1000.0f)));
	}

}

namespace Net
{

namespace Master
{

	struct Node // could be a server or client
	{
		enum class State
		{
			ServerStoryMode,
			ServerMultiplayer,
			ServerIdle,
			ClientWaitingStoryMode,
			ClientWaitingMultiplayer,
			ClientIdle,
			count,
		};

		Channel channel;
		r64 last_message_timestamp;
		State state;
		ServerState server_state;
	};

	Array<Node> nodes;
	Sock::Handle sock;

	Node* node_for_address(Sock::Address addr)
	{
		for (s32 i = 0; i < nodes.length; i++)
		{
			if (nodes[i].channel.addr.equals(addr))
				return &nodes[i];
		}
		Node* n = nodes.add();
		new (n) Node();
		n->channel.addr = addr;
		return n;
	}

	b8 packet_handle(StreamRead* p, Node* node)
	{
		using Stream = StreamRead;
		SequenceID seq;
		serialize_int(p, SequenceID, seq, 0, NET_SEQUENCE_COUNT - 1);
		Message type;
		serialize_enum(p, Message, type);
		node->channel.received(type, seq, &sock);
		printf("Received %d bytes from %s:%hd", p->bytes_total, Sock::host_to_str(node->channel.addr.host), node->channel.addr.port);
		return true;
	}

	s32 proc()
	{
		if (Sock::init())
			return 1;

		if (Sock::udp_open(&sock, 3497, true))
		{
			printf("%s\n", Sock::get_error());
			return 1;
		}

		while (true)
		{
			for (s32 i = 0; i < nodes.length; i++)
				nodes[i].channel.update(&sock);

			Sock::Address addr;
			StreamRead packet;
			s32 bytes_read = Sock::udp_receive(&sock, &addr, packet.data.data, NET_MAX_PACKET_SIZE);
			packet.resize_bytes(bytes_read);
			if (bytes_read > 0)
			{
				if (packet.read_checksum())
				{
					packet_decompress(&packet, bytes_read);
					packet_handle(&packet, node_for_address(addr));
				}
				else
					vi_debug("%s", "Discarding packet due to invalid checksum.");
			}
			else
				platform::sleep(1.0f / 60.0f);
		}

		return 0;
	}

}

}

}

int main(int argc, char** argv)
{
	return VI::Net::Master::proc();
}