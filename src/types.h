#pragma once
#include <stdint.h>
#include <cstddef>

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
	const InputState* input;
	const InputState* last_input;
	RenderSync* render;
	GameTime time;
};

typedef u16 RenderMask;

typedef s32 AssetID;
const AssetID AssetNull = -1;
typedef s32 AssetRef;

typedef u16 ID;
const ID IDNull = (ID)-1;

}
