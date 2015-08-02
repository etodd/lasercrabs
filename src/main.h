#pragma once

#include "types.h"
#include <GL/glew.h>
#pragma once

#include <GLFW/glfw3.h>
#include "glfw_config.h"

namespace VI
{

struct Main
{
	static GLFWwindow* window;
	static void resize(GLFWwindow*, int, int);
	static int proc();
};

}
