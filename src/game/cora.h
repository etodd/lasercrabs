#pragma once
#include "types.h"
#include "data/entity.h"

namespace VI
{

struct RenderParams;
struct Vec2;

namespace Cora
{
	enum class Mode
	{
		Hidden,
		Active,
	};

	void global_init();
	void init();
	void variable(AssetID, AssetID);
	AssetID variable(AssetID);
	void clear();
	void go(AssetID);
	b8 has_focus();
	LinkArg<AssetID>& node_executed();
	Link& conversation_finished();
	void text_clear();
	void text_schedule(r32, const char*);
	void activate(AssetID);
	void draw(const RenderParams&, const Vec2&);
}


}
