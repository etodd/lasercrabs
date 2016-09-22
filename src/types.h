#pragma once
#include <stdint.h>
#include <cstddef>

namespace VI
{

#define MAX_ENTITIES 2048

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

typedef u16 RenderMask;

typedef u16 AssetID;
const AssetID AssetNull = AssetID(-1);

typedef u16 ID;
const ID IDNull = ID(MAX_ENTITIES);


}
