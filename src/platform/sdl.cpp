#define _AMD64_

#include <glew/include/GL/glew.h>
#include <sdl/include/SDL.h>
//#undef main

#include "render/glvm.h"
#include "load.h"

#include <thread>
#include "physics.h"
#include "loop.h"
#include <Windows.h>

namespace VI
{
	SDL_Window* window = 0;
	SDL_GameController* controllers[MAX_GAMEPADS] = {};

	void refresh_controllers()
	{
		for (s32 i = 0; i < MAX_GAMEPADS; i++)
		{
			if (controllers[i])
			{
				SDL_GameControllerClose(controllers[i]);
				controllers[i] = nullptr;
			}
		}

		for (s32 i = 0; i < SDL_NumJoysticks(); i++)
		{
			if (SDL_IsGameController(i))
				controllers[i] = SDL_GameControllerOpen(i);
		}
	}

	s32 proc()
	{
#if _WIN32
		SetProcessDPIAware();
#endif
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

		Loader::data_directory = SDL_GetPrefPath("PoorMan", "Yearning");

		SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt");

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
#if defined(__APPLE__)
		SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1");
#endif

		Settings& settings = Loader::settings();

		window = SDL_CreateWindow
		(
			"The Yearning",
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED,
			settings.width, settings.height,
			SDL_WINDOW_OPENGL
			| SDL_WINDOW_SHOWN
			| SDL_WINDOW_BORDERLESS
			| SDL_WINDOW_INPUT_GRABBED
			| SDL_WINDOW_INPUT_FOCUS
			| SDL_WINDOW_MOUSE_FOCUS
			| SDL_WINDOW_MOUSE_CAPTURE
			| SDL_WINDOW_ALLOW_HIGHDPI
			| (settings.fullscreen ? SDL_WINDOW_FULLSCREEN : 0)
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

		if (SDL_GL_SetSwapInterval(settings.vsync ? 1 : 0) != 0)
		{
			fprintf(stderr, "Failed to set OpenGL swap interval: %s\n", SDL_GetError());
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

		Sync<LoopSync> render_sync;

		LoopSwapper update_swapper = render_sync.swapper(0);
		LoopSwapper render_swapper = render_sync.swapper(1);

		Sync<PhysicsSync, 1> physics_sync;

		PhysicsSwapper physics_swapper = physics_sync.swapper();
		PhysicsSwapper physics_update_swapper = physics_sync.swapper();

		std::thread physics_thread(Physics::loop, &physics_swapper);

		std::thread update_thread(Loop::loop, &update_swapper, &physics_update_swapper);

		std::thread ai_thread(AI::loop);

		LoopSync* sync = render_swapper.get();

		r64 last_time = SDL_GetTicks() / 1000.0;

		b8 has_focus = true;

		SDL_PumpEvents();

		const u8* sdl_keys = SDL_GetKeyboardState(0);

		refresh_controllers();

		while (true)
		{
			render(sync);

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
					refresh_controllers();
				else if (sdl_event.type == SDL_WINDOWEVENT)
				{
					if (sdl_event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
						has_focus = true;
					else if (sdl_event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
						has_focus = false;
				}
			}

			sync->input.focus = has_focus;

			memcpy(sync->input.keys, sdl_keys, sizeof(sync->input.keys));

			u32 mouse_buttons = SDL_GetRelativeMouseState(&sync->input.cursor_x, &sync->input.cursor_y);

			sync->input.keys[(s32)KeyCode::MouseLeft] = mouse_buttons & (1 << 0);
			sync->input.keys[(s32)KeyCode::MouseMiddle] = mouse_buttons & (1 << 1);
			sync->input.keys[(s32)KeyCode::MouseRight] = mouse_buttons & (1 << 2);

			s32 active_gamepads = 0;
			for (s32 i = 0; i < MAX_GAMEPADS; i++)
			{
				SDL_GameController* controller = controllers[i];
				Gamepad* gamepad = &sync->input.gamepads[i];
				gamepad->active = controller != 0;
				gamepad->btns = 0;
				if (gamepad->active)
				{
					gamepad->left_x = (r32)SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX) / 32767.0f;
					gamepad->left_y = (r32)SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY) / 32767.0f;
					gamepad->right_x = (r32)SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX) / 32767.0f;
					gamepad->right_y = (r32)SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY) / 32767.0f;
					gamepad->left_trigger = (r32)SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 32767.0f;
					gamepad->right_trigger = (r32)SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 32767.0f;
					if (gamepad->left_trigger > 0.5f)
						gamepad->btns |= Gamepad::Btn::LeftTrigger;
					if (gamepad->right_trigger > 0.5f)
						gamepad->btns |= Gamepad::Btn::RightTrigger;
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
					if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_X))
						gamepad->btns |= Gamepad::Btn::X;
					if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_Y))
						gamepad->btns |= Gamepad::Btn::Y;
					if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_START))
						gamepad->btns |= Gamepad::Btn::Start;
					if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP))
						gamepad->btns |= Gamepad::Btn::DUp;
					if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN))
						gamepad->btns |= Gamepad::Btn::DDown;
					if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT))
						gamepad->btns |= Gamepad::Btn::DLeft;
					if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
						gamepad->btns |= Gamepad::Btn::DRight;
					active_gamepads++;
				}
				else
				{
					gamepad->left_x = 0.0f;
					gamepad->left_y = 0.0f;
					gamepad->right_x = 0.0f;
					gamepad->right_y = 0.0f;
					gamepad->left_trigger = 0.0f;
					gamepad->right_trigger = 0.0f;
				}
			}

			SDL_GetWindowSize(window, &sync->input.width, &sync->input.height);

			r64 time = (SDL_GetTicks() / 1000.0);
			sync->time.total = (r32)time;
			sync->time.delta = vi_min((r32)(time - last_time), 0.25f);
			last_time = time;

			b8 quit = sync->quit;

			sync = render_swapper.swap<SwapType_Read>();

			if (quit || sync->quit)
				break;
		}

		AI::quit();

		update_thread.join();
		physics_thread.join();
		ai_thread.join();

		SDL_GL_DeleteContext(context);
		SDL_DestroyWindow(window);
		SDL_Quit();

		return 0;
	}

}

int main(int argc, char** argv)
{
	return VI::proc();
}
