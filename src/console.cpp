#include "console.h"
#include "game/game.h"
#include "asset/font.h"

namespace VI
{

Array<char> Console::command = Array<char>();
UIText Console::text = UIText();
bool Console::visible = false;
char Console::shift_map[127];
char Console::normal_map[127];
float Console::repeat_start_time = 0.0f;
float Console::repeat_last_time = 0.0f;

#define REPEAT_DELAY 0.2f
#define REPEAT_INTERVAL 0.03f

void Console::init()
{
	text.font = Loader::font(Asset::Font::SegoeUISymbol);
	text.size = 16.0f;
	command.resize(2);
	command[0] = '$';

	for (char c = 0; c < 127; c++)
		shift_map[c] = normal_map[c] = c;

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
	shift_map[KEYCODE_MINUS] = '_';
	shift_map[KEYCODE_EQUALS] = '+';
	shift_map[KEYCODE_LEFTBRACKET] = '{';
	shift_map[KEYCODE_RIGHTBRACKET] = '}';
	shift_map[KEYCODE_COMMA] = '<';
	shift_map[KEYCODE_PERIOD] = '>';
	shift_map[KEYCODE_SLASH] = '?';
	shift_map[KEYCODE_GRAVE] = '`';
	shift_map[KEYCODE_SEMICOLON] = ':';
	shift_map[KEYCODE_BACKSLASH] = '|';

	for (char c = 'A'; c <= 'Z'; c++)
		normal_map[c] = c + 32;
}

void Console::update(const Update& u)
{
	if (u.input->keys[KEYCODE_GRAVE]
		&& !u.input->last_keys[KEYCODE_GRAVE])
		visible = !visible;

	if (visible)
	{
		text.pos = Vec2(0, u.input->height - text.size);
		bool update = false;
		bool shift = u.input->keys[KEYCODE_LSHIFT]
			|| u.input->keys[KEYCODE_RSHIFT];
		bool any_KEYCODE_pressed = false;
		for (int i = 1; i < text.font->characters.length; i++)
		{
			if (i == KEYCODE_GRAVE)
				continue;

			char c = shift ? shift_map[i] : normal_map[i];
			if (text.font->characters[c].code == c)
			{
				bool add = false;
				if (u.input->keys[i])
				{
					any_KEYCODE_pressed = true;
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
		}

		if (command.length > 2 && u.input->keys[KEYCODE_BACKSPACE])
		{
			any_KEYCODE_pressed = true;

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

		if (!any_KEYCODE_pressed)
			repeat_start_time = 0.0f;

		if (u.input->keys[KEYCODE_RETURN])
		{
			visible = false;
			Game::execute(u, &command[1]);
			command.resize(2);
			command[1] = '\0';
			update = true;
		}

		text.text(command.data);
	}
}

void Console::draw(const RenderParams& p)
{
	if (visible)
		text.draw(p);
}

}