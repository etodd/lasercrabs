#pragma once

#include <cstdlib>

namespace VI
{

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
	bool mouse_buttons[8];
	int width;
	int height;
};

struct Update
{
	InputState* input;
	GameTime time;
};

typedef int AssetID;

}
