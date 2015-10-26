#define _AMD64_

#include "gl.h"
#include "load.h"

#include <SDL.h>
#undef main // Fix SDL nonsense

#include <thread>
#include "physics.h"
#include "loop.h"
#include "game/game.h"

namespace VI
{

SDL_Window* window = 0;
SDL_GameController* controllers[MAX_GAMEPADS] = {};

void refresh_controllers()
{
	for (int i = 0; i < MAX_GAMEPADS; i++)
	{
		if (controllers[i])
		{
			SDL_GameControllerClose(controllers[i]);
			controllers[i] = nullptr;
		}
	}

	for (int i = 0; i < SDL_NumJoysticks(); i++)
	{
		if (SDL_IsGameController(i))
			controllers[i] = SDL_GameControllerOpen(i);
	}
}

int proc()
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
		
		fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
		return -1;
	}

	SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt");

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
#if defined(__APPLE__)
	SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1");
#endif

	window = SDL_CreateWindow
	(
		"MK-ZEBRA",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		1280, 720,
		//506, 253,
		SDL_WINDOW_OPENGL
		| SDL_WINDOW_SHOWN
		| SDL_WINDOW_BORDERLESS
		| SDL_WINDOW_INPUT_GRABBED
		| SDL_WINDOW_INPUT_FOCUS
		| SDL_WINDOW_MOUSE_FOCUS
		| SDL_WINDOW_MOUSE_CAPTURE
	);

#if defined(__APPLE__)
	SDL_SetWindowGrab(window, SDL_TRUE);
#endif

	// Open a window and create its OpenGL context
	if (!window)
	{
		fprintf(stderr, "Failed to open SDL window. Most likely your GPU is out of date! %s\n", SDL_GetError());
		SDL_Quit();
		return -1;
	}

	SDL_GLContext context = SDL_GL_CreateContext(window);
	if (!context)
	{
		fprintf(stderr, "Failed to create GL context: %s\n", SDL_GetError());
		return -1;
	}

	if (SDL_SetRelativeMouseMode(SDL_TRUE) != 0)
	{
		fprintf(stderr, "Failed to set relative mouse mode: %s\n", SDL_GetError());
		return -1;
	}

	{
		glewExperimental = true; // Needed for core profile

		GLenum glew_result = glewInit();
		if (glew_result != GLEW_OK)
		{
			fprintf(stderr, "Failed to initialize GLEW: %s\n", glewGetErrorString(glew_result));
			return -1;
		}
	}

	glGetError(); // Clear initial error caused by GLEW

	render_init();

	// Launch threads

	Sync<RenderSync> render_sync;

	RenderSwapper update_swapper = render_sync.swapper(0);
	RenderSwapper render_swapper = render_sync.swapper(1);

	Sync<PhysicsSync, 1> physics_sync;

	PhysicsSwapper physics_swapper = physics_sync.swapper();
	PhysicsSwapper physics_update_swapper = physics_sync.swapper();

	std::thread physics_thread(Physics::loop, &physics_swapper);

	std::thread update_thread(Loop::loop, &update_swapper, &physics_update_swapper);

	RenderSync* sync = render_swapper.get();

	double last_time_real = SDL_GetTicks() / 1000.0;
	double last_time = last_time_real;

	bool last_keys[KEYCODE_COUNT];
	memset(last_keys, 0, sizeof(last_keys));
	unsigned int last_mouse_buttons = 0;
	unsigned int gamepad_btns[MAX_GAMEPADS] = {};

	bool has_focus = true;

	SDL_PumpEvents();

	const Uint8* sdl_keys = SDL_GetKeyboardState(0);

	refresh_controllers();

	while (true)
	{
		render(sync);

		// Swap buffers
		SDL_GL_SwapWindow(window);

		SDL_PumpEvents();

		bool quit = false;

		SDL_Event sdl_event;
		while (SDL_PollEvent(&sdl_event))
		{
			if (sdl_event.type == SDL_QUIT)
				quit = true;
			else if (sdl_event.type == SDL_JOYDEVICEADDED
				|| sdl_event.type == SDL_JOYDEVICEREMOVED)
				refresh_controllers();
			else if (sdl_event.type == SDL_WINDOWEVENT)
			{
				if (sdl_event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
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

		int active_gamepads = 0;
		for (int i = 0; i < MAX_GAMEPADS; i++)
		{
			SDL_GameController* controller = controllers[i];
			Gamepad* gamepad = &sync->input.gamepads[i];
			gamepad->active = controller != 0;
			gamepad->last_btns = gamepad_btns[i];
			gamepad->btns = 0;
			if (gamepad->active)
			{
				gamepad->left_x = (float)SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX) / 32767.0f;
				gamepad->left_y = (float)SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY) / 32767.0f;
				gamepad->right_x = (float)SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX) / 32767.0f;
				gamepad->right_y = (float)SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY) / 32767.0f;
				gamepad->left_trigger = (float)SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 32767.0f;
				gamepad->right_trigger = (float)SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 32767.0f;
				if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER))
					gamepad->btns |= Gamepad::Btn::LeftShoulder;
				if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))
					gamepad->btns |= Gamepad::Btn::RightShoulder;
				if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSTICK))
					gamepad->btns |= Gamepad::Btn::LeftClick;
				if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_RIGHTSTICK))
					gamepad->btns |= Gamepad::Btn::RightClick;
				if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A))
					gamepad->btns |= Gamepad::Btn::A;
				if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B))
					gamepad->btns |= Gamepad::Btn::B;
				if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_START))
					gamepad->btns |= Gamepad::Btn::Start;
				active_gamepads++;
			}
			gamepad_btns[i] = gamepad->btns;
		}

		sync->quit = quit;

		SDL_GetWindowSize(window, &sync->input.width, &sync->input.height);

		double real = (SDL_GetTicks() / 1000.0);
		sync->time.real = (float)real;
		sync->time.delta = (sync->time.real - last_time_real) * Game::time_scale;
		last_time_real = sync->time.real;
		last_time = sync->time.total = last_time + sync->time.delta;

		sync = render_swapper.swap<SwapType_Read>();

		if (quit)
			break;
	}

	update_thread.join();
	physics_thread.join();

	SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}

}

int main(int argc, char* argv[])
{
	return VI::proc();
}
