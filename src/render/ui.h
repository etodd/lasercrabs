#pragma once

#include "types.h"
#include "lmath.h"
#include "data/array.h"

namespace VI
{


#define UI_TEXT_SIZE_DEFAULT 16.0f

struct RenderParams;

typedef enum UITextFlags
{
	UITextFlagNone = 0,
	UITextFlagSingleLine = 1,
} UITextFlags;

struct UIText
{
	struct VariableEntry
	{
		s8 gamepad;
		char name[255];
		char value[255];
	};

	static Array<VariableEntry> variables;

	static void variables_clear();
	static void variable_add(s8, const char*, const char*);

	enum class Anchor : s8
	{
		Min,
		Center,
		Max,
		count,
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
	void text(s8, const char*, ...);
	void text_raw(s8, const char*, UITextFlags = UITextFlagNone);
	void refresh_bounds();
	void set_size(r32);
	void wrap(r32);
	b8 clipped() const;
	b8 has_text() const;
	void draw(const RenderParams&, const Vec2&, r32 = 0.0f) const;
	UIText();
};

#define UI_SCROLL_MAX 7
struct UIScroll
{
	s32 pos;
	s32 count;

	void update(const Update&, s32, s32 = -1); // for non-menu things
	void update_menu(s32); // for menus
	void scroll_into_view(s32);
	void start(const RenderParams&, const Vec2&) const;
	void end(const RenderParams&, const Vec2&) const;
	b8 item(s32) const;
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

	static const Vec4 color_default;
	static const Vec4 color_alert;
	static const Vec4 color_accent;
	static const Vec4 color_background;
	static const Vec4 color_disabled;
	static r32 scale;
	static AssetID mesh_id;
	static AssetID texture_mesh_id;
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
	static void triangle_percentage(const RenderParams&, const Rect2&, r32, const Vec4& = Vec4(1, 1, 1, 1), r32 = 0.0f);
	static void triangle_border(const RenderParams&, const Rect2&, r32, const Vec4& = Vec4(1, 1, 1, 1), r32 = 0.0f);
	static void update(const RenderParams&);
	static void draw(const RenderParams&);
	static void mesh(const RenderParams&, const AssetID, const Vec2&, const Vec2& = Vec2(1, 1), const Vec4& = Vec4(1, 1, 1, 1), r32 = 0.0f);
	static b8 project(const RenderParams&, const Vec3&, Vec2*);
	static s32 input_delta_vertical(const Update&, s32);
	static s32 input_delta_horizontal(const Update&, s32);

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
