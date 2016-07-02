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
		Vec2 pos;
		UIText label;
		UIText value;
		b8 slider;
		Rect2 rect() const;
		Rect2 down_rect() const;
		Rect2 up_rect() const;
	};

	enum class Delta
	{
		None,
		Up,
		Down,
	};

	static r32 height(s32);

	char selected;
	StaticArray<Item, 10> items;
	u8 gamepad;
	r32 animation_time;

	UIMenu();
	void clear();
	void animate();
	void start(const Update&, u8, b8 = true);
	Rect2 add_item(Vec2*, b8, const char*, const char* = nullptr, b8 = false, AssetID = AssetNull);
	b8 item(const Update&, Vec2*, const char*, const char* = nullptr, b8 = false, AssetID = AssetNull);
	Delta slider_item(const Update&, Vec2*, const char*, const char*, b8 = false, AssetID = AssetNull);
	void draw_alpha(const RenderParams&) const;
	void end();
};

namespace Menu
{

enum class State
{
	Hidden,
	Visible,
	Options,
};

extern AssetID next_level;
extern Game::Mode next_mode;
extern r32 connect_timer;
extern b8 splitscreen_level_selected;
extern AssetID transition_previous_level;

void init();
void transition(AssetID, Game::Mode);
void update(const Update&);
void clear();
void draw(const RenderParams&);
void splitscreen();
void title();
void refresh_variables();
void pause_menu(const Update&, const Rect2&, u8, UIMenu*, State*);
b8 options(const Update&, u8, UIMenu*, Vec2*);
r32 options_height();

}

}
