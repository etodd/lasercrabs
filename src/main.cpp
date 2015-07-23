#include "types.h"
#include "vi_assert.h"

#include <thread>

#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "glfw_config.h"
#include <GLFW/../../src/internal.h>

#include "load.h"
#include "render/render.h"
#include "game/game.h"

namespace VI
{

GLFWwindow* window;

void resize(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

int proc()
{
	// Initialise GLFW
	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		return -1;
	}

	glfwWindowHint(GLFW_SAMPLES, 1);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	bool fullscreen = false;
	if (fullscreen)
	{
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);
		glfwWindowHint(GLFW_RED_BITS, mode->redBits);
		glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
		glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
		glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
		window = glfwCreateWindow(mode->width, mode->height, "MK-ZEBRA", monitor, NULL);
	}
	else
	{
		window = glfwCreateWindow(1024, 768, "MK-ZEBRA", NULL, NULL);
	}

	// Open a window and create its OpenGL context
	if (!window)
	{
		fprintf(stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Sorry.\n");
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, resize);

	// Initialize GLEW
	glewExperimental = true; // Needed for core profile
	if (glewInit() != GLEW_OK)
	{
		fprintf(stderr, "Failed to initialize GLEW\n");
		return -1;
	}

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// Dark blue background
	glClearColor(0.0f, 0.0f, 0.4f, 0.0f);

	// Enable depth test
	glEnable(GL_DEPTH_TEST);
	// Accept fragment if it closer to the camera than the former one
	glDepthFunc(GL_LESS); 

	// Cull triangles which normal is not towards the camera
	glEnable(GL_CULL_FACE);

	RenderSync render_sync;

	RenderSync::Swapper update_swapper = render_sync.swapper(0);
	RenderSync::Swapper render_swapper = render_sync.swapper(1);

	std::thread update_thread(game_loop, &update_swapper);

	SyncData* sync = render_swapper.get();

	GLData gl_data;

	float lastTime = (float)glfwGetTime();
	while (true)
	{
		render(sync, &gl_data);

		// Swap buffers
		glfwSwapBuffers(window);

		glfwPollEvents();

		bool quit = sync->quit = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS || glfwWindowShouldClose(window);

#if DEBUG
		// Convenience function for recording gifs
		if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS)
			glfwSetWindowSize(window, 500, 281);
#endif

		_GLFWwindow* _window = (_GLFWwindow*)window;
		memcpy(sync->input.keys, _window->keys, sizeof(sync->input.keys));

		glfwGetFramebufferSize(window, &sync->input.width, &sync->input.height);

		glfwGetCursorPos(window, &sync->input.cursor_x, &sync->input.cursor_y);
		glfwSetCursorPos(window, sync->input.width / 2, sync->input.height / 2);
		memcpy(sync->input.mouse_buttons, _window->mouseButtons, sizeof(bool) * 8);
		sync->time.total = (float)glfwGetTime();
		sync->time.delta = sync->time.total - lastTime;
		lastTime = sync->time.total;

		sync = render_swapper.swap<SwapType_Read>();

		if (quit)
			break;
	}

	update_thread.join();

	glfwTerminate();

	return 0;
}

}

#if defined WIN32 && !_CONSOLE
int CALLBACK WinMain(
	__in  HINSTANCE hInstance,
	__in  HINSTANCE hPrevInstance,
	__in  LPSTR lpCmdLine,
	__in  int nCmdShow)
#else
int main()
#endif
{
	return VI::proc();
}