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
	char last_keys[348 + 1];
	double cursor_x;
	double cursor_y;
	char mouse_buttons[8];
	char last_mouse_buttons[8];
	int width;
	int height;
	int set_width;
	int set_height;
};

struct Update
{
	InputState* input;
	GameTime time;
};

typedef int AssetID;
typedef int AssetRef;

}
