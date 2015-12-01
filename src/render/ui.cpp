#include "ui.h"
#include "asset/shader.h"
#include "load.h"
#include "render/render.h"

namespace VI
{

Array<UIText*> UIText::instances = Array<UIText*>();

UIText::UIText()
	: indices(),
	vertices(),
	color(Vec4(1, 1, 1, 1)),
	font(),
	size(16),
	string(),
	normalized_bounds(),
	anchor_x(),
	anchor_y()
{
	instances.add(this);
}

UIText::~UIText()
{
	for (int i = 0; i < instances.length; i++)
	{
		if (instances[i] == this)
		{
			instances.remove(i);
			break;
		}
	}
}

void UIText::reeval()
{
	if (string)
		text(string);
}

void UIText::reeval_all()
{
	for (int i = 0; i < instances.length; i++)
		instances[i]->reeval();
}

Array<UIText::VariableEntry> UIText::variables = Array<UIText::VariableEntry>();

void UIText::set_variable(const char* name, const char* value)
{
	vi_assert(strlen(name) < 255 && strlen(value) < 255);
	bool found = false;
	for (int i = 0; i < variables.length; i++)
	{
		if (strcmp(variables[i].name, name) == 0)
		{
			strcpy(variables[i].value, value);
			found = true;
			break;
		}
	}
	if (!found)
	{
		VariableEntry* entry = variables.add();
		strcpy(entry->name, name);
		strcpy(entry->value, value);
	}
}

void UIText::text(const char* s)
{
	{
		strncpy(string, s, 512);
		int char_index = 0;
		int rendered_index = 0;

		const char* variable = 0;
		while (true)
		{
			char c = variable && *variable ? *variable : string[char_index];

			if (c == '{' && string[char_index + 1] == '{')
			{
				char* start = &string[char_index + 2];
				char* end = start;
				while (*end != '}' || *(end + 1) != '}')
					end = strchr(end + 1, '}');

				for (int i = 0; i < variables.length; i++)
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

	refresh_vertices();

	if (clip_char > 0) // clip is active; make sure to update it
		clip(clip_char);
}

void UIText::refresh_vertices()
{
	Font* f = Loader::font(font);
	normalized_bounds = Vec2::zero;
	vertices.length = 0;
	indices.length = 0;
	int vertex_index = 0;
	int index_index = 0;
	Vec3 pos(0, 0, 0);
	int char_index = 0;
	char c;
	const float spacing = 0.075f;
	float wrap = wrap_width / size;
	while ((c = rendered_string[char_index]))
	{
		Font::Character* character = &f->characters[c];
		if (c == '\n')
		{
			pos.x = 0.0f;
			pos.y -= 1.0f + spacing;
		}
		else if (wrap > 0.0f && (c == ' ' || c == '\t'))
		{
			// Check if we need to put the next word on the next line

			float end_of_next_word = pos.x + spacing + character->max.x;
			int word_index = char_index + 1;
			char word_char;
			while (true)
			{
				word_char = rendered_string[word_index];
				if (!word_char || word_char == ' ' || word_char == '\t' || word_char == '\n')
					break;
				end_of_next_word += spacing + f->characters[word_char].max.x;
				word_index++;
			}

			if (end_of_next_word > wrap)
			{
				// New line
				pos.x = 0.0f;
				pos.y -= 1.0f + spacing;
			}
			else
			{
				// Just a regular whitespace character
				pos.x += spacing + character->max.x;
			}
		}
		else
		{
			if (character->code == c)
			{
				vertices.resize(vertex_index + character->vertex_count);
				for (int i = 0; i < character->vertex_count; i++)
					vertices[vertex_index + i] = f->vertices[character->vertex_start + i] + pos;

				pos.x += spacing + character->max.x;

				indices.resize(index_index + character->index_count);
				for (int i = 0; i < character->index_count; i++)
					indices[index_index + i] = vertex_index + f->indices[character->index_start + i] - character->vertex_start;
			}
			else
			{
				// Font is missing character
			}
			vertex_index = vertices.length;
			index_index = indices.length;
		}

		normalized_bounds.x = fmax(normalized_bounds.x, pos.x);
		normalized_bounds.y = fmax(normalized_bounds.y, 1.0f - pos.y);

		char_index++;
	}
}

// Only render characters up to the specified index
void UIText::clip(int index)
{
	if (index == clip_char)
		return;

	clip_char = index;
	clip_vertex = 0;
	clip_index = 0;

	Font* f = Loader::font(font);
	for (int i = 0; i < index; i++)
	{
		char c = rendered_string[i];
		if (!c)
		{
			// clip is longer than the current string; ignore it
			clip_vertex = 0;
			clip_index = 0;
			break;
		}

		Font::Character* character = &f->characters[c];
		clip_vertex += character->vertex_count;
		clip_index += character->index_count;
	}
}

bool UIText::clipped() const
{
	return clip_index > 0;
}

void UIText::set_size(float s)
{
	size = s;
	refresh_vertices();
}

void UIText::wrap(float w)
{
	wrap_width = w;
	refresh_vertices();
}

Vec2 UIText::bounds() const
{
	Vec2 b = normalized_bounds * size * UI::scale;
	if (wrap_width > 0.0f)
		b.x = wrap_width;
	return b;
}

void UIText::draw(const RenderParams& params, const Vec2& pos, const float rot) const
{
	int vertex_start = UI::vertices.length;
	Vec2 screen = Vec2(params.camera->viewport.width * 0.5f, params.camera->viewport.height * 0.5f);
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
			break;
		case Anchor::Center:
			offset.y += (bound.y - size) * 0.5f;
			break;
		case Anchor::Max:
			offset.y += bound.y - size;
			break;
		default:
			vi_assert(false);
			break;
	}
	Vec2 scale = Vec2(1.0f / screen.x, 1.0f / screen.y);
	float cs = cosf(rot), sn = sinf(rot);

	int vertex_count = clip_vertex > 0 ? clip_vertex : vertices.length;
	UI::vertices.reserve(UI::vertices.length + vertex_count);
	for (int i = 0; i < vertex_count; i++)
	{
		Vec3 vertex;
		vertex.x = (offset.x + size * UI::scale * (vertices[i].x * cs - vertices[i].y * sn)) * scale.x;
		vertex.y = (offset.y + size * UI::scale * (vertices[i].x * sn + vertices[i].y * cs)) * scale.y;
		UI::vertices.add(vertex);
	}

	UI::colors.reserve(UI::colors.length + vertex_count);
	for (int i = 0; i < vertex_count; i++)
		UI::colors.add(color);

	int index_count = clip_index > 0 ? clip_index : indices.length;
	UI::indices.reserve(UI::indices.length + index_count);
	for (int i = 0; i < index_count; i++)
		UI::indices.add(indices[i] + vertex_start);
}

const Vec4 UI::default_color = Vec4(1, 1, 1, 1);
const Vec4 UI::alert_color = Vec4(1.0f, 0.2f, 0.2f, 1.0f);
const Vec4 UI::subtle_color = Vec4(1.0f, 1.0f, 1.0f, 0.75f);
float UI::scale = 1.0f;
int UI::mesh_id = AssetNull;
int UI::texture_mesh_id = AssetNull;
Array<Vec3> UI::vertices = Array<Vec3>();
Array<Vec4> UI::colors = Array<Vec4>();
Array<int> UI::indices = Array<int>();

void UI::box(const RenderParams& params, const Vec2& pos, const Vec2& size, const Vec4& color)
{
	if (size.x > 0 && size.y > 0 && color.w > 0)
	{
		int vertex_start = UI::vertices.length;
		Vec2 screen = Vec2(params.camera->viewport.width * 0.5f, params.camera->viewport.height * 0.5f);
		Vec2 scale = Vec2(1.0f / screen.x, 1.0f / screen.y);
		Vec2 scaled_pos = (pos - screen) * scale;
		Vec2 scaled_size = size * scale;

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

void UI::centered_box(const RenderParams& params, const Vec2& pos, const Vec2& size, const Vec4& color, float rot)
{
	if (size.x > 0 && size.y > 0 && color.w > 0)
	{
		int vertex_start = UI::vertices.length;
		Vec2 screen = Vec2(params.camera->viewport.width * 0.5f, params.camera->viewport.height * 0.5f);
		Vec2 scale = Vec2(1.0f / screen.x, 1.0f / screen.y);
		Vec2 scaled_pos = (pos - screen) * scale;

		Vec2 corners[4] =
		{
			Vec2(size.x * -0.5f, size.y * -0.5f),
			Vec2(size.x * 0.5f, size.y * -0.5f),
			Vec2(size.x * -0.5f, size.y * 0.5f),
			Vec2(size.x * 0.5f, size.y * 0.5f),
		};

		float cs = cosf(rot), sn = sinf(rot);
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

void UI::border(const RenderParams& params, const Vec2& pos, const Vec2& size, const float thickness, const Vec4& color)
{
	if (size.x > 0 && size.y > 0 && color.w > 0)
	{
		int vertex_start = UI::vertices.length;
		Vec2 screen = Vec2(params.camera->viewport.width * 0.5f, params.camera->viewport.height * 0.5f);
		Vec2 scale = Vec2(1.0f / screen.x, 1.0f / screen.y);
		Vec2 scaled_pos = (pos - screen) * scale;
		Vec2 scaled_size = size * scale;
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

void UI::centered_border(const RenderParams& params, const Vec2& pos, const Vec2& size, const float thickness, const Vec4& color, const float rot)
{
	if (size.x > 0 && size.y > 0 && color.w > 0)
	{
		int vertex_start = UI::vertices.length;
		Vec2 screen = Vec2(params.camera->viewport.width * 0.5f, params.camera->viewport.height * 0.5f);
		Vec2 scale = Vec2(1.0f / screen.x, 1.0f / screen.y);
		Vec2 scaled_pos = (pos - screen) * scale;

		Vec2 corners[8] =
		{
			Vec2(size.x * -0.5f, size.y * -0.5f),
			Vec2(size.x * 0.5f, size.y * -0.5f),
			Vec2(size.x * -0.5f, size.y * 0.5f),
			Vec2(size.x * 0.5f, size.y * 0.5f),
			Vec2(size.x * -0.5f - thickness, size.y * -0.5f - thickness),
			Vec2(size.x * 0.5f + thickness, size.y * -0.5f - thickness),
			Vec2(size.x * -0.5f - thickness, size.y * 0.5f + thickness),
			Vec2(size.x * 0.5f + thickness, size.y * 0.5f + thickness),
		};

		float cs = cosf(rot), sn = sinf(rot);
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

void UI::triangle(const RenderParams& params, const Vec2& pos, const Vec2& size, const Vec4& color, float rot)
{
	if (size.x > 0 && size.y > 0 && color.w > 0)
	{
		int vertex_start = UI::vertices.length;
		Vec2 screen = Vec2(params.camera->viewport.width * 0.5f, params.camera->viewport.height * 0.5f);
		Vec2 scale = Vec2(1.0f / screen.x, 1.0f / screen.y);
		Vec2 scaled_pos = (pos - screen) * scale;

		const float ratio = 0.8660254037844386f;
		Vec2 corners[3] =
		{
			Vec2(size.x * 0.5f * ratio, size.y * -0.25f),
			Vec2(0, size.y * 0.5f),
			Vec2(size.x * -0.5f * ratio, size.y * -0.25f),
		};

		float cs = cosf(rot), sn = sinf(rot);
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

void UI::mesh(const RenderParams& params, const AssetID mesh, const Vec2& pos, const Vec2& size, const Vec4& color, const float rot)
{
	if (size.x > 0 && size.y > 0 && color.w > 0)
	{
		int vertex_start = UI::vertices.length;
		Vec2 screen = Vec2(params.camera->viewport.width * 0.5f, params.camera->viewport.height * 0.5f);
		Vec2 scale = Vec2(1.0f / screen.x, 1.0f / screen.y);
		Vec2 scaled_pos = (pos - screen) * scale;
		scale *= size;

		float cs = cosf(rot), sn = sinf(rot);

		Mesh* m = Loader::mesh(mesh);
		for (int i = 0; i < m->vertices.length; i++)
		{
			UI::vertices.add(Vec3(scaled_pos.x + (m->vertices[i].x * cs - m->vertices[i].y * sn) * scale.x, scaled_pos.y + (m->vertices[i].x * sn + m->vertices[i].y * cs) * scale.y, 0));
			UI::colors.add(color);
		}
		for (int i = 0; i < m->indices.length; i++)
			UI::indices.add(vertex_start + m->indices[i]);
	}
}

bool UI::project(const RenderParams& p, const Vec3& v, Vec2& out)
{
	Vec4 projected = p.view_projection * Vec4(v.x, v.y, v.z, 1);
	Vec2 screen = Vec2(p.camera->viewport.width * 0.5f, p.camera->viewport.height * 0.5f);
	out = Vec2((projected.x / projected.w + 1.0f) * screen.x, (projected.y / projected.w + 1.0f) * screen.y);
	return projected.z > -projected.w && projected.z < projected.w;
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

	int indices[] =
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
	sync->write<int>(6);
	sync->write(indices, 6);

	scale = get_scale(sync->input.width, sync->input.height);
}

float UI::get_scale(const int width, const int height)
{
	int area = width * height;
	if (area >= 1680 * 1050)
		return 1.5f;
	else
		return 1.0f;
}

void UI::update(const RenderParams& p)
{
	scale = get_scale(p.camera->viewport.width, p.camera->viewport.height);
}

void UI::draw(const RenderParams& p)
{
	if (indices.length > 0)
	{
		p.sync->write(RenderOp::UpdateAttribBuffers);
		p.sync->write(mesh_id);
		p.sync->write<int>(vertices.length);
		p.sync->write(vertices.data, vertices.length);
		p.sync->write(colors.data, colors.length);

		p.sync->write(RenderOp::UpdateIndexBuffer);
		p.sync->write(mesh_id);
		p.sync->write<int>(indices.length);
		p.sync->write(indices.data, indices.length);

		p.sync->write(RenderOp::Shader);
		p.sync->write(Asset::Shader::ui);
		p.sync->write(p.technique);

		p.sync->write(RenderOp::Mesh);
		p.sync->write(mesh_id);

		vertices.length = 0;
		colors.length = 0;
		indices.length = 0;
	}
}

void UI::texture(const RenderParams& p, const int texture, const Vec2& pos, const Vec2& size, const Vec4& color, const Vec2& uva, const Vec2& uvb, const AssetID shader)
{
	Vec2 screen = Vec2(p.camera->viewport.width * 0.5f, p.camera->viewport.height * 0.5f);
	Vec2 scale = Vec2(1.0f / screen.x, 1.0f / screen.y);
	Vec2 scaled_pos = (pos - screen) * scale;
	Vec2 scaled_size = size * scale;

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
		Vec2(uva.x, uva.y),
		Vec2(uvb.x, uva.y),
		Vec2(uva.x, uvb.y),
		Vec2(uvb.x, uvb.y),
	};

	p.sync->write(RenderOp::UpdateAttribBuffers);
	p.sync->write(texture_mesh_id);
	p.sync->write<int>(4);
	p.sync->write(vertices, 4);
	p.sync->write(colors, 4);
	p.sync->write(uvs, 4);

	p.sync->write(RenderOp::Shader);
	p.sync->write(shader == AssetNull ? Asset::Shader::ui_texture : shader);
	p.sync->write(p.technique);

	p.sync->write(RenderOp::Uniform);
	p.sync->write(Asset::Uniform::color_buffer);
	p.sync->write(RenderDataType::Texture);
	p.sync->write<int>(1);
	p.sync->write<RenderTextureType>(RenderTextureType::Texture2D);
	p.sync->write<AssetID>(texture);

	p.sync->write(RenderOp::Mesh);
	p.sync->write(texture_mesh_id);
}

}
