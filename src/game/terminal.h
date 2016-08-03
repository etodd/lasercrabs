#pragma once
#include "types.h"

namespace VI
{

struct EntityFinder;
struct RenderParams;

namespace Penelope
{
	enum class Mode
	{
		Hidden,
		Center,
		Left,
	};

	void global_init();
	void init(AssetID = AssetNull, Mode = Mode::Left);
	void variable(AssetID, AssetID);
	AssetID variable(AssetID);
	void clear();
	void go(AssetID);
	b8 has_focus();
	void link_node_executed(void(*)(AssetID));
	void text_clear();
	void text_schedule(r32, const char*);
}


namespace Terminal
{

void init(const Update&, const EntityFinder&);
void update(const Update&);
void draw(const RenderParams&);
void show();

}

}
