#include "console.h"
#include "render/view.h"
#include "render/armature.h"
#include <GLFW/glfw3.h>
#include "game/game.h"

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

	shift_map['0'] = ')';
	shift_map['1'] = '!';
	shift_map['2'] = '@';
	shift_map['3'] = '#';
	shift_map['4'] = '$';
	shift_map['5'] = '%';
	shift_map['6'] = '^';
	shift_map['7'] = '&';
	shift_map['8'] = '*';
	shift_map['9'] = '(';
	shift_map['-'] = '_';
	shift_map['='] = '+';
	shift_map['['] = '{';
	shift_map[']'] = '}';
	shift_map[','] = '<';
	shift_map['.'] = '>';
	shift_map['/'] = '?';
	shift_map['`'] = '`';
	shift_map[';'] = ':';
	shift_map['\''] = '"';
	shift_map['\\'] = '|';

	for (char c = 'A'; c <= 'Z'; c++)
		normal_map[c] = c + 32;
}

void Console::update(const Update& u)
{
	if (u.input->keys['`'] == GLFW_PRESS
		&& u.input->last_keys['`'] != GLFW_PRESS)
		visible = !visible;

	if (visible)
	{
		text.pos = Vec2(0, u.input->height - text.size);
		bool update = false;
		bool shift = u.input->keys[GLFW_KEY_LEFT_SHIFT] == GLFW_PRESS
			|| u.input->keys[GLFW_KEY_RIGHT_SHIFT] == GLFW_PRESS;
		bool any_key_pressed = false;
		for (int i = 0; i < 127; i++)
		{
			if (i == '`')
				continue;

			char c = shift ? shift_map[i] : normal_map[i];
			if (text.font->characters[c].code == c)
			{
				bool add = false;
				if (u.input->keys[i] == GLFW_PRESS)
				{
					any_key_pressed = true;
					if (u.input->last_keys[i] != GLFW_PRESS)
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

		if (command.length > 2 && u.input->keys[GLFW_KEY_BACKSPACE] == GLFW_PRESS)
		{
			any_key_pressed = true;

			bool remove = false;
			if (u.input->last_keys[GLFW_KEY_BACKSPACE] != GLFW_PRESS)
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

		if (u.input->keys[GLFW_KEY_ENTER] == GLFW_PRESS)
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