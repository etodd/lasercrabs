#include "main.h"

#include "types.h"
#include "vi_assert.h"

#include <thread>

#include "load.h"
#include "render/render.h"
#include "game/game.h"

namespace VI
{

SDL_Window* Main::window = 0;
SDL_GameController* Main::controller = 0;

void Main::resize(SDL_Window* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

void Main::get_controller()
{
	if (controller)
		SDL_GameControllerClose(controller);
	controller = 0;
	for (int i = 0; i < SDL_NumJoysticks(); ++i)
	{
		if (SDL_IsGameController(i))
		{
			controller = SDL_GameControllerOpen(i);
			break;
		}
	}
}

int Main::proc()
{
	// Initialise SDL
	if (SDL_Init(
		SDL_INIT_VIDEO
		| SDL_INIT_EVENTS
		| SDL_INIT_GAMECONTROLLER
		| SDL_INIT_HAPTIC
		| SDL_INIT_TIMER
		| SDL_INIT_JOYSTICK
		) < 0)
	{
		fprintf(stderr, "Failed to initialize SDL\n");
		return -1;
	}

	SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt");

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

	window = SDL_CreateWindow
	(
		"MK-ZEBRA",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		1280, 720,
		SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
	);

	// Open a window and create its OpenGL context
	if (!window)
	{
		fprintf(stderr, "Failed to open SDL window. Most likely your GPU is out of date!\n");
		SDL_Quit();
		return -1;
	}

	SDL_GLContext context = SDL_GL_CreateContext(window);

	SDL_SetRelativeMouseMode(SDL_TRUE);

	// Initialize GLEW
	glewExperimental = true; // Needed for core profile
	if (glewInit() != GLEW_OK)
	{
		fprintf(stderr, "Failed to initialize GLEW\n");
		return -1;
	}

	// Ensure we can capture the escape key being pressed below

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

	float lastTime = (float)(SDL_GetTicks() / 1000.0);

	bool last_keys[KEYCODE_COUNT];
	memset(last_keys, 0, sizeof(last_keys));
	unsigned int last_mouse_buttons = 0;

	bool has_focus = true;

	SDL_PumpEvents();

	const Uint8* sdl_keys = SDL_GetKeyboardState(0);

	get_controller();

	while (true)
	{
		render(sync, &gl_data);

		// Swap buffers
		SDL_GL_SwapWindow(window);

		SDL_PumpEvents();

		SDL_Event sdl_event;
		while (SDL_PollEvent(&sdl_event))
		{
			if (sdl_event.type == SDL_QUIT)
				sync->quit = true;
			else if (sdl_event.type == SDL_JOYDEVICEADDED
				|| sdl_event.type == SDL_JOYDEVICEREMOVED)
				get_controller();
			else if (sdl_event.type == SDL_WINDOWEVENT)
			{
				if (sdl_event.window.event == SDL_WINDOWEVENT_RESIZED)
					resize(window, sync->input.width, sync->input.height);
				else if (sdl_event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
					has_focus = true;
				else if (sdl_event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
					has_focus = false;
			}
		}

		sync->focus = has_focus;

		memcpy(sync->input.last_keys, last_keys, sizeof(last_keys));
		memcpy(last_keys, sdl_keys, sizeof(last_keys));
		memcpy(sync->input.keys, sdl_keys, sizeof(sync->input.keys));
		sync->input.last_mouse_buttons = last_mouse_buttons;
		sync->input.mouse_buttons = last_mouse_buttons = SDL_GetRelativeMouseState(&sync->input.cursor_x, &sync->input.cursor_y);

		sync->input.joystick = controller != 0;
		if (sync->input.joystick)
		{
			sync->input.joystick_left_x = (float)SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX) / 32767.0f;
			sync->input.joystick_left_y = (float)SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY) / 32767.0f;
			sync->input.joystick_right_x = (float)SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX) / 32767.0f;
			sync->input.joystick_right_y = (float)SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY) / 32767.0f;
			sync->input.joystick_left_trigger = (float)SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 32767.0f;
			sync->input.joystick_right_trigger = (float)SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 32767.0f;
		}

		bool quit = sync->quit = sync->input.keys[KEYCODE_ESCAPE];

		SDL_GetWindowSize(window, &sync->input.width, &sync->input.height);

		sync->time.total = (float)(SDL_GetTicks() / 1000.0);
		sync->time.delta = sync->time.total - lastTime;
		lastTime = sync->time.total;

		sync = render_swapper.swap<SwapType_Read>();

		if (sync->input.set_width > 0)
		{
			SDL_SetWindowSize(window, sync->input.set_width, sync->input.set_height);
			sync->input.set_width = 0;
			sync->input.set_height = 0;
		}

		if (quit)
			break;
	}

	update_thread.join();

	SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}

}

int main(int argc, char* argv[])
{
	return VI::Main::proc();
}
