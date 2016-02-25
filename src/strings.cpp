#include "strings.h"

namespace VI
{


const char* string_values[Asset::String::count];

void strings_set(AssetID id, const char* value)
{
	string_values[id] = value;
}

const char* _(AssetID id)
{
	return string_values[id];
};


};
