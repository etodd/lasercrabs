#pragma once
#include "types.h"
#include "lmath.h"
#include "data/array.h"
#include "render/ui.h"
#include "game.h"

namespace VI
{

struct RenderParams;

#define MENU_ITEM_WIDTH (400.0f * UI::scale)
#define MENU_ITEM_FONT_SIZE 18.0f
#define MENU_ITEM_PADDING (10.0f * UI::scale)
#define MENU_ITEM_HEIGHT ((MENU_ITEM_FONT_SIZE * UI::scale) + MENU_ITEM_PADDING * 2.0f)
#define MENU_ITEM_PADDING_LEFT (48.0f * UI::scale)
#define MENU_ITEM_VALUE_OFFSET (MENU_ITEM_WIDTH * 0.7f)

struct UIMenu
{
	static UIMenu* active[MAX_GAMEPADS];

	struct Item
	{
		AssetID icon;
		UIText label;
		UIText value;
		b8 slider;
	};

	enum class Delta
	{
		None,
		Up,
		Down,
	};

	static void text_clip_timer(UIText*, r32, r32);
	static void text_clip(UIText*, r32, r32);

	char selected;
	StaticArray<Item, 10> items;
	s8 gamepad;
	r32 animation_time;
	UIScroll scroll;

	UIMenu();
	void clear();
	void animate();
	r32 height() const;
	void start(const Update&, s8, b8 = true);
	const Item* last_visible_item() const;
	b8 add_item(b8, const char*, const char* = nullptr, b8 = false, AssetID = AssetNull);
	b8 item(const Update&, const char*, const char* = nullptr, b8 = false, AssetID = AssetNull);
	Delta slider_item(const Update&, const char*, const char*, b8 = false, AssetID = AssetNull);
	void draw_alpha(const RenderParams&, const Vec2&, UIText::Anchor, UIText::Anchor) const;
	void end();
};

typedef void(*DialogCallback)(s8);

namespace Menu
{

enum class State
{
	Hidden,
	Visible,
	Options,
};

extern State main_menu_state;
extern DialogCallback dialog_callback[MAX_GAMEPADS];

void init();
void update(const Update&);
void clear();
void draw(const RenderParams&);
void title();
void show();
void refresh_variables();
void pause_menu(const Update&, s8, UIMenu*, State*);
b8 options(const Update&, s8, UIMenu*);
void progress_spinner(const RenderParams&, const Vec2&, r32 = 20.0f);
void progress_bar(const RenderParams&, const char*, r32, const Vec2&);
void progress_infinite(const RenderParams&, const char*, const Vec2&);
void dialog(s8, DialogCallback, const char*, ...);
void dialog_with_time_limit(s8, DialogCallback, r32, const char*, ...);
void dialog_no_action(s8);
void draw_letterbox(const RenderParams&, r32, r32);

}

}
