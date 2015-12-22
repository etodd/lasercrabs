#pragma once

namespace VI
{

struct GameTime
{
	float total;
	float delta;
};

struct InputState;

struct Update
{
	const InputState* input;
	const InputState* last_input;
	GameTime time;
};

typedef int AssetID;
const AssetID AssetNull = -1;
typedef int AssetRef;

}
