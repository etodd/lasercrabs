#pragma once

#include <cstdlib>

#define ASSET(x, y) static const ID x = { y };
struct Asset
{
	typedef size_t ID;
	struct Model
	{
		static const char* filenames[];
		static const size_t count = 1;
		ASSET(city3, 0)
	};
	struct Texture
	{
		static const char* filenames[];
		static const size_t count = 1;
		ASSET(test, 0)
	};
	struct Shader
	{
		static const char* filenames[];
		static const size_t count = 1;
		ASSET(Standard, 0)
	};
};
#undef ASSET
