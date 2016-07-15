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
	char rendered_string[1024];
	Vec4 color;
	AssetID font;
	r32 size;
	r32 wrap_width;
	Anchor anchor_x;
	Anchor anchor_y;
	s32 clip;

	Vec2 normalized_bounds;
	Vec2 bounds() const;
	Rect2 rect(const Vec2&) const;
	void text(const char*, ...);
	void text_raw(const char*);
	void refresh_bounds();
	void set_size(r32);
	void wrap(r32);
	b8 clipped() const;
	b8 has_text() const;
	void draw(const RenderParams&, const Vec2&, r32 = 0.0f) const;
	UIText();
};

struct LoopSync;

struct UI
{
	struct TextureBlit
	{
		s32 texture;
		Rect2 rect;
		Vec4 color;
		Rect2 uv;
		AssetID shader;
		r32 rotation;
		Vec2 anchor;
	};

	static const Vec4 default_color;
	static const Vec4 alert_color;
	static const Vec4 accent_color;
	static const Vec4 background_color;
	static const Vec4 disabled_color;
	static r32 scale;
	static s32 mesh_id;
	static s32 texture_mesh_id;
	static Array<Vec3> vertices;
	static Array<Vec4> colors;
	static Array<s32> indices;
	static Array<TextureBlit> texture_blits;
	static void init(LoopSync*);
	static r32 get_scale(const s32, const s32);
	static void box(const RenderParams&, const Rect2&, const Vec4& = Vec4(1, 1, 1, 1));
	static void centered_box(const RenderParams&, const Rect2&, const Vec4& = Vec4(1, 1, 1, 1), r32 = 0.0f);
	static void border(const RenderParams&, const Rect2&, r32, const Vec4& = Vec4(1, 1, 1, 1));
	static void centered_border(const RenderParams&, const Rect2&, r32, const Vec4& = Vec4(1, 1, 1, 1), r32 = 0.0f);
	static void triangle(const RenderParams&, const Rect2&, const Vec4& = Vec4(1, 1, 1, 1), r32 = 0.0f);
	static void triangle_border(const RenderParams&, const Rect2&, r32, const Vec4& = Vec4(1, 1, 1, 1), r32 = 0.0f);
	static void update(const RenderParams&);
	static void draw(const RenderParams&);
	static void mesh(const RenderParams&, const AssetID, const Vec2&, const Vec2& = Vec2(1, 1), const Vec4& = Vec4(1, 1, 1, 1), r32 = 0.0f);
	static b8 project(const RenderParams&, const Vec3&, Vec2*);

	// Instantly draw a texture
	static void texture(const RenderParams&, const s32, const Rect2&, const Vec4& = Vec4(1, 1, 1, 1), const Rect2& = { Vec2::zero, Vec2(1, 1) }, const AssetID = AssetNull);

	// Cue up a sprite to be rendered later
	static void sprite(const RenderParams&, s32, const Rect2&, const Vec4& = Vec4(1, 1, 1, 1), const Rect2& = { Vec2::zero, Vec2(1, 1) }, r32 = 0.0f, const Vec2& = Vec2(0.5f), AssetID = AssetNull);

	static b8 flash_function(r32);
	static b8 flash_function_slow(r32);
	static b8 is_onscreen(const RenderParams&, const Vec3&, Vec2*, Vec2* = nullptr);
	static void indicator(const RenderParams&, const Vec3&, const Vec4&, b8, r32 = 1.0f, r32 = 0.0f);

#if DEBUG
	static Array<Vec3> debugs;
	static void debug(const Vec3&);
#endif
};

}
