#pragma once
#include "types.h"

namespace VI
{

struct Asset
{
	static const AssetID Nothing = -1;
	struct Model
	{
		static const size_t count = 3;
		static const char* filenames[3];
		static const AssetID Alpha;
		static const AssetID city3;
		static const AssetID cube;
	};
	struct Texture
	{
		static const size_t count = 1;
		static const char* filenames[1];
		static const AssetID test;
	};
	struct Shader
	{
		static const size_t count = 2;
		static const char* filenames[2];
		static const AssetID Armature;
		static const AssetID Standard;
	};
	struct Animation
	{
		static const size_t count = 1;
		static const char* filenames[1];
		static const AssetID walk;
	};
	struct Uniform
	{
		static const size_t count = 6;
		static const char* filenames[6];
		static const AssetID Bones;
		static const AssetID LightPosition_worldspace;
		static const AssetID M;
		static const AssetID MVP;
		static const AssetID V;
		static const AssetID myTextureSampler;
	};
	struct Font
	{
		static const size_t count = 1;
		static const char* filenames[1];
		static const AssetID Planer_Reg;
	};
};

}