#pragma once

#include "types.h"
#include "lmath.h"
#include "data/array.h"
#include "render.h"

namespace VI
{

struct UIText
{
	struct VariableEntry
	{
		char name[255];
		char value[255];
	};

	static Array<VariableEntry> variables;

	static void set_variable(const char*, const char*);

	enum class Anchor
	{
		Min,
		Center,
		Max,
	};
	static Array<UIText*> instances;
	char* string;
	Array<int> indices;
	Array<Vec3> vertices;
	Vec4 color;
	AssetID font;
	float size;
	Anchor anchor_x;
	Anchor anchor_y;

	Vec2 normalized_bounds;
	Vec2 bounds() const;
	void text(const char*);
	void reeval();
	static void reeval_all();
	void draw(const RenderParams&, const Vec2& pos, const float = 0.0f) const;
	UIText();
	~UIText();
};

struct UI
{
	static const Vec4 default_color;
	static float scale;
	static int mesh_id;
	static int texture_mesh_id;
	static Array<Vec3> vertices;
	static Array<Vec4> colors;
	static Array<int> indices;
	static void init(RenderSync*);
	static float get_scale(const int, const int);
	static void box(const RenderParams&, const Vec2&, const Vec2&, const Vec4& = Vec4(1, 1, 1, 1));
	static void centered_box(const RenderParams&, const Vec2&, const Vec2&, const Vec4& = Vec4(1, 1, 1, 1), const float = 0.0f);
	static void border(const RenderParams&, const Vec2&, const Vec2&, const float, const Vec4& = Vec4(1, 1, 1, 1));
	static void centered_border(const RenderParams&, const Vec2&, const Vec2&, const float, const Vec4& = Vec4(1, 1, 1, 1), const float = 0.0f);
	static void triangle(const RenderParams&, const Vec2&, const Vec2&, const Vec4& = Vec4(1, 1, 1, 1), const float = 0.0f);
	static void update(const RenderParams&);
	static void draw(const RenderParams&);
	static void mesh(const RenderParams&, const AssetID, const Vec2&, const Vec2& = Vec2(1, 1), const Vec4& = Vec4(1, 1, 1, 1), const float = 0.0f);
	static bool project(const RenderParams&, const Vec3&, Vec2&);
	static void texture(const RenderParams&, const int, const Vec2&, const Vec2&, const Vec4& = Vec4(1, 1, 1, 1), const Vec2& = Vec2::zero, const Vec2& = Vec2::zero);
};

}
