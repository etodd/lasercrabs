#include "ui.h"
#include "asset/shader.h"
#include "load.h"
#include "render/render.h"
#include "utf8/utf8.h"
#include "strings.h"

namespace VI
{

UIText::UIText()
	: color(UI::default_color),
	font(),
	size(16),
	rendered_string(),
	normalized_bounds(),
	anchor_x(),
	anchor_y(),
	clip(),
	wrap_width()
{
}

Array<UIText::VariableEntry> UIText::variables;

void UIText::set_variable(const char* name, const char* value)
{
	vi_assert(strlen(name) < 255 && strlen(value) < 255);
	b8 found = false;
	for (s32 i = 0; i < variables.length; i++)
	{
		if (utf8cmp(variables[i].name, name) == 0)
		{
			utf8cpy(variables[i].value, value);
			found = true;
			break;
		}
	}
	if (!found)
	{
		VariableEntry* entry = variables.add();
		utf8cpy(entry->name, name);
		utf8cpy(entry->value, value);
	}
}

void UIText::text(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	char string[512];

	if (!format)
		format = "";

#if defined(_WIN32)
	vsprintf_s(string, 512, format, args);
#else
	vsnprintf(string, 512, format, args);
#endif

	va_end(args);

	text_raw(string);
}

void UIText::text_raw(const char* string)
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

			if (c == '{' && string[char_index + 1] == '{')
			{
				const char* start = &string[char_index + 2];
				const char* end = start;
				while (*end != '}' || *(end + 1) != '}')
					end = strchr(end + 1, '}');

				for (s32 i = 0; i < variables.length; i++)
				{
					if (strncmp(variables[i].name, start, end - start) == 0)
					{
						variable = variables[i].value;

						c = *variable;
						// set up char_index to resume at the end of the variable name once we're done with it
						char_index = end + 2 - string;
						break;
					}
				}
			}

			if (!c)
				break;

			// Store the final result in rendered_string
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
	Font* f = Loader::font(font);
	normalized_bounds = Vec2::zero;
	Vec3 pos(0, -1.0f, 0);
	s32 char_index = 0;
	char c;
	const Vec2 spacing = Vec2(0.075f, 0.3f);
	r32 wrap = wrap_width / (size * UI::scale);
	while ((c = rendered_string[char_index]))
	{
		Font::Character* character = &f->get(&c);
		if (c == '\n')
		{
			pos.x = 0.0f;
			pos.y -= 1.0f + spacing.y;
		}
		else if (wrap > 0.0f && (c == ' ' || c == '\t'))
		{
			// Check if we need to put the next word on the next line

			r32 end_of_next_word = pos.x + spacing.x + character->max.x;
			s32 word_index = char_index + 1;
			char word_char;
			while (true)
			{
				word_char = rendered_string[word_index];
				if (!word_char || word_char == ' ' || word_char == '\t' || word_char == '\n')
					break;
				end_of_next_word += spacing.x + f->get(&word_char).max.x;
				word_index++;
			}

			if (end_of_next_word > wrap)
			{
				// New line
				pos.x = 0.0f;
				pos.y -= 1.0f + spacing.y;
			}
			else
			{
				// Just a regular whitespace character
				pos.x += spacing.x + character->max.x;
			}
		}
		else
		{
			if (character->code == c)
				pos.x += spacing.x + character->max.x;
			else
			{
				// Font is missing character
			}
		}

		normalized_bounds.x = vi_max(normalized_bounds.x, pos.x);

		char_index++;
	}

	normalized_bounds.y = -pos.y;
}

b8 UIText::clipped() const
{
	return clip > 0.0f && clip < utf8len(rendered_string);
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

	Font* f = Loader::font(font);
	s32 vertex_index = UI::vertices.length;
	s32 index_index = UI::indices.length;
	Vec3 p(0, -1.0f, 0);
	s32 char_index = 0;
	char c;
	const Vec2 spacing = Vec2(0.075f, 0.3f);
	r32 scaled_size = size * UI::scale;
	r32 wrap = wrap_width / scaled_size;
	while ((clip == 0 || char_index <= clip) && (c = rendered_string[char_index]))
	{
		Font::Character* character = &f->get(&c);
		if (c == '\n')
		{
			p.x = 0.0f;
			p.y -= 1.0f + spacing.y;
		}
		else if (wrap > 0.0f && (c == ' ' || c == '\t'))
		{
			// Check if we need to put the next word on the next line

			r32 end_of_next_word = p.x + spacing.x + character->max.x;
			s32 word_index = char_index + 1;
			char word_char;
			while (true)
			{
				word_char = rendered_string[word_index];
				if (!word_char || word_char == ' ' || word_char == '\t' || word_char == '\n')
					break;
				end_of_next_word += spacing.x + f->get(&word_char).max.x;
				word_index++;
			}

			if (end_of_next_word > wrap)
			{
				// New line
				p.x = 0.0f;
				p.y -= 1.0f + spacing.y;
			}
			else
			{
				// Just a regular whitespace character
				p.x += spacing.x + character->max.x;
			}
		}
		else
		{
			if (character->code == c)
			{
				UI::vertices.resize(vertex_index + character->vertex_count);
				UI::colors.resize(UI::vertices.length);
				for (s32 i = 0; i < character->vertex_count; i++)
				{
					Vec3 v = f->vertices[character->vertex_start + i] + p;
					Vec3 vertex;
					vertex.x = (offset.x + scaled_size * (v.x * cs - v.y * sn)) * scale.x;
					vertex.y = (offset.y + scaled_size * (v.x * sn + v.y * cs)) * scale.y;
					UI::vertices[vertex_index + i] = vertex;
					UI::colors[vertex_index + i] = color;
				}

				p.x += spacing.x + character->max.x;

				UI::indices.resize(index_index + character->index_count);
				for (s32 i = 0; i < character->index_count; i++)
					UI::indices[index_index + i] = vertex_index + f->indices[character->index_start + i] - character->vertex_start;
			}
			else
			{
				// Font is missing character
			}
			vertex_index = UI::vertices.length;
			index_index = UI::indices.length;
		}

		char_index++;
	}
}

const Vec4 UI::default_color = Vec4(1, 1, 1, 1);
const Vec4 UI::alert_color = Vec4(1.0f, 0.5f, 0.5f, 1);
const Vec4 UI::accent_color = Vec4(1.0f, 0.9f, 0.4f, 1);
const Vec4 UI::background_color = Vec4(0, 0, 0, 1);
const Vec4 UI::disabled_color = Vec4(0.5f, 0.5f, 0.5f, 1);
r32 UI::scale = 1.0f;
s32 UI::mesh_id = AssetNull;
s32 UI::texture_mesh_id = AssetNull;
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

void UI::triangle_border(const RenderParams& params, const Rect2& r, r32 thickness, const Vec4& color, r32 rot)
{
	if (r.size.x > 0 && r.size.y > 0 && color.w > 0)
	{
		s32 vertex_start = UI::vertices.length;
		const Vec2 screen = params.camera->viewport.size * 0.5f;
		const Vec2 scale = Vec2(1.0f / screen.x, 1.0f / screen.y);
		const Vec2 scaled_pos = (r.pos - screen) * scale;
		r32 scaled_thickness = thickness * UI::scale * 2;

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

		Mesh* m = Loader::mesh(mesh);
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
	Vec4 projected = p.view_projection * Vec4(v.x, v.y, v.z, 1);
	Vec2 screen = p.camera->viewport.size * 0.5f;
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

	scale = get_scale(sync->input.width, sync->input.height);
}

r32 UI::get_scale(s32 width, s32 height)
{
	s32 area = width * height;
	if (area > 1920 * 1080)
		return 1.5f;
	else
		return 1.0f;
}

void UI::update(const RenderParams& p)
{
	scale = get_scale(p.camera->viewport.size.x, p.camera->viewport.size.y);
}

void UI::draw(const RenderParams& p)
{
#if DEBUG
	for (s32 i = 0; i < debugs.length; i++)
	{
		Vec2 projected;
		if (project(p, debugs[i], &projected))
			centered_box(p, { projected, Vec2(4, 4) * scale });
	}
	debugs.length = 0;
#endif
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

	// Draw sprites
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
			Vec2(tb.uv.pos.x, tb.uv.pos.y),
			Vec2(tb.uv.pos.x + tb.uv.size.x, tb.uv.pos.y),
			Vec2(tb.uv.pos.x, tb.uv.pos.y + tb.uv.size.y),
			Vec2(tb.uv.pos.x + tb.uv.size.x, tb.uv.pos.y + tb.uv.size.y),
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
}

// Instantly draw a texture
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
	Loader::texture(texture);
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

#if DEBUG
Array<Vec3> UI::debugs;
void UI::debug(const Vec3& pos)
{
	debugs.add(pos);
}
#endif

}