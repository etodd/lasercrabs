#pragma once
#include "types.h"

namespace VI
{

namespace Asset
{
	namespace Uniform
	{
		const int count = 14;
		const AssetID bones = 0;
		const AssetID camera_pos = 1;
		const AssetID color_buffer = 2;
		const AssetID depth_buffer = 3;
		const AssetID diffuse_color = 4;
		const AssetID diffuse_map = 5;
		const AssetID light_color = 6;
		const AssetID light_pos = 7;
		const AssetID light_radius = 8;
		const AssetID lighting_buffer = 9;
		const AssetID m = 10;
		const AssetID mvp = 11;
		const AssetID normal_buffer = 12;
		const AssetID p = 13;
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