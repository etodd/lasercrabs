#pragma once
#include "types.h"
#include "game.h"

namespace VI
{

typedef void(*ScriptFunction)(const Update&, const EntityFinder&);

struct Script
{
	static Script all[];

	static Script* find(const char*);

	const char* name;
	ScriptFunction function;
};

namespace Penelope
{
	void global_init();
	void init(AssetID = AssetNull);
	void add_terminal(Entity*);
	void variable(AssetID, AssetID);
	AssetID variable(AssetID);
	void clear();
	b8 has_focus();
};

}
