#pragma once
#include "types.h"
#include "lmath.h"
#include "data/array.h"
#include "render/ui.h"
#include "game.h"

namespace VI
{

struct RenderParams;
struct PlayerHuman;

#define MENU_ITEM_WIDTH (400.0f * UI::scale)
#define MENU_ITEM_FONT_SIZE 18.0f
#define MENU_ITEM_PADDING (10.0f * UI::scale)
#define MENU_ITEM_HEIGHT ((MENU_ITEM_FONT_SIZE * UI::scale) + MENU_ITEM_PADDING * 2.0f)
#define MENU_ITEM_PADDING_LEFT (48.0f * UI::scale)
#define MENU_ITEM_VALUE_OFFSET (MENU_ITEM_WIDTH * 0.7f)
#define DIALOG_ANIM_TIME 0.25f

struct UIMenu
{
	static UIMenu* active[MAX_GAMEPADS];

	struct Item
	{
		enum class Type : s8
		{
			Button,
			Slider,
			Text,
			count,
		};

		AssetID icon;
		UIText label;
		UIText value;
		Type type;
	};

	struct Origin
	{
		Vec2 pos;
		UIText::Anchor anchor_x;
		UIText::Anchor anchor_y;
	};

	enum class SliderItemAllowSelect : s8
	{
		No,
		Yes,
		count,
	};

	enum class EnableInput : s8
	{
		No,
		Yes,
		NoMovement,
		count,
	};

	static void text_clip_timer(UIText*, r32, r32, s32 = 0);
	static void text_clip(UIText*, r32, r32, s32 = 0);
	template<typename T> static b8 enum_option(T* t, s32 delta)
	{
		s32 value_new = s32(*t) + delta;
		if (value_new < 0)
			value_new = s32(T::count) - 1;
		else if (value_new >= s32(T::count))
			value_new = 0;
		*t = T(value_new);
		return delta != 0;
	}

	Array<Item> items;
	Origin origin;
	r32 animation_time;
	r32 cached_height;
	UIScroll scroll;
	s8 selected;
	s8 gamepad;
	EnableInput enable_input;

	UIMenu();
	void clear();
	void animate();
	r32 height() const;
	Rect2 rect(r32 = 1.0f) const;
	void start(const Update&, const Origin&, s8, EnableInput = EnableInput::Yes);
	const Item* last_visible_item() const;
	b8 add_item(Item::Type, const char*, const char* = nullptr, b8 = false, AssetID = AssetNull);
	b8 item(const Update&, const char*, const char* = nullptr, b8 = false, AssetID = AssetNull);
	b8 text(const Update&, const char*, const char* = nullptr, b8 = true, AssetID = AssetNull);
	s32 slider_item(const Update&, const char*, const char*, b8 = false, AssetID = AssetNull, SliderItemAllowSelect = SliderItemAllowSelect::No);
	void draw_ui(const RenderParams&) const;
	void end(const Update&);
};

typedef void(*DialogCallback)(s8);
typedef void(*DialogTextCallback)(const TextField&);
typedef void(*DialogTextCancelCallback)();
typedef void(*DialogLayoutCallback)(s8, const Update*, const RenderParams*);

namespace Menu
{

enum class State : s8
{
	Hidden,
	Visible,
	Maps,
	Teams,
	Player,
	Settings,
	SettingsControlsKeyboard,
	SettingsControlsGamepad,
	SettingsGraphics,
	Credits,
	count,
};

enum class TeamSelectMode : s8
{
	Normal,
	MatchStart,
	count,
};

enum class AllowClose : s8
{
	No,
	Yes,
	count,
};

extern State main_menu_state;
extern r32 dialog_time[MAX_GAMEPADS];

void init(const InputState&);
void exit(s8);
void update(const Update&);
void update_end(const Update&);
void clear();
void draw_ui(const RenderParams&);
void title();
void splash();
void title_multiplayer();
void show();
void open_url(const char*);
void refresh_variables(const InputState&);
void pause_menu(const Update&, const UIMenu::Origin&, s8, UIMenu*, State*);
void title_menu(const Update&, Camera*);
void teams_select_match_start_init(PlayerHuman*);
State teams(const Update&, const UIMenu::Origin&, s8, UIMenu*, TeamSelectMode, UIMenu::EnableInput = UIMenu::EnableInput::Yes);
void friendship_state(u32, b8);
b8 choose_region(const Update&, const UIMenu::Origin&, s8, UIMenu*, AllowClose);
void progress_spinner(const RenderParams&, const Vec2&, r32 = 20.0f);
void progress_bar(const RenderParams&, const char*, r32, const Vec2&);
void progress_infinite(const RenderParams&, const char*, const Vec2&);
void dialog(s8, DialogCallback, const char*, ...);
void dialog_with_cancel(s8, DialogCallback, DialogCallback, const char*, ...);
void dialog_with_time_limit(s8, DialogCallback, DialogCallback, r32, const char*, ...);
void dialog_no_action(s8);
void dialog_text_cancel_no_action();
void dialog_text(DialogTextCallback, const char*, s32, const char*, ...);
void dialog_text_with_cancel(DialogTextCallback, DialogTextCancelCallback, const char*, s32, const char*, ...);
void dialog_clear(s8);
void dialog_custom(s8, DialogLayoutCallback);
void draw_letterbox(const RenderParams&, r32, r32);
b8 dialog_active(s8);
AssetID region_string(Region);

}

}
