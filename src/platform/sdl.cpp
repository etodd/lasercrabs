#define _AMD64_

#include <glew/include/GL/glew.h>
#include <sdl/include/SDL.h>
//#undef main

#include "render/glvm.h"
#include "load.h"

#include <thread>
#include "physics.h"
#include "loop.h"
#include "settings.h"
#if _WIN32
#include <Windows.h>
#endif
#include <time.h>
#include "lodepng/lodepng.h"

namespace VI
{

	namespace platform
	{

		u64 timestamp()
		{
			time_t t;
			::time(&t);
			return (u64)t;
		}

		double time()
		{
			return (SDL_GetTicks() / 1000.0);
		}

		void sleep(float time)
		{
			SDL_Delay(time * 1000.0f);
		}

	}

	SDL_Window* window = 0;
	SDL_GameController* controllers[MAX_GAMEPADS] = {};
	Gamepad::Type controller_types[MAX_GAMEPADS] = {};
	SDL_Haptic* haptics[MAX_GAMEPADS] = {};

	void refresh_controllers()
	{
		for (s32 i = 0; i < MAX_GAMEPADS; i++)
		{
			if (haptics[i])
			{
				SDL_HapticClose(haptics[i]);
				haptics[i] = nullptr;
			}

			if (controllers[i])
			{
				SDL_GameControllerClose(controllers[i]);
				controllers[i] = nullptr;
			}
			controller_types[i] = Gamepad::Type::None;
		}

		for (s32 i = 0; i < SDL_NumJoysticks(); i++)
		{
			if (SDL_IsGameController(i))
			{
				controllers[i] = SDL_GameControllerOpen(i);
				const char* name = SDL_GameControllerName(controllers[i]);
				if (strstr(name, "DualShock"))
					controller_types[i] = Gamepad::Type::Playstation;
				else
					controller_types[i] = Gamepad::Type::Xbox;

				SDL_Joystick* joystick = SDL_GameControllerGetJoystick(controllers[i]);

				if (SDL_JoystickIsHaptic(joystick))
				{
					haptics[i] = SDL_HapticOpenFromJoystick(joystick);
					if (SDL_HapticRumbleInit(haptics[i])) // failed
					{
						SDL_HapticClose(haptics[i]);
						haptics[i] = nullptr;
					}
				}
			}
		}
	}

	s32 vsync_set(b8 vsync)
	{
		if (SDL_GL_SetSwapInterval(vsync ? 1 : 0) != 0)
		{
			fprintf(stderr, "Failed to set OpenGL swap interval: %s\n", SDL_GetError());
			return -1;
		}
	}

	s32 proc()
	{
		{
			const char* itch_api_key = getenv("ITCHIO_API_KEY");
			if (itch_api_key)
			{
				Game::auth_type = Net::Master::AuthType::Itch;
				strncpy(Game::auth_key, itch_api_key, MAX_AUTH_KEY);
			}
		}

#if _WIN32
		SetProcessDPIAware();
#endif

#if defined(__APPLE__)
		SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1");
#endif

		// initialize SDL
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

		Loader::data_directory = SDL_GetPrefPath("HelveticaScenario", "Deceiver");

		SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt");

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

		{
			Array<DisplayMode> modes;
			for (s32 i = SDL_GetNumDisplayModes(0) - 1; i >= 0; i--)
			{
				SDL_DisplayMode mode;
				SDL_GetDisplayMode(0, i, &mode);

				// check for duplicates; could have multiple display modes with same resolution but different refresh rate
				b8 unique = true;
				for (s32 j = 0; j < modes.length; j++)
				{
					const DisplayMode& other = modes[j];
					if (other.width == mode.w && other.height == mode.h)
					{
						unique = false;
						break;
					}
				}
				
				if (unique)
					modes.add({ mode.w, mode.h });
			}
			Loader::settings_load(modes);
		}

		if (SDL_SetRelativeMouseMode(SDL_TRUE) != 0)
		{
			fprintf(stderr, "Failed to set relative mouse mode: %s\n", SDL_GetError());
			return -1;
		}

		window = SDL_CreateWindow
		(
			"Deceiver",
			0,
			0,
			Settings::display().width, Settings::display().height,
			SDL_WINDOW_OPENGL
			| SDL_WINDOW_SHOWN
			| SDL_WINDOW_INPUT_GRABBED
			| SDL_WINDOW_INPUT_FOCUS
			| SDL_WINDOW_MOUSE_FOCUS
			| SDL_WINDOW_MOUSE_CAPTURE
			| SDL_WINDOW_ALLOW_HIGHDPI
			| (Settings::fullscreen ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_BORDERLESS)
		);

#if defined(__APPLE__)
		SDL_SetWindowGrab(window, SDL_TRUE);
#endif

		// open a window and create its OpenGL context
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

		if (vsync_set(Settings::vsync))
			return -1;

		{
			glewExperimental = true; // needed for core profile

			GLenum glew_result = glewInit();
			if (glew_result != GLEW_OK)
			{
				fprintf(stderr, "Failed to initialize GLEW: %s\n", glewGetErrorString(glew_result));
				return -1;
			}
		}

		DisplayMode resolution_current = Settings::display();
		b8 resolution_current_fullscreen = Settings::fullscreen;
		b8 resolution_current_vsync = Settings::vsync;

		glGetError(); // clear initial error caused by GLEW

		render_init();

		// launch threads

		Sync<LoopSync> render_sync;

		LoopSwapper swapper_render_update = render_sync.swapper(0);
		LoopSwapper swapper_render = render_sync.swapper(1);

		Sync<PhysicsSync, 1> physics_sync;

		PhysicsSwapper swapper_physics = physics_sync.swapper();
		PhysicsSwapper swapper_physics_update = physics_sync.swapper();

		std::thread thread_physics(Physics::loop, &swapper_physics);

		std::thread thread_update(Loop::loop, &swapper_render_update, &swapper_physics_update);

		std::thread thread_ai(AI::loop);

		LoopSync* sync = swapper_render.get();

		r64 last_time = SDL_GetTicks() / 1000.0;

		b8 has_focus = true;

		SDL_PumpEvents();

		const u8* sdl_keys = SDL_GetKeyboardState(0);

		refresh_controllers();

		while (true)
		{
			{
				// display mode
				const DisplayMode& desired = sync->display_mode;
				if (desired.width != 0) // we're getting actual valid data from the update thread
				{
					if (sync->fullscreen != resolution_current_fullscreen)
					{
						if (SDL_SetWindowFullscreen(window, sync->fullscreen ? SDL_WINDOW_FULLSCREEN : 0))
						{
							fprintf(stderr, "Failed to set fullscreen mode: %s\n", SDL_GetError());
							return -1;
						}
						resolution_current_fullscreen = sync->fullscreen;
					}

					if (sync->vsync != resolution_current_vsync)
					{
						if (vsync_set(sync->vsync))
							return -1;
						resolution_current_vsync = sync->vsync;
					}

					if (desired.width != resolution_current.width || desired.height != resolution_current.height)
					{
						SDL_DisplayMode new_mode = {};
						for (s32 i = 0; i < SDL_GetNumDisplayModes(0); i++)
						{
							SDL_DisplayMode m;
							SDL_GetDisplayMode(0, i, &m);
							if (m.w == desired.width && m.h == desired.height && m.refresh_rate > new_mode.refresh_rate)
								new_mode = m;
						}

						vi_assert(new_mode.w == desired.width && new_mode.h == desired.height);
						if (sync->fullscreen)
							SDL_SetWindowDisplayMode(window, &new_mode);
						else
							SDL_SetWindowSize(window, new_mode.w, new_mode.h);

						resolution_current = desired;
					}
				}
			}

			render(sync);

			// swap buffers
			SDL_GL_SwapWindow(window);

			SDL_PumpEvents();

#if DEBUG
			// screenshot
			if (sync->input.keys.get(s32(KeyCode::F11)) && !sdl_keys[s32(KeyCode::F11)])
			{
				s32 w = Settings::display().width;
				s32 h = Settings::display().height;
				std::vector<unsigned char> data;
				data.resize(w * h * 4);

				{
					glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
					glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, &data[0]);
					GLenum error;
					if ((error = glGetError()) != GL_NO_ERROR)
					{
						vi_debug("GL error: %u", error);
						vi_debug_break();
					}
				}

				// flip image
				{
					vi_assert(h % 2 == 0);
					s32 y2 = h - 1;
					for (s32 y = 0; y < h / 2; y++)
					{
						for (s32 x = 0; x < w; x++)
						{
							u32* i = (u32*)(&data[(y * w + x) * 4]);
							u32* j = (u32*)(&data[(y2 * w + x) * 4]);
							u32 tmp = *i;
							*i = *j;
							*j = tmp;
						}
						y2--;
					}
				}

				lodepng::encode("screen.png", data, w, h);
			}
#endif

			sync->input.keys.clear();
			for (s32 i = 0; i < s32(KeyCode::count); i++)
			{
				if (sdl_keys[i])
					sync->input.keys.set(i, true);
			}

			SDL_Event sdl_event;
			while (SDL_PollEvent(&sdl_event))
			{
				if (sdl_event.type == SDL_QUIT)
					sync->quit = true;
				else if (sdl_event.type == SDL_MOUSEWHEEL)
				{ 
					b8 up = sdl_event.wheel.y > 0;
					if (sdl_event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
						up = !up;
					if (up)
						sync->input.keys.set(s32(KeyCode::MouseWheelUp), true);
					else
						sync->input.keys.set(s32(KeyCode::MouseWheelDown), true);
				} 
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

			s32 mouse_buttons = SDL_GetRelativeMouseState(&sync->input.cursor_x, &sync->input.cursor_y);

			sync->input.keys.set(s32(KeyCode::MouseLeft), mouse_buttons & (1 << 0));
			sync->input.keys.set(s32(KeyCode::MouseMiddle), mouse_buttons & (1 << 1));
			sync->input.keys.set(s32(KeyCode::MouseRight), mouse_buttons & (1 << 2));

			s32 active_gamepads = 0;
			for (s32 i = 0; i < MAX_GAMEPADS; i++)
			{
				SDL_GameController* controller = controllers[i];
				Gamepad* gamepad = &sync->input.gamepads[i];
				gamepad->type = controller_types[i];
				gamepad->btns = 0;
				if (gamepad->type == Gamepad::Type::None)
				{
					gamepad->left_x = 0.0f;
					gamepad->left_y = 0.0f;
					gamepad->right_x = 0.0f;
					gamepad->right_y = 0.0f;
					gamepad->left_trigger = 0.0f;
					gamepad->right_trigger = 0.0f;
					gamepad->rumble = 0.0f;
				}
				else
				{
					gamepad->left_x = r32(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX)) / 32767.0f;
					gamepad->left_y = r32(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY)) / 32767.0f;
					gamepad->right_x = r32(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX)) / 32767.0f;
					gamepad->right_y = r32(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY)) / 32767.0f;
					gamepad->left_trigger = r32(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT)) / 32767.0f;
					gamepad->right_trigger = r32(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT)) / 32767.0f;
					if (gamepad->left_trigger > 0.1f)
						gamepad->btns |= 1 << s32(Gamepad::Btn::LeftTrigger);
					if (gamepad->right_trigger > 0.1f)
						gamepad->btns |= 1 << s32(Gamepad::Btn::RightTrigger);
					if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER))
						gamepad->btns |= 1 << s32(Gamepad::Btn::LeftShoulder);
					if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))
						gamepad->btns |= 1 << s32(Gamepad::Btn::RightShoulder);
					if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSTICK))
						gamepad->btns |= 1 << s32(Gamepad::Btn::LeftClick);
					if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_RIGHTSTICK))
						gamepad->btns |= 1 << s32(Gamepad::Btn::RightClick);
					if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A))
						gamepad->btns |= 1 << s32(Gamepad::Btn::A);
					if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B))
						gamepad->btns |= 1 << s32(Gamepad::Btn::B);
					if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_X))
						gamepad->btns |= 1 << s32(Gamepad::Btn::X);
					if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_Y))
						gamepad->btns |= 1 << s32(Gamepad::Btn::Y);
					if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_BACK))
						gamepad->btns |= 1 << s32(Gamepad::Btn::Back);
					if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_START))
						gamepad->btns |= 1 << s32(Gamepad::Btn::Start);
					if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP))
						gamepad->btns |= 1 << s32(Gamepad::Btn::DUp);
					if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN))
						gamepad->btns |= 1 << s32(Gamepad::Btn::DDown);
					if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT))
						gamepad->btns |= 1 << s32(Gamepad::Btn::DLeft);
					if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
						gamepad->btns |= 1 << s32(Gamepad::Btn::DRight);
					if (gamepad->rumble > 0.0f)
						SDL_HapticRumblePlay(haptics[i], gamepad->rumble, 33);
					gamepad->rumble = 0.0f;
					active_gamepads++;
				}
			}

			r64 time = (SDL_GetTicks() / 1000.0);
			sync->time.total = r32(time);
			sync->time.delta = vi_min(r32(time - last_time), 0.25f);
			last_time = time;

			b8 quit = sync->quit;

			sync = swapper_render.swap<SwapType_Read>();

			if (quit || sync->quit)
				break;
		}

		AI::quit();

		thread_update.join();
		thread_physics.join();
		thread_ai.join();

		SDL_GL_DeleteContext(context);
		SDL_DestroyWindow(window);

		// SDL sucks
		for (s32 i = 0; i < MAX_GAMEPADS; i++)
		{
			if (haptics[i])
				SDL_HapticClose(haptics[i]);
		}

		SDL_Quit();

		return 0;
	}

}

int main(int argc, char** argv)
{
	return VI::proc();
}
