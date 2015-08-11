#include "types.h"
#include "vi_assert.h"

#include <thread>

#include <GL/glew.h>

#include "load.h"
#include "render/render.h"
#include "game/game.h"
#include "main.h"
#include <GLFW/../../src/internal.h>

namespace VI
{

GLFWwindow* Main::window = 0;

void Main::resize(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

int Main::proc()
{
	// Initialise GLFW
	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		return -1;
	}

	glfwWindowHint(GLFW_SAMPLES, 8);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	bool fullscreen = false;
	if (fullscreen)
	{
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);
		glfwWindowHint(GLFW_DECORATED, GL_FALSE);
		glfwWindowHint(GLFW_RED_BITS, mode->redBits);
		glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
		glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
		glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
		window = glfwCreateWindow(mode->width, mode->height, "MK-ZEBRA", NULL, NULL);
	}
	else
	{
		window = glfwCreateWindow(1280, 720, "MK-ZEBRA", NULL, NULL);
	}

	// Open a window and create its OpenGL context
	if (!window)
	{
		fprintf(stderr, "Failed to open GLFW window. Most likely your GPU is out of date!\n");
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, Main::resize);

	// Initialize GLEW
	glewExperimental = true; // Needed for core profile
	if (glewInit() != GLEW_OK)
	{
		fprintf(stderr, "Failed to initialize GLEW\n");
		return -1;
	}

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_FALSE);
#if __APPLE__
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
#else
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
#endif

	glClearColor(1.0f, 1.0f, 1.0f, 0.0f);

	// Enable depth test
	glEnable(GL_DEPTH_TEST);
	// Accept fragment if it closer to the camera than the former one
	glDepthFunc(GL_LESS); 

	// Cull triangles which normal is not towards the camera
	glEnable(GL_CULL_FACE);

	RenderSync render_sync;

	RenderSync::Swapper update_swapper = render_sync.swapper(0);
	RenderSync::Swapper render_swapper = render_sync.swapper(1);

	std::thread update_thread(Game::loop, &update_swapper);

	SyncData* sync = render_swapper.get();

	GLData gl_data;
	render_init(&gl_data);

	float lastTime = (float)glfwGetTime();

	char last_keys[GLFW_KEY_LAST + 1];
	char last_mouse_buttons[8];

	while (true)
	{
		render(sync, &gl_data);

		// Swap buffers
		glfwSwapBuffers(window);

		glfwPollEvents();

		bool focus = sync->focus = glfwGetWindowAttrib(window, GLFW_FOCUSED);

		memcpy(sync->input.last_keys, last_keys, sizeof(last_keys));
		memcpy(sync->input.last_mouse_buttons, last_mouse_buttons, sizeof(last_mouse_buttons));
		_GLFWwindow* _window = (_GLFWwindow*)window;
		memcpy(last_keys, _window->keys, sizeof(last_keys));
		memcpy(last_mouse_buttons, _window->mouseButtons, sizeof(last_mouse_buttons));
		memcpy(sync->input.keys, _window->keys, sizeof(sync->input.keys));
		memcpy(sync->input.mouse_buttons, _window->mouseButtons, sizeof(sync->input.mouse_buttons));

		glfwGetCursorPos(window, &sync->input.cursor_x, &sync->input.cursor_y);
		if (focus)
			glfwSetCursorPos(window, sync->input.width / 2, sync->input.height / 2);

		bool quit = sync->quit = sync->input.keys[GLFW_KEY_ESCAPE] == GLFW_PRESS || glfwWindowShouldClose(window);

		glfwGetFramebufferSize(window, &sync->input.width, &sync->input.height);

		sync->time.total = (float)glfwGetTime();
		sync->time.delta = sync->time.total - lastTime;
		lastTime = sync->time.total;

		sync = render_swapper.swap<SwapType_Read>();

		if (sync->input.set_width > 0)
		{
			glfwSetWindowSize(Main::window, sync->input.set_width, sync->input.set_height);
			sync->input.set_width = 0;
			sync->input.set_height = 0;
		}

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
	return VI::Main::proc();
}
