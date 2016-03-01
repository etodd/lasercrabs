#include "strings.h"
#include <cstring>

namespace VI
{


const char* string_values[Asset::String::count];

void strings_set(AssetID id, const char* value)
{
	string_values[id] = value;
}

AssetID string_get(const char* value)
{
	for (s32 i = 0; i < (s32)Asset::String::count; i++)
	{
		if (strcmp(string_values[i], value) == 0)
			return i;
	}
	return AssetNull;
}

const char* _(AssetID id)
{
	return string_values[id];
};


};
