#pragma once
#include "asset/string.h"

namespace VI
{

namespace strings = Asset::String;
void strings_set(AssetID, const char*);
const char* _(AssetID);

};