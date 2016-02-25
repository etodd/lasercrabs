#pragma once
#include "types.h"
#include "lmath.h"
#include "data/array.h"
#include "render/ui.h"

namespace VI
{

struct RenderParams;

#define MENU_ITEM_WIDTH (384.0f * UI::scale)
#define MENU_ITEM_FONT_SIZE 24.0f
#define MENU_ITEM_PADDING (10.0f * UI::scale)
#define MENU_ITEM_HEIGHT ((MENU_ITEM_FONT_SIZE * UI::scale) + MENU_ITEM_PADDING * 2.0f)
#define MENU_ITEM_PADDING_LEFT (24.0f * UI::scale)
#define MENU_ITEM_VALUE_OFFSET (MENU_ITEM_WIDTH * 0.75f)

struct UIMenu
{
	struct Item
	{
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

	char selected;
	StaticArray<Item, 10> items;

	static r32 height(s32);

	UIMenu();
	void clear();
	void start(const Update&, u8);
	Rect2 add_item(Vec2*, b8, const char*, const char* = nullptr, b8 = false);
	b8 item(const Update&, u8, Vec2*, const char*, const char* = nullptr, b8 = false);
	Delta slider_item(const Update&, u8, Vec2*, const char*, const char*, b8 = false);
	void draw_alpha(const RenderParams&) const;
	void end();
};

namespace Menu
{

void init();
void transition(AssetID);
void update(const Update&);
void clear();
void draw(const RenderParams&);
void menu();
void title();
void refresh_variables();
b8 is_special_level(AssetID);
bool options(const Update&, u8, UIMenu*, Vec2*);
r32 options_height();

}

}
