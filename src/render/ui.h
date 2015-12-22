#pragma once

#include "types.h"
#include "lmath.h"
#include "data/array.h"

namespace VI
{

struct RenderParams;

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
	char string[512];
	char rendered_string[512];
	Array<int> indices;
	Array<Vec3> vertices;
	Vec4 color;
	AssetID font;
	float size;
	float wrap_width;
	Anchor anchor_x;
	Anchor anchor_y;
	int clip_char;
	int clip_vertex;
	int clip_index;

	Vec2 normalized_bounds;
	Vec2 bounds() const;
	Rect2 rect(const Vec2&) const;
	void text(const char*);
	void reeval();
	void refresh_vertices();
	void set_size(float);
	void wrap(float);
	bool clipped() const;
	void clip(int);
	static void reeval_all();
	void draw(const RenderParams&, const Vec2& pos, const float = 0.0f) const;
	UIText();
	~UIText();
};

struct LoopSync;

struct UI
{
	static const Vec4 default_color;
	static const Vec4 alert_color;
	static const Vec4 subtle_color;
	static float scale;
	static int mesh_id;
	static int texture_mesh_id;
	static Array<Vec3> vertices;
	static Array<Vec4> colors;
	static Array<int> indices;
	static void init(LoopSync*);
	static float get_scale(const int, const int);
	static void box(const RenderParams&, const Rect2&, const Vec4& = Vec4(1, 1, 1, 1));
	static void centered_box(const RenderParams&, const Rect2&, const Vec4& = Vec4(1, 1, 1, 1), const float = 0.0f);
	static void border(const RenderParams&, const Rect2&, const float, const Vec4& = Vec4(1, 1, 1, 1));
	static void centered_border(const RenderParams&, const Rect2&, const float, const Vec4& = Vec4(1, 1, 1, 1), const float = 0.0f);
	static void triangle(const RenderParams&, const Rect2&, const Vec4& = Vec4(1, 1, 1, 1), const float = 0.0f);
	static void update(const RenderParams&);
	static void draw(const RenderParams&);
	static void mesh(const RenderParams&, const AssetID, const Vec2&, const Vec2& = Vec2(1, 1), const Vec4& = Vec4(1, 1, 1, 1), const float = 0.0f);
	static bool project(const RenderParams&, const Vec3&, Vec2*);
	static void texture(const RenderParams&, const int, const Rect2&, const Vec4& = Vec4(1, 1, 1, 1), const Rect2& = { Vec2::zero, Vec2(1, 1) }, const AssetID = AssetNull);
#if DEBUG
	static Array<Vec3> debugs;
	static void debug(const Vec3&);
#endif
};

}
