#pragma once
#include "types.h"

namespace VI
{

namespace Asset
{
	namespace Uniform
	{
		const int count = 31;
		const AssetID ambient_color = 0;
		const AssetID bones = 1;
		const AssetID color_buffer = 2;
		const AssetID depth_buffer = 3;
		const AssetID diffuse_color = 4;
		const AssetID diffuse_map = 5;
		const AssetID far_plane = 6;
		const AssetID film_grain_size = 7;
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
		const AssetID shockwave = 24;
		const AssetID ssao_buffer = 25;
		const AssetID uv_offset = 26;
		const AssetID uv_scale = 27;
		const AssetID v = 28;
		const AssetID vp = 29;
		const AssetID zenith_color = 30;
	}
	namespace Shader
	{
		const int count = 19;
		const AssetID armature = 0;
		const AssetID bloom_downsample = 1;
		const AssetID blur = 2;
		const AssetID composite = 3;
		const AssetID edge_detect = 4;
		const AssetID flat = 5;
		const AssetID flat_texture = 6;
		const AssetID global_light = 7;
		const AssetID point_grid = 8;
		const AssetID point_light = 9;
		const AssetID skybox = 10;
		const AssetID spot_light = 11;
		const AssetID ssao = 12;
		const AssetID ssao_blur = 13;
		const AssetID ssao_downsample = 14;
		const AssetID standard = 15;
		const AssetID standard_instanced = 16;
		const AssetID ui = 17;
		const AssetID ui_texture = 18;
	}
}

}