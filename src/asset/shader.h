#pragma once
#include "types.h"

namespace VI
{

namespace Asset
{
	namespace Uniform
	{
		const int count = 33;
		const AssetID ambient_color = 0;
		const AssetID bones = 1;
		const AssetID color_buffer = 2;
		const AssetID depth_buffer = 3;
		const AssetID detail_light_vp = 4;
		const AssetID detail_shadow_map = 5;
		const AssetID diffuse_color = 6;
		const AssetID diffuse_map = 7;
		const AssetID far_plane = 8;
		const AssetID film_grain_size = 9;
		const AssetID frustum = 10;
		const AssetID inv_buffer_size = 11;
		const AssetID inv_uv_scale = 12;
		const AssetID light_color = 13;
		const AssetID light_direction = 14;
		const AssetID light_fov_dot = 15;
		const AssetID light_pos = 16;
		const AssetID light_radius = 17;
		const AssetID light_vp = 18;
		const AssetID lighting_buffer = 19;
		const AssetID mv = 20;
		const AssetID mvp = 21;
		const AssetID noise_sampler = 22;
		const AssetID normal_buffer = 23;
		const AssetID p = 24;
		const AssetID shadow_map = 25;
		const AssetID shockwave = 26;
		const AssetID ssao_buffer = 27;
		const AssetID uv_offset = 28;
		const AssetID uv_scale = 29;
		const AssetID v = 30;
		const AssetID vp = 31;
		const AssetID zenith_color = 32;
	}
	namespace Shader
	{
		const int count = 20;
		const AssetID armature = 0;
		const AssetID bloom_downsample = 1;
		const AssetID blur = 2;
		const AssetID composite = 3;
		const AssetID debug_depth = 4;
		const AssetID edge_detect = 5;
		const AssetID flat = 6;
		const AssetID flat_texture = 7;
		const AssetID global_light = 8;
		const AssetID point_grid = 9;
		const AssetID point_light = 10;
		const AssetID skybox = 11;
		const AssetID spot_light = 12;
		const AssetID ssao = 13;
		const AssetID ssao_blur = 14;
		const AssetID ssao_downsample = 15;
		const AssetID standard = 16;
		const AssetID standard_instanced = 17;
		const AssetID ui = 18;
		const AssetID ui_texture = 19;
	}
}

}