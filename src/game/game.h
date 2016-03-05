#pragma once
#include "data/pin_array.h"
#include "ai.h"
#include "player.h"
#include "ai_player.h"
#include "input.h"

namespace VI
{

struct RenderParams;

typedef void(*UpdateFunction)(const Update&);
typedef void(*DrawFunction)(const RenderParams&);
typedef void(*CleanupFunction)();

struct EntityFinder
{
	struct NameEntry
	{
		const char* name;
		Ref<Entity> entity;
	};

	Array<NameEntry> map;
	void add(const char*, Entity*);
	Entity* find(const char*) const;
};

struct Game
{
	enum class Mode
	{
		Multiplayer,
		Parkour,
	};
	struct Data
	{
		AssetID level;
		AssetID next_level;
		Mode mode;
		AI::Team local_player_config[MAX_GAMEPADS];
		b8 third_person;
		b8 allow_detach;
		b8 local_multiplayer;
		Data();
	};

	struct Bindings
	{
		InputBinding start;
		InputBinding action;
		InputBinding cancel;
		InputBinding pause;
	};

	static Bindings bindings;
	static b8 quit;
	static r32 time_scale;
	static GameTime time;
	static GameTime real_time;
	static Data data;
	static Vec2 cursor;
	static b8 cursor_updated;
	static AssetID scheduled_load_level;
	static Array<UpdateFunction> updates;
	static Array<DrawFunction> draws;
	static Array<CleanupFunction> cleanups;

	static b8 init(LoopSync*);
	static void execute(const Update&, const char*);
	static void draw_cursor(const RenderParams&);
	static void update_cursor(const Update&);
	static void update(const Update&);
	static void schedule_load_level(AssetID);
	static void unload_level();
	static void load_level(const Update&, AssetID);
	static void draw_opaque(const RenderParams&);
	static void draw_alpha(const RenderParams&);
	static void draw_additive(const RenderParams&);
	static void term();
};

}
