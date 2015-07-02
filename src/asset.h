#pragma once
#include "types.h"

struct Asset
{
	struct Model
	{
		static const size_t count = 3;
		static const char* filenames[3];
		static const AssetID Alpha = 1;
		static const AssetID city3 = 2;
	};
	struct Texture
	{
		static const size_t count = 2;
		static const char* filenames[2];
		static const AssetID test = 1;
	};
	struct Shader
	{
		static const size_t count = 3;
		static const char* filenames[3];
		static const AssetID Armature = 1;
		static const AssetID Standard = 2;
	};
	struct Animation
	{
		static const size_t count = 1;
		static const char* filenames[1];
	};
};
