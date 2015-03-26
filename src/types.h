#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

struct GameTime
{
	float total;
	float delta;
};

struct InputState
{
	char keys[348 + 1]; // GLFW_KEY_LAST + 1
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

typedef glm::vec2 Vec2;
typedef glm::vec3 Vec3;
typedef glm::quat Quat;
typedef glm::mat4x4 Mat4;
