#pragma once
#include "asset/string.h"

namespace VI
{

namespace strings = Asset::String;
void strings_set(AssetID, const char*);
AssetID strings_get(const char*);
AssetID strings_add_dynamic(const char*, const char*);
const char* _(AssetID);

};