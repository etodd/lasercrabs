#include "ui.h"
#include "asset/shader.h"
#include "load.h"
#include "render/render.h"
#include "strings.h"
#include "asset/font.h"
#include "asset/mesh.h"
#include "game/game.h"
#include "game/overworld.h"
#include "settings.h"
#include "game/menu.h"
#include "game/player.h"

namespace VI
{

UIText::UIText()
	: color(UI::color_default),
	font(Asset::Font::lowpoly),
	size(UI_TEXT_SIZE_DEFAULT),
	rendered_string(),
	normalized_bounds(),
	anchor_x(),
	anchor_y(),
	clip(),
	wrap_width()
{
}

Array<UIText::VariableEntry> UIText::variables;

void UIText::variables_clear()
{
	variables.length = 0;
}

void UIText::variable_add(s8 gamepad, const char* name, const char* value)
{
	VariableEntry* entry = variables.add();
	entry->gamepad = gamepad;
	strncpy(entry->name, name, 254);
	strncpy(entry->value, value, 254);
}

void UIText::text(s8 gamepad, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	char string[1024];

	if (!format)
		format = "";

#if defined(_WIN32)
	vsprintf_s(string, 1023, format, args);
#else
	vsnprintf(string, 1023, format, args);
#endif

	va_end(args);

	text_raw(gamepad, string);
}

void UIText::text_raw(s8 gamepad, const char* string, UITextFlags flags)
{
	if (!string)
		string = "";

	{
		s32 char_index = 0;
		s32 rendered_index = 0;

		const char* variable = 0;
		while (true)
		{
			char c = variable && *variable ? *variable : string[char_index];

			if ((flags & UITextFlagSingleLine) && c == '\n')
				c = ' ';

			if (c == '{' && string[char_index + 1] == '{')
			{
				const char* start = &string[char_index + 2];
				const char* end = start;
				while (*end && (*end != '}' || *(end + 1) != '}'))
					end = Font::codepoint_next(end);

				if (*end)
				{
					for (s32 i = 0; i < variables.length; i++)
					{
						const VariableEntry& entry = variables[i];
						if (entry.gamepad == gamepad && strncmp(entry.name, start, end - start) == 0)
						{
							variable = entry.value;

							c = *variable;
							// set up char_index to resume at the end of the variable name once we're done with it
							char_index = end + 2 - string;
							break;
						}
					}
				}
			}

			if (!c)
				break;

			// store the final result in rendered_string
			rendered_string[rendered_index] = c;
			rendered_index++;

			if (variable && *variable)
				variable++;
			else
				char_index++;
		}
		rendered_string[rendered_index] = 0;
	}

	refresh_bounds();
}

b8 UIText::has_text() const
{
	return rendered_string[0] != 0;
}

void UIText::refresh_bounds()
{
	const Font* f = Loader::font(font);
	normalized_bounds = Vec2::zero;
	Vec3 pos(0, -1.0f, 0);
	const char* c = &rendered_string[0];
	const Vec2 spacing = Vec2(0.075f, 0.3f);
	r32 wrap = wrap_width / (size * UI::scale);
	while (*c)
	{
		const Font::Character& character = f->get(c);
		if (*c == '\n')
		{
			pos.x = 0.0f;
			pos.y -= 1.0f + spacing.y;
		}
		else if (wrap > 0.0f && (*c == ' ' || *c == '\t'))
		{
			// check if we need to put the next word on the next line

			r32 end_of_next_word = pos.x + spacing.x + character.max.x;
			const char* word_char = Font::codepoint_next(c);
			while (true)
			{
				if (!(*word_char) || *word_char == ' ' || *word_char == '\t' || *word_char == '\n')
					break;
				end_of_next_word += spacing.x + f->get(word_char).max.x;
				word_char = Font::codepoint_next(word_char);
			}

			if (end_of_next_word > wrap)
			{
				// new line
				pos.x = 0.0f;
				pos.y -= 1.0f + spacing.y;
			}
			else
			{
				// just a regular whitespace character
				pos.x += spacing.x + character.max.x;
			}
		}
		else
		{
			if (character.codepoint == Font::codepoint(c))
				pos.x += spacing.x + character.max.x;
			else
			{
				// font is missing character
			}
		}

		normalized_bounds.x = vi_max(normalized_bounds.x, pos.x);

		c = Font::codepoint_next(c);
	}

	normalized_bounds.y = -pos.y;
}

b8 UIText::clipped() const
{
	return clip > 0 && clip < Font::codepoint_count(rendered_string);
}

void UIText::set_size(r32 s)
{
	size = s;
	refresh_bounds();
}

void UIText::wrap(r32 w)
{
	wrap_width = w;
	refresh_bounds();
}

Vec2 UIText::bounds() const
{
	Vec2 b = normalized_bounds * size * UI::scale;
	if (wrap_width > 0.0f)
		b.x = wrap_width;
	return b;
}

Rect2 UIText::rect(const Vec2& pos) const
{
	Rect2 result;
	result.size = bounds();
	switch (anchor_x)
	{
		case Anchor::Min:
			result.pos.x = pos.x;
			break;
		case Anchor::Center:
			result.pos.x = pos.x + result.size.x * -0.5f;
			break;
		case Anchor::Max:
			result.pos.x = pos.x - result.size.x;
			break;
		default:
			vi_assert(false);
			break;
	}
	switch (anchor_y)
	{
		case Anchor::Min:
			result.pos.y = pos.y;
			break;
		case Anchor::Center:
			result.pos.y = pos.y + result.size.y * -0.5f;
			break;
		case Anchor::Max:
			result.pos.y = pos.y - result.size.y;
			break;
		default:
			vi_assert(false);
			break;
	}
	return result;
}

void UIText::draw(const RenderParams& params, const Vec2& pos, r32 rot) const
{
	s32 vertex_start = UI::vertices.length;
	Vec2 screen = params.camera->viewport.size * 0.5f;
	Vec2 offset = pos - screen;
	Vec2 bound = bounds();
	switch (anchor_x)
	{
		case Anchor::Min:
			break;
		case Anchor::Center:
			offset.x += bound.x * -0.5f;
			break;
		case Anchor::Max:
			offset.x -= bound.x;
			break;
		default:
			vi_assert(false);
			break;
	}
	switch (anchor_y)
	{
		case Anchor::Min:
			offset.y += bound.y;
			break;
		case Anchor::Center:
			offset.y += bound.y * 0.5f;
			break;
		case Anchor::Max:
			break;
		default:
			vi_assert(false);
			break;
	}
	Vec2 scale = Vec2(1.0f / screen.x, 1.0f / screen.y);
	r32 cs = cosf(rot), sn = sinf(rot);

	const Font* f = Loader::font(font);
	s32 vertex_index = UI::vertices.length;
	s32 index_index = UI::indices.length;
	Vec3 p(0, -1.0f, 0);
	const char* c = &rendered_string[0];
	const Vec2 spacing = Vec2(0.075f, 0.3f);
	r32 scaled_size = size * UI::scale;
	r32 wrap = wrap_width / scaled_size;
	s32 char_index = 0;
	while (*c)
	{
		b8 clipped = clip > 0 && char_index == clip - 1;
		const Font::Character* character = &f->get(c);
		if (*c == '\n')
		{
			p.x = 0.0f;
			p.y -= 1.0f + spacing.y;
		}
		else if (wrap > 0.0f && (*c == ' ' || *c == '\t'))
		{
			// check if we need to put the next word on the next line

			r32 end_of_next_word = p.x + spacing.x + character->max.x;
			const char* word_char = Font::codepoint_next(c);
			while (true)
			{
				if (!(*word_char) || *word_char == ' ' || *word_char == '\t' || *word_char == '\n')
					break;
				end_of_next_word += spacing.x + f->get(word_char).max.x;
				word_char = Font::codepoint_next(word_char);
			}

			if (end_of_next_word > wrap)
			{
				// new line
				p.x = 0.0f;
				p.y -= 1.0f + spacing.y;
			}
			else
			{
				// just a regular whitespace character
				p.x += spacing.x + character->max.x;
			}
		}
		else
		{
			b8 valid_character;
			if (character->codepoint == Font::codepoint(c))
				valid_character = true;
			else
			{
				valid_character = false;
				character = &f->get(" ");
			}
			if (clipped || !valid_character)
			{
				// draw character as a rectangle
				Vec3 v0 = p + Vec3(character->min.x, character->min.y, 0);
				Vec3 v1 = p + Vec3(character->max.x, character->min.y, 0);
				Vec3 v2 = p + Vec3(character->min.x, character->max.y, 0);
				Vec3 v3 = p + Vec3(character->max.x, character->max.y, 0);
				{
					Vec3 vertex;
					vertex.x = (offset.x + scaled_size * (v0.x * cs - v0.y * sn)) * scale.x;
					vertex.y = (offset.y + scaled_size * (v0.x * sn + v0.y * cs)) * scale.y;
					UI::vertices.add(vertex);
				}
				{
					Vec3 vertex;
					vertex.x = (offset.x + scaled_size * (v1.x * cs - v1.y * sn)) * scale.x;
					vertex.y = (offset.y + scaled_size * (v1.x * sn + v1.y * cs)) * scale.y;
					UI::vertices.add(vertex);
				}
				{
					Vec3 vertex;
					vertex.x = (offset.x + scaled_size * (v2.x * cs - v2.y * sn)) * scale.x;
					vertex.y = (offset.y + scaled_size * (v2.x * sn + v2.y * cs)) * scale.y;
					UI::vertices.add(vertex);
				}
				{
					Vec3 vertex;
					vertex.x = (offset.x + scaled_size * (v3.x * cs - v3.y * sn)) * scale.x;
					vertex.y = (offset.y + scaled_size * (v3.x * sn + v3.y * cs)) * scale.y;
					UI::vertices.add(vertex);
				}
				UI::colors.add(color);
				UI::colors.add(color);
				UI::colors.add(color);
				UI::colors.add(color);
				UI::indices.add(vertex_index + 0);
				UI::indices.add(vertex_index + 1);
				UI::indices.add(vertex_index + 2);
				UI::indices.add(vertex_index + 1);
				UI::indices.add(vertex_index + 3);
				UI::indices.add(vertex_index + 2);
			}
			else
			{
				UI::vertices.resize(vertex_index + character->vertex_count);
				UI::colors.resize(UI::vertices.length);
				for (s32 i = 0; i < character->vertex_count; i++)
				{
					Vec3 v = p + f->vertices[character->vertex_start + i];
					Vec3 vertex;
					vertex.x = (offset.x + scaled_size * (v.x * cs - v.y * sn)) * scale.x;
					vertex.y = (offset.y + scaled_size * (v.x * sn + v.y * cs)) * scale.y;
					UI::vertices[vertex_index + i] = vertex;
					UI::colors[vertex_index + i] = color;
				}

				UI::indices.resize(index_index + character->index_count);
				for (s32 i = 0; i < character->index_count; i++)
					UI::indices[index_index + i] = vertex_index + f->indices[character->index_start + i] - character->vertex_start;
			}

			p.x += spacing.x + character->max.x;

			vertex_index = UI::vertices.length;
			index_index = UI::indices.length;
		}

		if (clipped)
			break;

		c = Font::codepoint_next(c);
		char_index++;
	}
}

s32 UI::input_delta_vertical(const Update& u, s32 gamepad)
{
	s32 result = 0;
	// joystick
	if (u.input->gamepads[gamepad].type != Gamepad::Type::None)
	{
		Vec2 last_joystick(u.last_input->gamepads[gamepad].left_x, u.last_input->gamepads[gamepad].left_y);
		Input::dead_zone(&last_joystick.x, &last_joystick.y, UI_JOYSTICK_DEAD_ZONE);
		if (last_joystick.y == 0.0f)
		{
			Vec2 current_joystick(u.input->gamepads[gamepad].left_x, u.input->gamepads[gamepad].left_y);
			Input::dead_zone(&current_joystick.x, &current_joystick.y, UI_JOYSTICK_DEAD_ZONE);
			r32 threshold = fabsf(current_joystick.x);
			if (current_joystick.y < -threshold)
				result--;
			else if (current_joystick.y > threshold)
				result++;
		}
	}

	// keyboard / D-pad
	if ((u.input->get(Controls::Forward, gamepad) && !u.last_input->get(Controls::Forward, gamepad)))
		result--;

	if (u.input->get(Controls::Backward, gamepad) && !u.last_input->get(Controls::Backward, gamepad))
		result++;

	return result;
}

s32 UI::input_delta_horizontal(const Update& u, s32 gamepad)
{
	s32 result = 0;
	// joystick
	if (u.input->gamepads[gamepad].type != Gamepad::Type::None)
	{
		Vec2 last_joystick(u.last_input->gamepads[gamepad].left_x, u.last_input->gamepads[gamepad].left_y);
		Input::dead_zone(&last_joystick.x, &last_joystick.y, UI_JOYSTICK_DEAD_ZONE);
		if (last_joystick.x == 0.0f)
		{
			Vec2 current_joystick(u.input->gamepads[gamepad].left_x, u.input->gamepads[gamepad].left_y);
			Input::dead_zone(&current_joystick.x, &current_joystick.y, UI_JOYSTICK_DEAD_ZONE);
			r32 threshold = fabsf(current_joystick.y);
			if (current_joystick.x < -threshold)
				result--;
			else if (current_joystick.x > threshold)
				result++;
		}
	}

	// keyboard / D-pad
	if ((u.input->get(Controls::Left, gamepad) && !u.last_input->get(Controls::Left, gamepad)))
		result--;

	if (u.input->get(Controls::Right, gamepad) && !u.last_input->get(Controls::Right, gamepad))
		result++;

	return result;
}

// gamepad = -1 by default, meaning no input will be processed
void UIScroll::update(const Update& u, s32 item_count, s32 gamepad)
{
	update_menu(item_count);

	if (gamepad >= 0)
	{
		pos += UI::input_delta_vertical(u, gamepad);

		// keep within range
		pos = vi_max(0, vi_min(count - size, pos));
	}
}

void UIScroll::update_menu(s32 item_count)
{
	count = item_count;

	// keep within range
	pos = vi_max(0, vi_min(count - size, pos));
}

void UIScroll::scroll_into_view(s32 i)
{
	pos = vi_min(i, pos);
	pos = vi_max(i + 1 - size, pos);
}

void UIScroll::start(const RenderParams& params, const Vec2& p) const
{
	if (pos > 0)
	{
		Vec2 p2 = p + Vec2(0, 16.0f * UI::scale);
		UI::centered_box(params, { p2, Vec2(32.0f * UI::scale) }, UI::color_background);
		UI::triangle(params, { p2, Vec2(16.0f * UI::scale) }, UI::color_accent());
	}
}

void UIScroll::end(const RenderParams& params, const Vec2& p) const
{
	if (pos + size < count)
	{
		Vec2 p2 = p + Vec2(0, -16.0f * UI::scale);
		UI::centered_box(params, { p2, Vec2(32.0f * UI::scale) }, UI::color_background);
		UI::triangle(params, { p2, Vec2(16.0f * UI::scale) }, UI::color_accent(), PI);
	}
}

s32 UIScroll::top() const
{
	return pos;
}

s32 UIScroll::bottom(s32 items) const
{
	return vi_min(items, pos + size);
}

b8 UIScroll::visible(s32 i) const
{
	return i >= pos && i < pos + size;
}

const Vec4 UI::color_default = Vec4(1, 1, 1, 1);
const Vec4 UI::color_background = Vec4(0, 0, 0, 1);

const Vec4 color_alert_pvp = Vec4(1.0f, 0.4f, 0.4f, 1);
const Vec4 color_alert_normal = Vec4(255.0f / 255.0f, 115.0f / 255.0f, 200.0f / 255.0f, 1);
const Vec4& UI::color_alert()
{
	return Overworld::pvp_colors() ? color_alert_pvp : color_alert_normal;
}

const Vec4 color_accent_pvp = Vec4(1.0f, 0.95f, 0.35f, 1);
const Vec4 color_accent_normal = Vec4(1.0f, 1.0f, 0.4f, 1);
const Vec4& UI::color_accent()
{
	return Overworld::pvp_colors() ? color_accent_pvp : color_accent_normal;
}

const Vec4 color_disabled_pvp = Vec4(0.5f, 0.5f, 0.5f, 1);
const Vec4 color_disabled_normal = Vec4(0.75f, 0.75f, 0.75f, 1);
const Vec4& UI::color_disabled()
{
	return Overworld::pvp_colors() ? color_disabled_pvp : color_disabled_normal;
}

const Vec4& UI::color_ping(r32 p)
{
	if (p < 0.1f)
		return color_default;
	else if (p < 0.2f)
		return color_accent();
	else
		return color_alert();
}

Vec2 UI::cursor_pos(200, 200);
r32 UI::scale = 1.0f;
AssetID UI::mesh_id = AssetNull;
AssetID UI::texture_mesh_id = AssetNull;
Array<Vec3> UI::vertices;
Array<Vec4> UI::colors;
Array<s32> UI::indices;
Array<UI::TextureBlit> UI::texture_blits;

void UI::box(const RenderParams& params, const Rect2& r, const Vec4& color)
{
	if (r.size.x > 0 && r.size.y > 0 && color.w > 0)
	{
		s32 vertex_start = UI::vertices.length;
		Vec2 screen = params.camera->viewport.size * 0.5f;
		Vec2 scale = Vec2(1.0f / screen.x, 1.0f / screen.y);
		Vec2 scaled_pos = (r.pos - screen) * scale;
		Vec2 scaled_size = r.size * scale;

		UI::vertices.add(Vec3(scaled_pos.x, scaled_pos.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x + scaled_size.x, scaled_pos.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x, scaled_pos.y + scaled_size.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x + scaled_size.x, scaled_pos.y + scaled_size.y, 0));
		UI::colors.add(color);
		UI::colors.add(color);
		UI::colors.add(color);
		UI::colors.add(color);
		UI::indices.add(vertex_start + 0);
		UI::indices.add(vertex_start + 1);
		UI::indices.add(vertex_start + 2);
		UI::indices.add(vertex_start + 1);
		UI::indices.add(vertex_start + 3);
		UI::indices.add(vertex_start + 2);
	}
}

void UI::centered_box(const RenderParams& params, const Rect2& r, const Vec4& color, r32 rot)
{
	if (r.size.x > 0 && r.size.y > 0 && color.w > 0)
	{
		s32 vertex_start = UI::vertices.length;
		Vec2 screen = params.camera->viewport.size * 0.5f;
		Vec2 scale = Vec2(1.0f / screen.x, 1.0f / screen.y);
		Vec2 scaled_pos = (r.pos - screen) * scale;

		Vec2 corners[4] =
		{
			Vec2(r.size.x * -0.5f, r.size.y * -0.5f),
			Vec2(r.size.x * 0.5f, r.size.y * -0.5f),
			Vec2(r.size.x * -0.5f, r.size.y * 0.5f),
			Vec2(r.size.x * 0.5f, r.size.y * 0.5f),
		};

		r32 cs = cosf(rot), sn = sinf(rot);
		UI::vertices.add(Vec3(scaled_pos.x + (corners[0].x * cs - corners[0].y * sn) * scale.x, scaled_pos.y + (corners[0].x * sn + corners[0].y * cs) * scale.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x + (corners[1].x * cs - corners[1].y * sn) * scale.x, scaled_pos.y + (corners[1].x * sn + corners[1].y * cs) * scale.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x + (corners[2].x * cs - corners[2].y * sn) * scale.x, scaled_pos.y + (corners[2].x * sn + corners[2].y * cs) * scale.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x + (corners[3].x * cs - corners[3].y * sn) * scale.x, scaled_pos.y + (corners[3].x * sn + corners[3].y * cs) * scale.y, 0));
		UI::colors.add(color);
		UI::colors.add(color);
		UI::colors.add(color);
		UI::colors.add(color);
		UI::indices.add(vertex_start + 0);
		UI::indices.add(vertex_start + 1);
		UI::indices.add(vertex_start + 2);
		UI::indices.add(vertex_start + 1);
		UI::indices.add(vertex_start + 3);
		UI::indices.add(vertex_start + 2);
	}
}

void UI::border(const RenderParams& params, const Rect2& r, r32 thickness, const Vec4& color)
{
	if (r.size.x > 0 && r.size.y > 0 && color.w > 0)
	{
		s32 vertex_start = UI::vertices.length;
		Vec2 screen = params.camera->viewport.size * 0.5f;
		Vec2 scale = Vec2(1.0f / screen.x, 1.0f / screen.y);
		Vec2 scaled_pos = (r.pos - screen) * scale;
		Vec2 scaled_size = r.size * scale;
		Vec2 scaled_thickness = Vec2(thickness, thickness) * scale;

		UI::vertices.add(Vec3(scaled_pos.x, scaled_pos.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x + scaled_size.x, scaled_pos.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x, scaled_pos.y + scaled_size.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x + scaled_size.x, scaled_pos.y + scaled_size.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x - scaled_thickness.x, scaled_pos.y - scaled_thickness.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x + scaled_size.x + scaled_thickness.x, scaled_pos.y - scaled_thickness.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x - scaled_thickness.x, scaled_pos.y + scaled_size.y + scaled_thickness.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x + scaled_size.x + scaled_thickness.x, scaled_pos.y + scaled_size.y + scaled_thickness.y, 0));
		UI::colors.add(color);
		UI::colors.add(color);
		UI::colors.add(color);
		UI::colors.add(color);
		UI::colors.add(color);
		UI::colors.add(color);
		UI::colors.add(color);
		UI::colors.add(color);
		UI::indices.add(vertex_start + 0);
		UI::indices.add(vertex_start + 4);
		UI::indices.add(vertex_start + 5);
		UI::indices.add(vertex_start + 0);
		UI::indices.add(vertex_start + 5);
		UI::indices.add(vertex_start + 1);
		UI::indices.add(vertex_start + 1);
		UI::indices.add(vertex_start + 5);
		UI::indices.add(vertex_start + 3);
		UI::indices.add(vertex_start + 5);
		UI::indices.add(vertex_start + 7);
		UI::indices.add(vertex_start + 3);
		UI::indices.add(vertex_start + 3);
		UI::indices.add(vertex_start + 7);
		UI::indices.add(vertex_start + 2);
		UI::indices.add(vertex_start + 7);
		UI::indices.add(vertex_start + 6);
		UI::indices.add(vertex_start + 2);
		UI::indices.add(vertex_start + 6);
		UI::indices.add(vertex_start + 0);
		UI::indices.add(vertex_start + 2);
		UI::indices.add(vertex_start + 6);
		UI::indices.add(vertex_start + 4);
		UI::indices.add(vertex_start + 0);
	}
}

void UI::centered_border(const RenderParams& params, const Rect2& r, r32 thickness, const Vec4& color, r32 rot)
{
	if (r.size.x > 0 && r.size.y > 0 && color.w > 0)
	{
		s32 vertex_start = UI::vertices.length;
		const Vec2 screen = params.camera->viewport.size * 0.5f;
		const Vec2 scale = Vec2(1.0f / screen.x, 1.0f / screen.y);
		const Vec2 scaled_pos = (r.pos - screen) * scale;
		r32 scaled_thickness = thickness * UI::scale;

		const Vec2 corners[8] =
		{
			Vec2(r.size.x * -0.5f, r.size.y * -0.5f),
			Vec2(r.size.x * 0.5f, r.size.y * -0.5f),
			Vec2(r.size.x * -0.5f, r.size.y * 0.5f),
			Vec2(r.size.x * 0.5f, r.size.y * 0.5f),
			Vec2(r.size.x * -0.5f - scaled_thickness, r.size.y * -0.5f - scaled_thickness),
			Vec2(r.size.x * 0.5f + scaled_thickness, r.size.y * -0.5f - scaled_thickness),
			Vec2(r.size.x * -0.5f - scaled_thickness, r.size.y * 0.5f + scaled_thickness),
			Vec2(r.size.x * 0.5f + scaled_thickness, r.size.y * 0.5f + scaled_thickness),
		};

		r32 cs = cosf(rot);
		r32 sn = sinf(rot);
		UI::vertices.add(Vec3(scaled_pos.x + (corners[0].x * cs - corners[0].y * sn) * scale.x, scaled_pos.y + (corners[0].x * sn + corners[0].y * cs) * scale.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x + (corners[1].x * cs - corners[1].y * sn) * scale.x, scaled_pos.y + (corners[1].x * sn + corners[1].y * cs) * scale.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x + (corners[2].x * cs - corners[2].y * sn) * scale.x, scaled_pos.y + (corners[2].x * sn + corners[2].y * cs) * scale.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x + (corners[3].x * cs - corners[3].y * sn) * scale.x, scaled_pos.y + (corners[3].x * sn + corners[3].y * cs) * scale.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x + (corners[4].x * cs - corners[4].y * sn) * scale.x, scaled_pos.y + (corners[4].x * sn + corners[4].y * cs) * scale.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x + (corners[5].x * cs - corners[5].y * sn) * scale.x, scaled_pos.y + (corners[5].x * sn + corners[5].y * cs) * scale.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x + (corners[6].x * cs - corners[6].y * sn) * scale.x, scaled_pos.y + (corners[6].x * sn + corners[6].y * cs) * scale.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x + (corners[7].x * cs - corners[7].y * sn) * scale.x, scaled_pos.y + (corners[7].x * sn + corners[7].y * cs) * scale.y, 0));

		UI::colors.add(color);
		UI::colors.add(color);
		UI::colors.add(color);
		UI::colors.add(color);
		UI::colors.add(color);
		UI::colors.add(color);
		UI::colors.add(color);
		UI::colors.add(color);
		UI::indices.add(vertex_start + 0);
		UI::indices.add(vertex_start + 4);
		UI::indices.add(vertex_start + 5);
		UI::indices.add(vertex_start + 0);
		UI::indices.add(vertex_start + 5);
		UI::indices.add(vertex_start + 1);
		UI::indices.add(vertex_start + 1);
		UI::indices.add(vertex_start + 5);
		UI::indices.add(vertex_start + 3);
		UI::indices.add(vertex_start + 5);
		UI::indices.add(vertex_start + 7);
		UI::indices.add(vertex_start + 3);
		UI::indices.add(vertex_start + 3);
		UI::indices.add(vertex_start + 7);
		UI::indices.add(vertex_start + 2);
		UI::indices.add(vertex_start + 7);
		UI::indices.add(vertex_start + 6);
		UI::indices.add(vertex_start + 2);
		UI::indices.add(vertex_start + 6);
		UI::indices.add(vertex_start + 0);
		UI::indices.add(vertex_start + 2);
		UI::indices.add(vertex_start + 6);
		UI::indices.add(vertex_start + 4);
		UI::indices.add(vertex_start + 0);
	}
}

void UI::triangle(const RenderParams& params, const Rect2& r, const Vec4& color, r32 rot)
{
	if (r.size.x > 0 && r.size.y > 0 && color.w > 0)
	{
		s32 vertex_start = UI::vertices.length;
		Vec2 screen = params.camera->viewport.size * 0.5f;
		Vec2 scale = Vec2(1.0f / screen.x, 1.0f / screen.y);
		Vec2 scaled_pos = (r.pos - screen) * scale;

		const r32 ratio = 0.8660254037844386f;
		Vec2 corners[3] =
		{
			Vec2(r.size.x * 0.5f * ratio, r.size.y * -0.25f),
			Vec2(0, r.size.y * 0.5f),
			Vec2(r.size.x * -0.5f * ratio, r.size.y * -0.25f),
		};

		r32 cs = cosf(rot), sn = sinf(rot);
		UI::vertices.add(Vec3(scaled_pos.x + (corners[0].x * cs - corners[0].y * sn) * scale.x, scaled_pos.y + (corners[0].x * sn + corners[0].y * cs) * scale.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x + (corners[1].x * cs - corners[1].y * sn) * scale.x, scaled_pos.y + (corners[1].x * sn + corners[1].y * cs) * scale.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x + (corners[2].x * cs - corners[2].y * sn) * scale.x, scaled_pos.y + (corners[2].x * sn + corners[2].y * cs) * scale.y, 0));
		UI::colors.add(color);
		UI::colors.add(color);
		UI::colors.add(color);
		UI::indices.add(vertex_start + 0);
		UI::indices.add(vertex_start + 1);
		UI::indices.add(vertex_start + 2);
	}
}

void UI::triangle_percentage(const RenderParams& params, const Rect2& r, r32 percent, const Vec4& color, r32 rot)
{
	if (r.size.x > 0 && r.size.y > 0 && color.w > 0)
	{
		s32 vertex_start = UI::vertices.length;
		const Vec2 screen = params.camera->viewport.size * 0.5f;
		const Vec2 scale = Vec2(1.0f / screen.x, 1.0f / screen.y);
		const Vec2 scaled_pos = (r.pos - screen) * scale;

		const r32 ratio = 0.8660254037844386f;
		const Vec2 corners[3] =
		{
			Vec2(r.size.x * 0.5f * ratio, r.size.y * -0.25f),
			Vec2(0, r.size.y * 0.5f),
			Vec2(r.size.x * -0.5f * ratio, r.size.y * -0.25f),
		};

		r32 cs = cosf(rot);
		r32 sn = sinf(rot);
		const Vec3 transformed_corners[3] =
		{
			Vec3(scaled_pos.x + (corners[0].x * cs - corners[0].y * sn) * scale.x, scaled_pos.y + (corners[0].x * sn + corners[0].y * cs) * scale.y, 0),
			Vec3(scaled_pos.x + (corners[1].x * cs - corners[1].y * sn) * scale.x, scaled_pos.y + (corners[1].x * sn + corners[1].y * cs) * scale.y, 0),
			Vec3(scaled_pos.x + (corners[2].x * cs - corners[2].y * sn) * scale.x, scaled_pos.y + (corners[2].x * sn + corners[2].y * cs) * scale.y, 0),
		};
		UI::vertices.add(transformed_corners[0]);
		UI::vertices.add(transformed_corners[1]);
		UI::vertices.add(transformed_corners[2]);
		UI::vertices.add(Vec3(scaled_pos.x, scaled_pos.y, 0)); // center

		UI::colors.add(color);
		UI::colors.add(color);
		UI::colors.add(color);
		UI::colors.add(color);

		// vertices and animation go clockwise starting at bottom right
		r32 stop_angle = PI * -(30.0f / 180.0f) - percent * (PI * 2.0f);
		Vec2 stop_local = Vec2(cosf(stop_angle), sinf(stop_angle)) * r.size.x * 0.5f;
		// clamp stop vertex to the triangle
		Vec3 stop_projected = LMath::triangle_closest_point
		(
			transformed_corners[0],
			transformed_corners[1],
			transformed_corners[2],
			Vec3(scaled_pos.x + (stop_local.x * cs - stop_local.y * sn) * scale.x, scaled_pos.y + (stop_local.x * sn + stop_local.y * cs) * scale.y, 0)
		);
		UI::vertices.add(stop_projected);
		UI::colors.add(color);

		if (percent <= 0.3333f)
		{
			// stop here
			UI::indices.add(vertex_start + 0); // bottom right
			UI::indices.add(vertex_start + 3); // center
			UI::indices.add(vertex_start + 4); // stop
		}
		else // percent > 0.3333f
		{
			UI::indices.add(vertex_start + 0); // bottom right
			UI::indices.add(vertex_start + 3); // center
			UI::indices.add(vertex_start + 2); // bottom left

			if (percent <= 0.6666f)
			{
				// stop here
				UI::indices.add(vertex_start + 2); // bottom left
				UI::indices.add(vertex_start + 3); // center
				UI::indices.add(vertex_start + 4); // stop
			}
			else // percent > 0.6666f
			{
				UI::indices.add(vertex_start + 2); // bottom left
				UI::indices.add(vertex_start + 3); // center
				UI::indices.add(vertex_start + 1); // top

				// stop here
				UI::indices.add(vertex_start + 1); // top
				UI::indices.add(vertex_start + 3); // center
				UI::indices.add(vertex_start + 4); // stop
			}
		}
	}
}

void UI::triangle_border(const RenderParams& params, const Rect2& r, r32 thickness, const Vec4& color, r32 rot)
{
	if (r.size.x > 0 && r.size.y > 0 && color.w > 0)
	{
		s32 vertex_start = UI::vertices.length;
		const Vec2 screen = params.camera->viewport.size * 0.5f;
		const Vec2 scale = Vec2(1.0f / screen.x, 1.0f / screen.y);
		const Vec2 scaled_pos = (r.pos - screen) * scale;
		r32 scaled_thickness = thickness * UI::scale * 1.5f;

		const r32 ratio = 0.8660254037844386f;
		const Vec2 corners[6] =
		{
			Vec2(r.size.x * -0.5f * ratio, r.size.y * -0.25f),
			Vec2(r.size.x * 0.5f * ratio, r.size.y * -0.25f),
			Vec2(0, r.size.y * 0.5f),
			Vec2((r.size.x * -0.5f - scaled_thickness) * ratio, r.size.y * -0.25f - (scaled_thickness * 0.5f)),
			Vec2((r.size.x * 0.5f + scaled_thickness) * ratio, r.size.y * -0.25f - (scaled_thickness * 0.5f)),
			Vec2(0, r.size.y * 0.5f + scaled_thickness),
		};

		r32 cs = cosf(rot);
		r32 sn = sinf(rot);
		UI::vertices.add(Vec3(scaled_pos.x + (corners[0].x * cs - corners[0].y * sn) * scale.x, scaled_pos.y + (corners[0].x * sn + corners[0].y * cs) * scale.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x + (corners[1].x * cs - corners[1].y * sn) * scale.x, scaled_pos.y + (corners[1].x * sn + corners[1].y * cs) * scale.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x + (corners[2].x * cs - corners[2].y * sn) * scale.x, scaled_pos.y + (corners[2].x * sn + corners[2].y * cs) * scale.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x + (corners[3].x * cs - corners[3].y * sn) * scale.x, scaled_pos.y + (corners[3].x * sn + corners[3].y * cs) * scale.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x + (corners[4].x * cs - corners[4].y * sn) * scale.x, scaled_pos.y + (corners[4].x * sn + corners[4].y * cs) * scale.y, 0));
		UI::vertices.add(Vec3(scaled_pos.x + (corners[5].x * cs - corners[5].y * sn) * scale.x, scaled_pos.y + (corners[5].x * sn + corners[5].y * cs) * scale.y, 0));

		UI::colors.add(color);
		UI::colors.add(color);
		UI::colors.add(color);
		UI::colors.add(color);
		UI::colors.add(color);
		UI::colors.add(color);

		UI::indices.add(vertex_start + 2);
		UI::indices.add(vertex_start + 3);
		UI::indices.add(vertex_start + 0);
		UI::indices.add(vertex_start + 3);
		UI::indices.add(vertex_start + 2);
		UI::indices.add(vertex_start + 5);

		UI::indices.add(vertex_start + 5);
		UI::indices.add(vertex_start + 2);
		UI::indices.add(vertex_start + 4);
		UI::indices.add(vertex_start + 4);
		UI::indices.add(vertex_start + 2);
		UI::indices.add(vertex_start + 1);

		UI::indices.add(vertex_start + 3);
		UI::indices.add(vertex_start + 1);
		UI::indices.add(vertex_start + 0);
		UI::indices.add(vertex_start + 3);
		UI::indices.add(vertex_start + 4);
		UI::indices.add(vertex_start + 1);
	}
}

void UI::mesh(const RenderParams& params, const AssetID mesh, const Vec2& pos, const Vec2& size, const Vec4& color, r32 rot)
{
	if (size.x > 0 && size.y > 0 && color.w > 0)
	{
		s32 vertex_start = UI::vertices.length;
		Vec2 screen = params.camera->viewport.size * 0.5f;
		Vec2 scale = Vec2(1.0f / screen.x, 1.0f / screen.y);
		Vec2 scaled_pos = (pos - screen) * scale;
		scale *= size;

		r32 cs = cosf(rot), sn = sinf(rot);

		const Mesh* m = Loader::mesh(mesh);
		for (s32 i = 0; i < m->vertices.length; i++)
		{
			UI::vertices.add(Vec3(scaled_pos.x + (m->vertices[i].x * cs - m->vertices[i].y * sn) * scale.x, scaled_pos.y + (m->vertices[i].x * sn + m->vertices[i].y * cs) * scale.y, 0));
			UI::colors.add(color);
		}
		for (s32 i = 0; i < m->indices.length; i++)
			UI::indices.add(vertex_start + m->indices[i]);
	}
}

b8 UI::project(const RenderParams& p, const Vec3& v, Vec2* out)
{
	return project(p.view_projection, p.camera->viewport, v, out);
}

b8 UI::project(const Mat4& view_projection, const Rect2& viewport, const Vec3& v, Vec2* out)
{
	Vec4 projected = view_projection * Vec4(v.x, v.y, v.z, 1);
	Vec2 screen = viewport.size * 0.5f;
	*out = Vec2((projected.x / projected.w + 1.0f) * screen.x, (projected.y / projected.w + 1.0f) * screen.y);
	return projected.z > -projected.w;
}

void UI::init(LoopSync* sync)
{
	mesh_id = Loader::dynamic_mesh_permanent(2);
	Loader::dynamic_mesh_attrib(RenderDataType::Vec3);
	Loader::dynamic_mesh_attrib(RenderDataType::Vec4);
	Loader::shader_permanent(Asset::Shader::ui);

	texture_mesh_id = Loader::dynamic_mesh_permanent(3);
	Loader::dynamic_mesh_attrib(RenderDataType::Vec3);
	Loader::dynamic_mesh_attrib(RenderDataType::Vec4);
	Loader::dynamic_mesh_attrib(RenderDataType::Vec2);
	Loader::shader_permanent(Asset::Shader::ui_texture);

	s32 indices[] =
	{
		0,
		1,
		2,
		1,
		3,
		2
	};

	sync->write(RenderOp::UpdateIndexBuffer);
	sync->write(texture_mesh_id);
	sync->write<s32>(6);
	sync->write(indices, 6);

	scale = get_scale(Settings::display().width, Settings::display().height);
}

r32 UI::get_scale(s32 width, s32 height)
{
	s32 area = width * height;
	if (area > 1920 * 1080)
		return 1.5f;
	else if (area > 1280 * 720)
		return 1.25f;
	else if (area > 640 * 480 && height > 480)
		return 1.0f;
	else
		return 0.6f;
}

void UI::update()
{
	if (Camera::list.count() > 0)
	{
		const Camera* camera = Camera::list.iterator().item();
		const Rect2& viewport = camera->viewport;
		scale = get_scale(viewport.size.x, viewport.size.y);
	}
}

b8 UI::cursor_active()
{
#if SERVER
	return false;
#else
	if (Game::ui_gamepad_types[0] == Gamepad::Type::None)
	{
		if (UIMenu::active[0] || Menu::dialog_active(0) || Overworld::active())
			return true;

		PlayerHuman* player = PlayerHuman::for_gamepad(0);
		if (player)
		{
			PlayerHuman::UIMode mode = player->ui_mode();
			if (mode == PlayerHuman::UIMode::PvpSelectSpawn || mode == PlayerHuman::UIMode::PvpGameOver)
				return true;
		}
	}
	return false;
#endif
}

void UI::draw(const RenderParams& p)
{
	if (p.camera->gamepad == 0 && cursor_active())
	{
		mesh(p, Asset::Mesh::icon_cursor, cursor_pos + Vec2(-2, 4), Vec2(24 * UI::scale), UI::color_background);
		mesh(p, Asset::Mesh::icon_cursor, cursor_pos, Vec2(18 * UI::scale), UI::color_default);
	}

#if DEBUG
	for (s32 i = 0; i < debugs.length; i++)
	{
		Vec2 projected;
		if (project(p, debugs[i], &projected))
			centered_box(p, { projected, Vec2(4, 4) * scale });
	}
	debugs.length = 0;
#endif

	// draw sprites
	for (s32 i = 0; i < texture_blits.length; i++)
	{
		const TextureBlit& tb = texture_blits[i];
		Vec2 screen = p.camera->viewport.size * 0.5f;
		Vec2 scale = Vec2(1.0f / screen.x, 1.0f / screen.y);
		Vec2 scaled_pos = (tb.rect.pos - screen) * scale;

		const Vec2 corners[4] =
		{
			Vec2(tb.rect.size.x * (1.0f - tb.anchor.x), tb.rect.size.y * (1.0f - tb.anchor.y)),
			Vec2(tb.rect.size.x * -tb.anchor.x, tb.rect.size.y * (1.0f - tb.anchor.y)),
			Vec2(tb.rect.size.x * (1.0f - tb.anchor.x), tb.rect.size.y * -tb.anchor.y),
			Vec2(tb.rect.size.x * -tb.anchor.x, tb.rect.size.y * -tb.anchor.y),
		};

		r32 cs = cosf(tb.rotation), sn = sinf(tb.rotation);
		const Vec3 vertices[4] =
		{
			Vec3(scaled_pos.x + (corners[0].x * cs - corners[0].y * sn) * scale.x, scaled_pos.y + (corners[0].x * sn + corners[0].y * cs) * scale.y, 0),
			Vec3(scaled_pos.x + (corners[1].x * cs - corners[1].y * sn) * scale.x, scaled_pos.y + (corners[1].x * sn + corners[1].y * cs) * scale.y, 0),
			Vec3(scaled_pos.x + (corners[2].x * cs - corners[2].y * sn) * scale.x, scaled_pos.y + (corners[2].x * sn + corners[2].y * cs) * scale.y, 0),
			Vec3(scaled_pos.x + (corners[3].x * cs - corners[3].y * sn) * scale.x, scaled_pos.y + (corners[3].x * sn + corners[3].y * cs) * scale.y, 0),
		};

		const Vec4 colors[] =
		{
			tb.color,
			tb.color,
			tb.color,
			tb.color,
		};

		const Vec2 uvs[] =
		{
			Vec2(tb.uv.pos.x + tb.uv.size.x, tb.uv.pos.y),
			Vec2(tb.uv.pos.x, tb.uv.pos.y),
			Vec2(tb.uv.pos.x + tb.uv.size.x, tb.uv.pos.y + tb.uv.size.y),
			Vec2(tb.uv.pos.x, tb.uv.pos.y + tb.uv.size.y),
		};

		p.sync->write(RenderOp::UpdateAttribBuffers);
		p.sync->write(texture_mesh_id);
		p.sync->write<s32>(4);
		p.sync->write(vertices, 4);
		p.sync->write(colors, 4);
		p.sync->write(uvs, 4);

		p.sync->write(RenderOp::Shader);
		p.sync->write(tb.shader == AssetNull ? Asset::Shader::ui_texture : tb.shader);
		p.sync->write(p.technique);

		p.sync->write(RenderOp::Uniform);
		p.sync->write(Asset::Uniform::color_buffer);
		p.sync->write(RenderDataType::Texture);
		p.sync->write<s32>(1);
		p.sync->write<RenderTextureType>(RenderTextureType::Texture2D);
		p.sync->write<AssetID>(tb.texture);

		p.sync->write(RenderOp::Mesh);
		p.sync->write(RenderPrimitiveMode::Triangles);
		p.sync->write(texture_mesh_id);
	}
	texture_blits.length = 0;

	if (indices.length > 0)
	{
		p.sync->write(RenderOp::UpdateAttribBuffers);
		p.sync->write(mesh_id);
		p.sync->write<s32>(vertices.length);
		p.sync->write(vertices.data, vertices.length);
		p.sync->write(colors.data, colors.length);

		p.sync->write(RenderOp::UpdateIndexBuffer);
		p.sync->write(mesh_id);
		p.sync->write<s32>(indices.length);
		p.sync->write(indices.data, indices.length);

		p.sync->write(RenderOp::Shader);
		p.sync->write(Asset::Shader::ui);
		p.sync->write(p.technique);

		p.sync->write(RenderOp::Mesh);
		p.sync->write(RenderPrimitiveMode::Triangles);
		p.sync->write(mesh_id);

		vertices.length = 0;
		colors.length = 0;
		indices.length = 0;
	}
}

// instantly draw a texture
void UI::texture(const RenderParams& p, s32 texture, const Rect2& r, const Vec4& color, const Rect2& uv, const AssetID shader)
{
	Vec2 screen = p.camera->viewport.size * 0.5f;
	Vec2 scale = Vec2(1.0f / screen.x, 1.0f / screen.y);
	Vec2 scaled_pos = (r.pos - screen) * scale;
	Vec2 scaled_size = r.size * scale;

	Vec3 vertices[] =
	{
		Vec3(scaled_pos.x, scaled_pos.y, 0),
		Vec3(scaled_pos.x + scaled_size.x, scaled_pos.y, 0),
		Vec3(scaled_pos.x, scaled_pos.y + scaled_size.y, 0),
		Vec3(scaled_pos.x + scaled_size.x, scaled_pos.y + scaled_size.y, 0),
	};

	Vec4 colors[] =
	{
		color,
		color,
		color,
		color,
	};

	Vec2 uvs[] =
	{
		Vec2(uv.pos.x, uv.pos.y),
		Vec2(uv.pos.x + uv.size.x, uv.pos.y),
		Vec2(uv.pos.x, uv.pos.y + uv.size.y),
		Vec2(uv.pos.x + uv.size.x, uv.pos.y + uv.size.y),
	};

	p.sync->write(RenderOp::UpdateAttribBuffers);
	p.sync->write(texture_mesh_id);
	p.sync->write<s32>(4);
	p.sync->write(vertices, 4);
	p.sync->write(colors, 4);
	p.sync->write(uvs, 4);

	p.sync->write(RenderOp::Shader);
	p.sync->write(shader == AssetNull ? Asset::Shader::ui_texture : shader);
	p.sync->write(p.technique);

	p.sync->write(RenderOp::Uniform);
	p.sync->write(Asset::Uniform::color_buffer);
	p.sync->write(RenderDataType::Texture);
	p.sync->write<s32>(1);
	p.sync->write<RenderTextureType>(RenderTextureType::Texture2D);
	p.sync->write<AssetID>(texture);

	p.sync->write(RenderOp::Mesh);
	p.sync->write(RenderPrimitiveMode::Triangles);
	p.sync->write(texture_mesh_id);
}

// Cue up a sprite to be rendered later
void UI::sprite(const RenderParams& p, s32 texture, const Rect2& r, const Vec4& color, const Rect2& uv, r32 rot, const Vec2& anchor, AssetID shader)
{
	Loader::texture(texture, RenderTextureWrap::Clamp);
	if (r.size.x > 0 && r.size.y > 0 && color.w > 0)
	{
		TextureBlit* tb = texture_blits.add();
		tb->anchor = anchor;
		tb->color = color;
		tb->texture = texture;
		tb->shader = shader;
		tb->uv = uv;
		tb->rect = r;
		tb->rotation = rot;
	}
}

b8 UI::flash_function_slow(r32 time)
{
	return flash_function_slow(time, 4.0f);
}

b8 UI::flash_function_slow(r32 time, r32 speed)
{
	return b8(s32(time * speed) % 4);
}

b8 UI::flash_function(r32 time)
{
	return flash_function(time, 16.0f);
}

b8 UI::flash_function(r32 time, r32 speed)
{
	return b8(s32(time * speed) % 2);
}

// projects a 3D point into screen space, limiting it to stay onscreen
// returns true if the point is in front of the camera
b8 UI::is_onscreen(const RenderParams& params, const Vec3& pos, Vec2* out, Vec2* dir)
{
	b8 on_screen = project(params, pos, out);

	const Rect2& viewport = params.camera->viewport;

	Vec2 center = viewport.size * 0.5f;
	Vec2 offset = *out - center;
	if (!on_screen)
		offset *= -1.0f;

	r32 radius = vi_min(viewport.size.x, viewport.size.y) * 0.5f - (64.0f * UI::scale);

	r32 offset_length = offset.length();
	if ((offset_length > radius || (offset_length > 0.0f && !on_screen)))
	{
		offset *= radius / offset_length;
		*out = center + offset;
		if (dir)
			*dir = offset;
		return false;
	}

	if (dir)
		*dir = offset;
	return on_screen;
}

Vec2 UI::indicator(const RenderParams& params, const Vec3& pos, const Vec4& color, b8 offscreen, r32 scale, r32 rotation)
{
	Vec2 p;
	if (offscreen)
	{
		// if the target is offscreen, point toward it
		Vec2 offset;
		if (is_onscreen(params, pos, &p, &offset))
			triangle_border(params, { p, Vec2(28 * scale * UI::scale) }, 6 * scale, color, rotation);
		else
			triangle(params, { p, Vec2(24 * UI::scale * scale) }, color, atan2f(offset.y, offset.x) + PI * -0.5f);
	}
	else
	{
		if (project(params, pos, &p))
			triangle_border(params, { p, Vec2(28 * UI::scale * scale) }, 6 * scale, color, rotation);
	}
	return p;
}

#if DEBUG
Array<Vec3> UI::debugs;
void UI::debug(const Vec3& pos)
{
	debugs.add(pos);
}
#endif

}
