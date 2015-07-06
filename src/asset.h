#pragma once
#include "types.h"

struct Asset
{
	static const AssetID None = -1;
	struct Model
	{
		static const size_t count = 2;
		static const char* filenames[2];
		static const AssetID Alpha = 0;
		static const AssetID city3 = 1;
	};
	struct Texture
	{
		static const size_t count = 1;
		static const char* filenames[1];
		static const AssetID test = 0;
	};
	struct Shader
	{
		static const size_t count = 2;
		static const char* filenames[2];
		static const AssetID Armature = 0;
		static const AssetID Standard = 1;
	};
	struct Animation
	{
		static const size_t count = 8;
		static const char* filenames[8];
		static const AssetID idle = 0;
		static const AssetID jump = 1;
		static const AssetID run = 2;
		static const AssetID strafe_left = 3;
		static const AssetID strafe_right = 4;
		static const AssetID turn_left = 5;
		static const AssetID turn_right = 6;
		static const AssetID walk = 7;
	};
	struct Uniform
	{
		static const size_t count = 5;
		static const char* filenames[5];
		static const AssetID LightPosition_worldspace = 0;
		static const AssetID M = 1;
		static const AssetID MVP = 2;
		static const AssetID V = 3;
		static const AssetID myTextureSampler = 4;
	};
};
