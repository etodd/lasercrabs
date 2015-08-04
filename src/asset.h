#pragma once
#include "types.h"

namespace VI
{

struct Asset
{
	static const AssetID Nothing = -1;
	struct Model
	{
		static const int count = 7;
		static const char* filenames[7];
		static const AssetID Alpha;
		static const AssetID city1;
		static const AssetID city2;
		static const AssetID city3;
		static const AssetID city4;
		static const AssetID city4_1;
		static const AssetID cube;
	};
	struct Texture
	{
		static const int count = 1;
		static const char* filenames[1];
		static const AssetID test;
	};
	struct Shader
	{
		static const int count = 3;
		static const char* filenames[3];
		static const AssetID Armature;
		static const AssetID Standard;
		static const AssetID UI;
	};
	struct Animation
	{
		static const int count = 1;
		static const char* filenames[1];
		static const AssetID walk;
	};
	struct Uniform
	{
		static const int count = 9;
		static const char* filenames[9];
		static const AssetID ambient_color;
		static const AssetID bones;
		static const AssetID diffuse_color;
		static const AssetID diffuse_map;
		static const AssetID light_color;
		static const AssetID light_position;
		static const AssetID light_radius;
		static const AssetID m;
		static const AssetID mvp;
	};
	struct Font
	{
		static const int count = 2;
		static const char* filenames[2];
		static const AssetID SegoeUISymbol;
		static const AssetID lowpoly;
	};
};

}