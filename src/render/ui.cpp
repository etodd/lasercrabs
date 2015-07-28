#include "ui.h"
#include "asset.h"
#include "load.h"
#include "render/render.h"

namespace VI
{

UIText::UIText()
	: indices(),
	vertices(),
	color(Vec4(1, 1, 1, 1)),
	font(),
	string(),
	transform(Mat4::identity)
{
}

void UIText::text(const char* string)
{
	vertices.length = 0;
	indices.length = 0;

	int char_index = 0;
	int vertex_index = 0;
	int index_index = 0;
	Vec3 pos(0, 0, 0);
	while (true)
	{
		char c = string[char_index];
		if (!c)
			break;
		Font::Character* character = &font->characters[c];
		if (character->code == c)
		{
			vertices.resize(vertex_index + character->vertex_count);
			for (int i = 0; i < character->vertex_count; i++)
				vertices[vertex_index + i] = font->vertices[character->vertex_start + i] + pos;

			pos += Vec3(character->max.x - character->min.x, 0, 0);

			indices.resize(index_index + character->index_count);
			for (int i = 0; i < character->index_count; i++)
				indices[index_index + i] = vertex_index + font->indices[character->index_start + i] - character->vertex_start;
		}
		else
		{
			// Font is missing character
		}
		vertex_index = vertices.length;
		index_index = indices.length;
		char_index++;
	}
}

void UIText::draw(const Vec3& pos)
{
	int vertex_start = UI::vertices.length;
	for (int i = 0; i < vertices.length; i++)
		UI::vertices.add(vertices[i] + pos);

	for (int i = 0; i < vertices.length; i++)
		UI::colors.add(color);

	UI::indices.reserve(UI::indices.length + indices.length);
	for (int i = 0; i < indices.length; i++)
		UI::indices.add(indices[i] + vertex_start);

	/*
	UI::vertices.add(pos + Vec3(1, 0, 0));
	UI::vertices.add(pos + Vec3(0, 1, 0));
	UI::vertices.add(pos + Vec3(0, 0, 0));
	UI::colors.add(Vec4(1, 1, 1, 1));
	UI::colors.add(Vec4(1, 1, 1, 1));
	UI::colors.add(Vec4(1, 1, 1, 1));
	UI::indices.add(vertex_start + 0);
	UI::indices.add(vertex_start + 1);
	UI::indices.add(vertex_start + 2);
	*/
}

size_t UI::mesh = Asset::Nothing;
Array<Vec3> UI::vertices = Array<Vec3>();
Array<Vec4> UI::colors = Array<Vec4>();
Array<int> UI::indices = Array<int>();

void UI::draw(const RenderParams& p)
{
	if (indices.length > 0)
	{
		if (mesh == Asset::Nothing)
		{
			mesh = Loader::dynamic_mesh_permanent(2);
			Loader::dynamic_mesh_attrib(RenderDataType_Vec3);
			Loader::dynamic_mesh_attrib(RenderDataType_Vec4);
			Loader::shader_permanent(Asset::Shader::UI);
		}

		p.sync->write(RenderOp_UpdateAttribBuffers);
		p.sync->write(mesh);
		p.sync->write<int>(vertices.length);
		p.sync->write(vertices.data, vertices.length);
		p.sync->write(colors.data, colors.length);

		p.sync->write(RenderOp_UpdateIndexBuffer);
		p.sync->write(mesh);
		p.sync->write<int>(indices.length);
		p.sync->write(indices.data, indices.length);

		p.sync->write(RenderOp_Mesh);
		p.sync->write(mesh);
		p.sync->write(Asset::Shader::UI);

		Mat4 mvp = p.view_projection;

		p.sync->write<int>(3); // Uniform count

		p.sync->write(Asset::Uniform::MVP);
		p.sync->write(RenderDataType_Mat4);
		p.sync->write<int>(1);
		p.sync->write(&mvp);

		p.sync->write(Asset::Uniform::M);
		p.sync->write(RenderDataType_Mat4);
		p.sync->write<int>(1);
		p.sync->write(Mat4::identity);

		p.sync->write(Asset::Uniform::V);
		p.sync->write(RenderDataType_Mat4);
		p.sync->write<int>(1);
		p.sync->write(&p.view);

		vertices.length = 0;
		colors.length = 0;
		indices.length = 0;
	}
}

}
