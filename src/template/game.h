#pragma once
#include "render/render.h"
#include "data/entity.h"
#include "ai.h"

namespace VI
{

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
	struct Data
	{
		AssetID level;
	};

	static float time_scale;
	static Data data;
	static const Data initial_data;
	static AssetID scheduled_load_level;
	static Array<UpdateFunction> updates;
	static Array<DrawFunction> draws;
	static Array<CleanupFunction> cleanups;
	static bool init(RenderSync*);
	static void execute(const Update&, const char*);
	static void update(const Update&);
	static void schedule_load_level(AssetID);
	static void unload_level();
	static void load_level(AssetID);
	static void draw_opaque(const RenderParams&);
	static void draw_alpha(const RenderParams&);
	static void draw_additive(const RenderParams&);
	static void term();
};

}