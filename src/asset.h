#pragma once

#include <cstdlib>

#define ASSET(x, y) static const ID x = { y };
struct Asset
{
	typedef size_t ID;
	struct Model
	{
		static const size_t count = 2;
		static const char* filenames[count];
		ASSET(city3, 1)
	};
	struct Texture
	{
		static const size_t count = 2;
		static const char* filenames[count];
		ASSET(test, 1)
	};
	struct Shader
	{
		static const size_t count = 2;
		static const char* filenames[count];
		ASSET(Standard, 1)
	};
};
#undef ASSET
