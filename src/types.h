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

enum class DroneVulnerability : s8
{
	None,
	All,
	DroneBolts,
	ActiveArmor,
	count,
};

enum class ServerListType : s8
{
	Top,
	Recent,
	Mine,
	count,
};

enum class Region : s8
{
	USEast,
	USWest,
	Europe,
	count,
	Invalid = count,
};

enum class Ability : s8
{
	Bolter,
	ActiveArmor,
	Rectifier,
	MinionBoost,
	Shotgun,
	Sniper,
	ForceField,
	Grenade,
	count,
	None = count,
};

enum class Upgrade : s8
{
	Bolter,
	ActiveArmor,
	Rectifier,
	MinionBoost,
	Shotgun,
	Sniper,
	ForceField,
	Grenade,
	count,
	None = count,
};

enum class Resource : s8
{
	Energy,
	AccessKeys,
	DroneKits,
	ConsumableCount,
	DoubleJump = ConsumableCount,
	ExtendedWallRun,
	Grapple,
	AudioLog,
	count,
};

enum class ZoneState : s8
{
	Locked,
	ParkourUnlocked,
	ParkourOwned,
	PvpUnlocked,
	PvpFriendly,
	PvpHostile,
	count,
};

enum class GameType : s8
{
	Assault,
	Deathmatch,
	CaptureTheFlag,
	count,
};

enum class SessionType : s8
{
	Story,
	Multiplayer,
	count,
};

enum class StoryModeTeam : s8
{
	Attack,
	Defend,
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

	namespace Master
	{
		// type of authentication used to obtain a user key from the master server
		enum class AuthType : s8
		{
			None,
			Itch,
			Steam,
			GameJolt,
			ItchOAuth,
			count,
		};

		// role of a user in a specific server
		enum class Role : s8
		{
			None,
			Banned,
			Allowed,
			Admin,
			count,
		};
	}
}

namespace AI
{
	typedef s8 Team;
	typedef s8 TeamMask;

	const TeamMask TeamAll = 0b1111;
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
		r32 spawn_time;
		AI::Team team;
		Upgrade upgrade_priorities[s32(Upgrade::count)];
	};
}

}
