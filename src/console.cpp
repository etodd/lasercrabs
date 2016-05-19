#include "console.h"
#include "game/game.h"
#include "asset/font.h"
#include <cstdio>
#include "load.h"
#include "utf8/utf8.h"

namespace VI
{

Array<char> Console::command;
Array<char> Console::debug_buffer;
Array<Console::Log> Console::logs;
UIText Console::text;
UIText Console::fps_text;
UIText Console::debug_text;
UIText Console::log_text;
b8 Console::fps_visible = false;
s32 Console::fps_count = 0;
r32 Console::fps_accumulator = 0;
b8 Console::visible = false;
char Console::shift_map[127];
char Console::normal_map[127];
r32 Console::repeat_start_time = 0.0f;
r32 Console::repeat_last_time = 0.0f;

#define REPEAT_DELAY 0.2f
#define REPEAT_INTERVAL 0.03f
#define LOG_TIME 8.0f

#define font_asset Asset::Font::lowpoly

void Console::init()
{
	Loader::font_permanent(font_asset);
	text.font = font_asset;
	text.size = 18.0f;
	text.anchor_y = UIText::Anchor::Max;
	fps_text.font = font_asset;
	fps_text.size = 18.0f;
	debug_text.font = font_asset;
	debug_text.size = 18.0f;
	debug_text.anchor_y = UIText::Anchor::Max;
	log_text.font = font_asset;
	log_text.size = 18.0f;
	log_text.color = UI::default_color;
	log_text.anchor_x = UIText::Anchor::Max;
	log_text.anchor_y = UIText::Anchor::Max;

	debug_buffer.resize(1);

	command.resize(2);
	command[0] = '$';

	memset(normal_map, 0, sizeof(normal_map));
	memset(shift_map, 0, sizeof(shift_map));

	normal_map[(s32)KeyCode::D0] = '0';
	normal_map[(s32)KeyCode::D1] = '1';
	normal_map[(s32)KeyCode::D2] = '2';
	normal_map[(s32)KeyCode::D3] = '3';
	normal_map[(s32)KeyCode::D4] = '4';
	normal_map[(s32)KeyCode::D5] = '5';
	normal_map[(s32)KeyCode::D6] = '6';
	normal_map[(s32)KeyCode::D7] = '7';
	normal_map[(s32)KeyCode::D8] = '8';
	normal_map[(s32)KeyCode::D9] = '9';
	shift_map[(s32)KeyCode::D0] = ')';
	shift_map[(s32)KeyCode::D1] = '!';
	shift_map[(s32)KeyCode::D2] = '@';
	shift_map[(s32)KeyCode::D3] = '#';
	shift_map[(s32)KeyCode::D4] = '$';
	shift_map[(s32)KeyCode::D5] = '%';
	shift_map[(s32)KeyCode::D6] = '^';
	shift_map[(s32)KeyCode::D7] = '&';
	shift_map[(s32)KeyCode::D8] = '*';
	shift_map[(s32)KeyCode::D9] = '(';

	normal_map[(s32)KeyCode::Space] = ' ';
	shift_map[(s32)KeyCode::Space] = ' ';

	normal_map[(s32)KeyCode::Apostrophe] = '\'';
	shift_map[(s32)KeyCode::Apostrophe] = '"';

	normal_map[(s32)KeyCode::Minus] = '-';
	normal_map[(s32)KeyCode::Equals] = '=';
	normal_map[(s32)KeyCode::LeftBracket] = '[';
	normal_map[(s32)KeyCode::RightBracket] = ']';
	normal_map[(s32)KeyCode::Comma] = ',';
	normal_map[(s32)KeyCode::Period] = '.';
	normal_map[(s32)KeyCode::Slash] = '/';
	normal_map[(s32)KeyCode::Grave] = '`';
	normal_map[(s32)KeyCode::Semicolon] = ';';
	normal_map[(s32)KeyCode::Backslash] = '\\';
	shift_map[(s32)KeyCode::Minus] = '_';
	shift_map[(s32)KeyCode::Equals] = '+';
	shift_map[(s32)KeyCode::LeftBracket] = '{';
	shift_map[(s32)KeyCode::RightBracket] = '}';
	shift_map[(s32)KeyCode::Comma] = '<';
	shift_map[(s32)KeyCode::Period] = '>';
	shift_map[(s32)KeyCode::Slash] = '?';
	shift_map[(s32)KeyCode::Grave] = '~';
	shift_map[(s32)KeyCode::Semicolon] = ':';
	shift_map[(s32)KeyCode::Backslash] = '|';

	for (s32 i = 0; i < (s32)KeyCode::Z - (s32)KeyCode::A; i++)
	{
		normal_map[(s32)KeyCode::A + i] = 'a' + i;
		shift_map[(s32)KeyCode::A + i] = 'A' + i;
	}

	text.text(command.data);
}

void Console::update(const Update& u)
{
	if (u.input->keys[(s32)KeyCode::Grave]
		&& !u.last_input->keys[(s32)KeyCode::Grave])
		visible = !visible;

	if (fps_visible)
	{
		fps_count += 1;
		fps_accumulator += Game::real_time.delta;
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
		const Font* font = Loader::font_permanent(font_asset);
		b8 update = false;
		b8 shift = u.input->keys[(s32)KeyCode::LShift]
			|| u.input->keys[(s32)KeyCode::RShift];
		b8 any_key_pressed = false;
		for (s32 i = 1; i < font->characters.length; i++)
		{
			if (i == (s32)KeyCode::Grave)
				continue;

			char c = shift ? shift_map[i] : normal_map[i];
			if (!c)
				continue;

			b8 add = false;
			if (u.input->keys[i])
			{
				any_key_pressed = true;
				if (!u.last_input->keys[i])
				{
					repeat_start_time = Game::real_time.total;
					add = true;
				}
				else if (Game::real_time.total - repeat_start_time > REPEAT_DELAY &&
					Game::real_time.total - repeat_last_time > REPEAT_INTERVAL)
				{
					repeat_last_time = Game::real_time.total;
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

		if (command.length > 2 && u.input->keys[(s32)KeyCode::Backspace])
		{
			any_key_pressed = true;

			b8 remove = false;
			if (!u.last_input->keys[(s32)KeyCode::Backspace])
			{
				repeat_start_time = Game::real_time.total;
				remove = true;
			}
			else if (Game::real_time.total - repeat_start_time > REPEAT_DELAY &&
				Game::real_time.total - repeat_last_time > REPEAT_INTERVAL)
			{
				repeat_last_time = Game::real_time.total;
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

		if (!u.input->keys[(s32)KeyCode::Return] && u.last_input->keys[(s32)KeyCode::Return])
		{
			visible = false;

			if (utf8cmp(&command[1], "fps") == 0)
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

	b8 update_log = false;
	for (s32 i = 0; i < logs.length; i++)
	{
		logs[i].timer -= Game::real_time.delta;
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

	Console::update_log();
}

void Console::update_log()
{
	if (logs.length > 0)
	{
		s32 total_length = 0;
		for (s32 i = 0; i < logs.length; i++)
			total_length += logs[i].length + 1;
		Array<char> string(total_length, total_length);

		s32 index = 0;
		for (s32 i = logs.length - 1; i >= 0; i--)
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

	s32 buffer_start = debug_buffer.length;
	s32 buffer_length = strlen(buffer);
	if (debug_buffer.length > 0)
		debug_buffer[debug_buffer.length - 1] = '\n';

	debug_buffer.resize(debug_buffer.length + buffer_length + 1);

	memcpy(&debug_buffer[buffer_start], buffer, sizeof(char) * buffer_length);
	debug_buffer[debug_buffer.length - 1] = '\0';
}

void Console::draw(const RenderParams& p)
{
	const Rect2& vp = p.camera->viewport;
	r32 padding = 6.0f * UI::scale;
	if (visible)
	{
		text.wrap_width = vp.size.x - padding * 2.0f;
		r32 height = text.bounds().y + padding * 2.0f;
		UI::box(p, { Vec2(0, vp.size.y - height), Vec2(vp.size.x, height) }, Vec4(0, 0, 0, 1));
		text.draw(p, Vec2(padding, vp.size.y - padding));
	}
	if (fps_visible)
		fps_text.draw(p, Vec2(0, 0));

	debug_text.draw(p, Vec2(0, vp.size.y - (text.bounds().y + padding * 3.0f)));

	Vec2 pos = Vec2(vp.size.x - text.size * UI::scale * 2.0f, vp.size.y - text.size * UI::scale * 4.0f);
	if (log_text.bounds().x > 0)
	{
		UI::box(p, log_text.rect(pos).outset(padding), Vec4(0, 0, 0, 1));
		log_text.draw(p, pos);
	}
}

}
