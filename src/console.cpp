#include "console.h"
#include "game/game.h"
#include "asset/font.h"
#include <cstdio>
#include "load.h"

namespace VI
{

TextField Console::field;
Array<char> Console::debug_buffer;
Array<Console::Log> Console::logs;
UIText Console::text;
char Console::fps_text[255];
UIText Console::debug_text;
UIText Console::log_text;
b8 Console::fps_visible = false;
s32 Console::fps_count = 0;
r32 Console::fps_accumulator = 0;
r32 Console::longest_frame_time = 0;
b8 Console::visible = false;

#define LOG_TIME 8.0f

#define font_asset Asset::Font::pt_sans

void Console::init()
{
	Loader::font_permanent(font_asset);
	text.font = font_asset;
	text.size = 18.0f;
	text.anchor_y = UIText::Anchor::Max;
	debug_text.font = font_asset;
	debug_text.size = 18.0f;
	debug_text.anchor_y = UIText::Anchor::Max;
	log_text.font = font_asset;
	log_text.size = 18.0f;
	log_text.color = UI::color_default;
	log_text.anchor_x = UIText::Anchor::Max;
	log_text.anchor_y = UIText::Anchor::Max;

	debug_buffer.resize(1);

	field.value.resize(2);
	field.value[0] = '$';
	text.text(0, field.value.data);
}

void Console::update(const Update& u)
{
	if (u.input->get(Controls::Console, 0) && !u.last_input->get(Controls::Console, 0))
		visible = !visible;

	if (fps_visible)
	{
		fps_count += 1;
		fps_accumulator += Game::real_time.delta;
		longest_frame_time = vi_max(Game::real_time.delta, longest_frame_time);
		if (fps_accumulator > 0.5f)
		{
			sprintf(fps_text, "%.0f fps | %.0fms", fps_count / fps_accumulator, longest_frame_time * 1000.0f);
			fps_accumulator = 0.0f;
			fps_count = 0;
			longest_frame_time = 0;
		}
		debug("%s", fps_text);
	}

	if (visible)
	{
		b8 update = field.update(u, 1);
		if (!u.input->keys.get(s32(KeyCode::Return)) && u.last_input->keys.get(s32(KeyCode::Return)))
		{
			visible = false;

			if (strcmp(&field.value[1], "fps") == 0)
			{
				fps_visible = !fps_visible;
				fps_count = 0;
				fps_accumulator = 0.0f;
			}
			else
				Game::execute(&field.value[1]);

			field.value.resize(2);
			field.value[1] = '\0';
			update = true;
		}

		if (update)
			text.text(0, field.value.data);
	}

	debug_text.text(0, debug_buffer.data);
	if (debug_buffer.data)
		debug_buffer.data[0] = '\0';
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
		log_text.text(0, string.data);
	}

	else
		log_text.text(0, "");
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

void Console::draw_ui(const RenderParams& p)
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

	debug_text.draw(p, Vec2(0, vp.size.y - (text.bounds().y + padding * 3.0f)));

	Vec2 pos = Vec2(vp.size.x - text.size * UI::scale * 2.0f, vp.size.y - text.size * UI::scale * 4.0f);
	if (log_text.bounds().x > 0)
	{
		UI::box(p, log_text.rect(pos).outset(padding), Vec4(0, 0, 0, 1));
		log_text.draw(p, pos);
	}
}

}
