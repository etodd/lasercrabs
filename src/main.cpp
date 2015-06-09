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

GLFWwindow* window;

#include "render/view.h"
#include "load.h"
#include "data/array.h"
#include "data/entity.h"
#include "data/components.h"
#include "data/mesh.h"
#include "exec.h"
#include "physics.h"
#include "render/render.h"
#include "asset.h"

#include "game/entities.h"
#include "game/player.h"

void resize(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

void update_loop(Loader* loader, RenderSync::Swapper* swapper)
{
	ExecSystemDynamic<Update> update;
	ExecSystemDynamic<RenderParams*> draw;

	update.add(&Entities::all);
	update.add(&Physics::world);
	draw.add(&Entities::all.draw);

	StaticGeom* a = Entities::all.create<StaticGeom>(loader, Asset::Model::city3);

	Player* player = Entities::all.create<Player>();

	RenderParams render_params;

	SyncData* sync = swapper->get();

	Update u;

	while (!sync->quit)
	{
		u.input = &sync->input;
		u.time = sync->time;
		update.exec(u);

		render_params.sync = sync;

		render_params.projection = player->projection;
		render_params.view = player->view;

		sync->op(RenderOp_Clear);
		
		GLbitfield clear_flags = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
		sync->write<GLbitfield>(&clear_flags);

		draw.exec(&render_params);

		sync = swapper->swap<SwapType_Write>();
		sync->queue.length = 0;
	}
}

int main()
{
	// Initialise GLFW
	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		return -1;
	}

	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	window = glfwCreateWindow(1024, 768, "MK-ZEBRA", NULL, NULL);
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

	Loader loader(&update_swapper);

	std::thread update_thread(update_loop, &loader, &update_swapper);

	SyncData* sync = render_swapper.get();

	GLData gl_data;

	double lastTime = glfwGetTime();
	while (true)
	{
		render(sync, &gl_data);

		// Swap buffers
		glfwSwapBuffers(window);

		glfwPollEvents();

		bool quit = sync->quit = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS || glfwWindowShouldClose(window);

		_GLFWwindow* _window = (_GLFWwindow*)window;
		memcpy(sync->input.keys, _window->keys, sizeof(sync->input.keys));

		glfwGetCursorPos(window, &sync->input.cursor_x, &sync->input.cursor_y);
		glfwSetCursorPos(window, 1024/2, 768/2);
		sync->input.mouse = glfwGetMouseButton(window, 0);
		sync->time.total = glfwGetTime();
		sync->time.delta = sync->time.total - lastTime;
		lastTime = sync->time.total;
		glfwGetFramebufferSize(window, &sync->input.width, &sync->input.height);

		sync = render_swapper.swap<SwapType_Read>();

		if (quit)
			break;
	}

	update_thread.join();

	glfwTerminate();

	return 0;
}
