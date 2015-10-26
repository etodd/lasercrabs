#pragma once

namespace VI
{

struct GameTime
{
	float total;
	float delta;
	float real;
};

typedef int AssetID;
const AssetID AssetNull = -1;
typedef int AssetRef;

}
