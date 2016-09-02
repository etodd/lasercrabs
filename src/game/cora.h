#pragma once
#include "types.h"
#include "data/entity.h"

namespace VI
{

namespace Cora
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
	LinkArg<AssetID>& node_executed();
	void text_clear();
	void text_schedule(r32, const char*);
}


}
