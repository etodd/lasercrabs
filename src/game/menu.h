#pragma once
#include "types.h"
#include "lmath.h"
#include "data/array.h"
#include "render/ui.h"

namespace VI
{

struct RenderParams;

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

}

#define MENU_ITEM_WIDTH (384.0f * UI::scale)
#define MENU_ITEM_FONT_SIZE 24.0f
#define MENU_ITEM_HEIGHT (menu_item_font_size * UI::scale)
#define MENU_ITEM_PADDING (6.0f * UI::scale)

struct UIMenu
{
	struct Item
	{
		Vec2 pos;
		UIText label;
	};

	char selected;
	StaticArray<Item, 10> items;

	UIMenu();
	void clear();
	void start(const Update&, u8);
	b8 item(const Update&, u8, Vec2*, const char*);
	void end();
	void draw_alpha(const RenderParams&) const;
};

}
