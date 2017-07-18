#pragma once
#include "types.h"
#include "game.h"

namespace VI
{

typedef void (*ScriptFunction)(const EntityFinder&);
typedef b8 (*NetMsgFunction)(Net::StreamRead*, Net::MessageSource);

struct Script
{
	static Script list[];
	static s32 count;

	static AssetID find(const char*);
	static b8 net_msg(Net::StreamRead*, Net::MessageSource);
	static Net::StreamWrite* net_msg_new(NetMsgFunction);

	const char* name;
	ScriptFunction function;
	NetMsgFunction net_callback;
};

namespace Scripts
{

namespace Docks
{
	void play();
}

}

}
