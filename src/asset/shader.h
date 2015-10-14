#pragma once
#include "types.h"

namespace VI
{

namespace Asset
{
	namespace Uniform
	{
		const int count = 23;
		const AssetID bones = 0;
		const AssetID camera_pos = 1;
		const AssetID color_buffer = 2;
		const AssetID depth_buffer = 3;
		const AssetID diffuse_color = 4;
		const AssetID diffuse_map = 5;
		const AssetID fog_extent = 6;
		const AssetID fog_start = 7;
		const AssetID light_color = 8;
		const AssetID light_direction = 9;
		const AssetID light_fov_dot = 10;
		const AssetID light_pos = 11;
		const AssetID light_radius = 12;
		const AssetID light_vp = 13;
		const AssetID lighting_buffer = 14;
		const AssetID m = 15;
		const AssetID mvp = 16;
		const AssetID normal_buffer = 17;
		const AssetID p = 18;
		const AssetID shadow_map = 19;
		const AssetID uv_offset = 20;
		const AssetID uv_scale = 21;
		const AssetID v = 22;
	}
	namespace Shader
	{
		const int count = 10;
		const AssetID armature = 0;
		const AssetID composite = 1;
		const AssetID flat = 2;
		const AssetID flat_texture = 3;
		const AssetID point_light = 4;
		const AssetID skybox = 5;
		const AssetID spot_light = 6;
		const AssetID standard = 7;
		const AssetID ui = 8;
		const AssetID ui_texture = 9;
	}
}

}