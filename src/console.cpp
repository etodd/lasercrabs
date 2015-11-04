#include "console.h"
#include "game/game.h"
#include "asset/font.h"
#include <cstdio>
#include "game/audio.h"
#include "game/wwise/Wwise_IDs.h"

namespace VI
{

Array<char> Console::command = Array<char>();
Array<char> Console::debug_buffer = Array<char>();
Array<Console::Log> Console::logs = Array<Console::Log>();
UIText Console::text = UIText();
UIText Console::fps_text = UIText();
UIText Console::debug_text = UIText();
UIText Console::log_text = UIText();
bool Console::fps_visible = false;
int Console::fps_count = 0;
float Console::fps_accumulator = 0;
bool Console::visible = false;
char Console::shift_map[127];
char Console::normal_map[127];
float Console::repeat_start_time = 0.0f;
float Console::repeat_last_time = 0.0f;

#define REPEAT_DELAY 0.2f
#define REPEAT_INTERVAL 0.03f
#define LOG_TIME 8.0f

void Console::init()
{
	Loader::font_permanent(Asset::Font::lowpoly);
	text.font = Asset::Font::lowpoly;
	text.size = 18.0f;
	fps_text.font = Asset::Font::lowpoly;
	fps_text.size = 18.0f;
	debug_text.font = Asset::Font::lowpoly;
	debug_text.size = 18.0f;
	log_text.font = Asset::Font::lowpoly;
	log_text.size = 18.0f;
	log_text.color = UI::default_color;
	log_text.anchor_x = UIText::Anchor::Max;
	log_text.anchor_y = UIText::Anchor::Max;

	debug_buffer.resize(1);

	command.resize(2);
	command[0] = '$';

	memset(normal_map, 0, sizeof(normal_map));
	memset(shift_map, 0, sizeof(shift_map));

	normal_map[KEYCODE_0] = '0';
	normal_map[KEYCODE_1] = '1';
	normal_map[KEYCODE_2] = '2';
	normal_map[KEYCODE_3] = '3';
	normal_map[KEYCODE_4] = '4';
	normal_map[KEYCODE_5] = '5';
	normal_map[KEYCODE_6] = '6';
	normal_map[KEYCODE_7] = '7';
	normal_map[KEYCODE_8] = '8';
	normal_map[KEYCODE_9] = '9';
	shift_map[KEYCODE_0] = ')';
	shift_map[KEYCODE_1] = '!';
	shift_map[KEYCODE_2] = '@';
	shift_map[KEYCODE_3] = '#';
	shift_map[KEYCODE_4] = '$';
	shift_map[KEYCODE_5] = '%';
	shift_map[KEYCODE_6] = '^';
	shift_map[KEYCODE_7] = '&';
	shift_map[KEYCODE_8] = '*';
	shift_map[KEYCODE_9] = '(';

	normal_map[KEYCODE_SPACE] = ' ';
	shift_map[KEYCODE_SPACE] = ' ';

	normal_map[KEYCODE_APOSTROPHE] = '\'';
	shift_map[KEYCODE_APOSTROPHE] = '"';

	normal_map[KEYCODE_MINUS] = '-';
	normal_map[KEYCODE_EQUALS] = '=';
	normal_map[KEYCODE_LEFTBRACKET] = '[';
	normal_map[KEYCODE_RIGHTBRACKET] = ']';
	normal_map[KEYCODE_COMMA] = ',';
	normal_map[KEYCODE_PERIOD] = '.';
	normal_map[KEYCODE_SLASH] = '/';
	normal_map[KEYCODE_GRAVE] = '`';
	normal_map[KEYCODE_SEMICOLON] = ';';
	normal_map[KEYCODE_BACKSLASH] = '\\';
	shift_map[KEYCODE_MINUS] = '_';
	shift_map[KEYCODE_EQUALS] = '+';
	shift_map[KEYCODE_LEFTBRACKET] = '{';
	shift_map[KEYCODE_RIGHTBRACKET] = '}';
	shift_map[KEYCODE_COMMA] = '<';
	shift_map[KEYCODE_PERIOD] = '>';
	shift_map[KEYCODE_SLASH] = '?';
	shift_map[KEYCODE_GRAVE] = '~';
	shift_map[KEYCODE_SEMICOLON] = ':';
	shift_map[KEYCODE_BACKSLASH] = '|';

	for (int i = 0; i < KEYCODE_Z - KEYCODE_A; i++)
	{
		normal_map[KEYCODE_A + i] = 'a' + i;
		shift_map[KEYCODE_A + i] = 'A' + i;
	}

	text.text(command.data);
}

void Console::update(const Update& u)
{
	if (u.input->keys[KEYCODE_GRAVE]
		&& !u.input->last_keys[KEYCODE_GRAVE])
		visible = !visible;

	if (fps_visible)
	{
		fps_count += 1;
		fps_accumulator += u.time.delta;
		if (fps_accumulator > 0.5f)
		{
			char fps_label[256];
			sprintf(fps_label, "%.0f %.0fms", fps_count / fps_accumulator, (fps_accumulator / fps_count) * 1000.0f);
			fps_text.text(fps_label);
			fps_accumulator = 0.0f;
			fps_count = 0;
		}
	}

	if (visible)
	{
		Font* font = Loader::font_permanent(Asset::Font::SegoeUISymbol);
		bool update = false;
		bool shift = u.input->keys[KEYCODE_LSHIFT]
			|| u.input->keys[KEYCODE_RSHIFT];
		bool any_key_pressed = false;
		for (int i = 1; i < font->characters.length; i++)
		{
			if (i == KEYCODE_GRAVE)
				continue;

			char c = shift ? shift_map[i] : normal_map[i];
			if (!c)
				continue;

			bool add = false;
			if (u.input->keys[i])
			{
				any_key_pressed = true;
				if (!u.input->last_keys[i])
				{
					repeat_start_time = u.time.total;
					add = true;
				}
				else if (u.time.total - repeat_start_time > REPEAT_DELAY &&
					u.time.total - repeat_last_time > REPEAT_INTERVAL)
				{
					repeat_last_time = u.time.total;
					add = true;
				}

				if (add)
				{
					command[command.length - 1] = c;
					command.add(0);
					update = true;
					break;
				}
			}
		}

		if (command.length > 2 && u.input->keys[KEYCODE_BACKSPACE])
		{
			any_key_pressed = true;

			bool remove = false;
			if (!u.input->last_keys[KEYCODE_BACKSPACE])
			{
				repeat_start_time = u.time.total;
				remove = true;
			}
			else if (u.time.total - repeat_start_time > REPEAT_DELAY &&
				u.time.total - repeat_last_time > REPEAT_INTERVAL)
			{
				repeat_last_time = u.time.total;
				remove = true;
			}

			if (remove)
			{
				command.remove(command.length - 1);
				command[command.length - 1] = '\0';
				update = true;
			}
		}

		if (!any_key_pressed)
			repeat_start_time = 0.0f;

		if (u.input->keys[KEYCODE_RETURN])
		{
			visible = false;

			if (strcmp(&command[1], "fps") == 0)
			{
				fps_visible = !fps_visible;
				fps_count = 0;
				fps_accumulator = 0.0f;
			}
			else
				Game::execute(u, &command[1]);

			command.resize(2);
			command[1] = '\0';
			update = true;
		}

		if (update)
			text.text(command.data);
	}

	debug_text.text(debug_buffer.data);
	debug_buffer.length = 0;

	bool update_log = false;
	for (int i = 0; i < logs.length; i++)
	{
		logs[i].timer -= u.time.delta;
		if (logs[i].timer < 0.0f)
		{
			logs.remove_ordered(i);
			i--;
			update_log = true;
		}
	}

	if (update_log)
		Console::update_log();
}

void Console::log(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	Log* log_line = logs.add();

	log_line->timer = LOG_TIME;

#if defined(_WIN32)
	log_line->length = vsprintf_s(log_line->string, 255, format, args);
#else
	log_line->length = vsnprintf(log_line->string, 255, format, args);
#endif

	va_end(args);

	Audio::post_global_event(AK::EVENTS::PLAY_LOG);

	Console::update_log();
}

void Console::update_log()
{
	if (logs.length > 0)
	{
		int total_length = 0;
		for (int i = 0; i < logs.length; i++)
			total_length += logs[i].length + 1;
		Array<char> string(total_length, total_length);

		int index = 0;
		for (int i = logs.length - 1; i >= 0; i--)
		{
			memcpy(&string[index], logs[i].string, logs[i].length);
			index += logs[i].length;
			string[index] = '\n';
			index++;
		}
		string[index - 1] = '\0';
		log_text.text(string.data);
	}

	else
		log_text.text("");
}

void Console::debug(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	char buffer[255];

#if defined(_WIN32)
	vsprintf_s(buffer, 255, format, args);
#else
	vsnprintf(buffer, 255, format, args);
#endif

	va_end(args);

	int buffer_start = debug_buffer.length;
	int buffer_length = strlen(buffer);
	if (debug_buffer.length > 0)
		debug_buffer[debug_buffer.length - 1] = '\n';

	debug_buffer.resize(debug_buffer.length + buffer_length + 1);

	memcpy(&debug_buffer[buffer_start], buffer, sizeof(char) * buffer_length);
	debug_buffer[debug_buffer.length - 1] = '\0';
}

void Console::draw(const RenderParams& p)
{
	if (visible)
		text.draw(p, Vec2(0, p.camera->viewport.height - text.size * UI::scale));
	if (fps_visible)
		fps_text.draw(p, Vec2::zero);

	debug_text.draw(p, Vec2(0, p.camera->viewport.height - text.size * UI::scale * 2.0f));

	log_text.draw(p, Vec2(p.camera->viewport.width - text.size * UI::scale * 2.0f, p.camera->viewport.height - text.size * UI::scale * 4.0f));
}

}