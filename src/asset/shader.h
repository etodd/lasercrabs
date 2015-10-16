#pragma once
#include "types.h"

namespace VI
{

namespace Asset
{
	namespace Uniform
	{
		const int count = 27;
		const AssetID ambient_color = 0;
		const AssetID bones = 1;
		const AssetID buffer_size = 2;
		const AssetID color_buffer = 3;
		const AssetID depth_buffer = 4;
		const AssetID diffuse_color = 5;
		const AssetID diffuse_map = 6;
		const AssetID far_plane = 7;
		const AssetID frustum = 8;
		const AssetID inv_buffer_size = 9;
		const AssetID inv_uv_scale = 10;
		const AssetID light_color = 11;
		const AssetID light_direction = 12;
		const AssetID light_fov_dot = 13;
		const AssetID light_pos = 14;
		const AssetID light_radius = 15;
		const AssetID light_vp = 16;
		const AssetID lighting_buffer = 17;
		const AssetID mv = 18;
		const AssetID mvp = 19;
		const AssetID noise_sampler = 20;
		const AssetID normal_buffer = 21;
		const AssetID p = 22;
		const AssetID shadow_map = 23;
		const AssetID ssao_buffer = 24;
		const AssetID uv_offset = 25;
		const AssetID uv_scale = 26;
	}
	namespace Shader
	{
		const int count = 12;
		const AssetID armature = 0;
		const AssetID composite = 1;
		const AssetID flat = 2;
		const AssetID flat_texture = 3;
		const AssetID point_light = 4;
		const AssetID spot_light = 5;
		const AssetID ssao = 6;
		const AssetID ssao_blur = 7;
		const AssetID ssao_downsample = 8;
		const AssetID standard = 9;
		const AssetID ui = 10;
		const AssetID ui_texture = 11;
	}
}

}