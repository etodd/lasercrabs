#pragma once
#include <stdint.h>
#include <cstddef>
#include "game/constants.h"

namespace VI
{

typedef bool b8;
	
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef std::size_t memory_index;

typedef float r32;
typedef double r64;

typedef s8 Family;
typedef s16 Revision;
typedef s64 ComponentMask;

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
	GameTime real_time;
};

typedef s16 RenderMask;

typedef s16 AssetID;
const AssetID AssetNull = AssetID(-1);

typedef s16 ID;
const ID IDNull = ID(MAX_ENTITIES);

enum class ServerListType : s8
{
	Browse,
	Recent,
	Mine,
	count,
};

enum class Ability : s8
{
	Bolter,
	ActiveArmor,
	Minion,
	Sensor,
	ForceField,
	Sniper,
	Grenade,
	count,
	None,
};

enum class Upgrade : s8
{
	Bolter,
	ActiveArmor,
	Minion,
	Sensor,
	ForceField,
	Sniper,
	Grenade,
	count,
	None = count,
};

enum class Resource : s8
{
	Energy,
	AccessKeys,
	Drones,
	count,
};

enum class ZoneState : s8
{
	Locked,
	ParkourUnlocked,
	PvpFriendly,
	PvpHostile,
	count,
};

enum class GameType : s8
{
	Assault,
	Deathmatch,
	count,
};

enum class SessionType : s8
{
	Story,
	Multiplayer,
	count,
};

namespace Net
{
	enum class MessageSource : s8 // included here to prevent having to include net.h everywhere
	{
		Remote,
		Loopback,
		Invalid, // message is malicious or something; deserialize it but ignore
		count,
	};
}

namespace AI
{
	typedef s8 Team;
	typedef s32 TeamMask;

	const Team TeamNone = -1;

	struct Config
	{
		r32 interval_memory_update;
		r32 interval_low_level;
		r32 interval_high_level;
		r32 inaccuracy_min;
		r32 inaccuracy_range;
		r32 aim_timeout;
		r32 aim_speed;
		r32 aim_min_delay;
		r32 dodge_chance;
		r32 spawn_time;
		AI::Team team;
	};
}

}
