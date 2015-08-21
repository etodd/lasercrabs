#pragma once
#include "types.h"

namespace VI
{

namespace Asset
{
	namespace Uniform
	{
		const int count = 9;
		const AssetID ambient_color = 0;
		const AssetID bones = 1;
		const AssetID diffuse_color = 2;
		const AssetID diffuse_map = 3;
		const AssetID light_color = 4;
		const AssetID light_position = 5;
		const AssetID light_radius = 6;
		const AssetID m = 7;
		const AssetID mvp = 8;
	}
	namespace Shader
	{
		const int count = 5;
		const AssetID Armature = 0;
		const AssetID Standard = 1;
		const AssetID UI = 2;
		const AssetID flat = 3;
		const AssetID flat_texture = 4;
	}
}

}