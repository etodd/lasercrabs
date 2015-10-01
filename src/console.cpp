#include "console.h"
#include "game/game.h"
#include "asset/font.h"

namespace VI
{

Array<char> Console::command = Array<char>();
UIText Console::text = UIText();
UIText Console::fps_text = UIText();
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

void Console::init(int width, int height)
{
	text.font = Asset::Font::SegoeUISymbol;
	text.size = 16.0f * UI::scale;
	fps_text.font = Asset::Font::SegoeUISymbol;
	fps_text.size = 16.0f * UI::scale;
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
		Font* font = Loader::font(Asset::Font::SegoeUISymbol);
		text.pos = Vec2(0, u.input->height - text.size);
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
}

void Console::draw(const RenderParams& p)
{
	if (visible)
		text.draw(p);
	if (fps_visible)
		fps_text.draw(p);
}

}