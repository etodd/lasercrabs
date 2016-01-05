#pragma once

namespace VI
{

struct GameTime
{
	float total;
	float delta;
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

typedef unsigned short RenderMask;

typedef int AssetID;
const AssetID AssetNull = -1;
typedef int AssetRef;

}
