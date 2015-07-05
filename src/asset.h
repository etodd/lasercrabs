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
		static const size_t count = 9;
		static const char* filenames[9];
		static const AssetID idle = 1;
		static const AssetID jump = 2;
		static const AssetID run = 3;
		static const AssetID strafe_left = 4;
		static const AssetID strafe_right = 5;
		static const AssetID turn_left = 6;
		static const AssetID turn_right = 7;
		static const AssetID walk = 8;
	};
};
