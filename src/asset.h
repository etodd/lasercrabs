#pragma once
#include "types.h"

struct Asset
{
	struct Model
	{
		static const size_t count = 3;
		static const char* filenames[3];
		static const AssetID city3 = 1;
		static const AssetID Alpha = 2;
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
		static const AssetID Standard = 1;
		static const AssetID Armature = 2;
	};
	struct Animation
	{
		static const size_t count = 11;
		static const char* filenames[11];
		static const AssetID idle = 1;
		static const AssetID right_strafe_walking = 2;
		static const AssetID standard_run = 3;
		static const AssetID right_strafe = 4;
		static const AssetID left_strafe = 5;
		static const AssetID right_turn_90 = 6;
		static const AssetID jump = 7;
		static const AssetID left_strafe_walking = 8;
		static const AssetID walking = 9;
		static const AssetID left_turn_90 = 10;
	};
};
