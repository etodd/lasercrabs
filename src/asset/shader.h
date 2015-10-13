#pragma once
#include "types.h"

namespace VI
{

namespace Asset
{
	namespace Uniform
	{
		const int count = 10;
		const AssetID bones = 0;
		const AssetID color_buffer = 1;
		const AssetID depth_buffer = 2;
		const AssetID diffuse_color = 3;
		const AssetID diffuse_map = 4;
		const AssetID lighting_buffer = 5;
		const AssetID m = 6;
		const AssetID mvp = 7;
		const AssetID normal_buffer = 8;
		const AssetID p = 9;
	}
	namespace Shader
	{
		const int count = 8;
		const AssetID armature = 0;
		const AssetID composite = 1;
		const AssetID flat = 2;
		const AssetID flat_texture = 3;
		const AssetID lighting = 4;
		const AssetID standard = 5;
		const AssetID ui = 6;
		const AssetID ui_texture = 7;
	}
}

}