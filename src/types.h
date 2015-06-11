#pragma once

#include <cstdlib>

struct GameTime
{
	float total;
	float delta;
};

struct InputState
{
	char keys[348 + 1];
	double cursor_x;
	double cursor_y;
	bool mouse;
	int width;
	int height;
};

struct Update
{
	InputState* input;
	GameTime time;
};

typedef size_t AssetID;
