#pragma once
#include <stdint.h>
#include <cstddef>
#include "game/constants.h"

namespace VI
{

typedef bool b8;

typedef std::size_t memory_index;
	
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float r32;
typedef double r64;

struct GameTime
{
	r32 total;
	r32 delta;
};

struct InputState;
struct RenderSync;

struct Update
{
	InputState* input;
	const InputState* last_input;
	GameTime time;
};

typedef s16 RenderMask;

typedef s16 AssetID;
const AssetID AssetNull = AssetID(-1);

typedef s16 ID;
const ID IDNull = ID(MAX_ENTITIES);

enum class Ability
{
	Sensor,
	Minion,
	Teleporter,
	Rocket,
	ContainmentField,
	Sniper,
	Decoy,
	Grenade,
	Bolter,
	count,
	None,
};

enum class Upgrade
{
	Sensor,
	Minion,
	Teleporter,
	Rocket,
	ContainmentField,
	Sniper,
	Decoy,
	Grenade,
	Bolter,
	count,
	None = count,
};

namespace Net
{
	enum class MessageSource // included here to prevent having to include net.h everywhere
	{
		Remote,
		Loopback,
		Invalid, // message is malicious or something; deserialize it but ignore
		count,
	};
}


}
