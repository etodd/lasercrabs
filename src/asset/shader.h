#pragma once
#include "types.h"

namespace VI
{

namespace Asset
{
	namespace Uniform
	{
		const int count = 29;
		const AssetID ambient_color = 0;
		const AssetID bones = 1;
		const AssetID color_buffer = 2;
		const AssetID depth_buffer = 3;
		const AssetID diffuse_color = 4;
		const AssetID diffuse_map = 5;
		const AssetID far_plane = 6;
		const AssetID fog_extent = 7;
		const AssetID fog_start = 8;
		const AssetID frustum = 9;
		const AssetID inv_buffer_size = 10;
		const AssetID inv_uv_scale = 11;
		const AssetID light_color = 12;
		const AssetID light_direction = 13;
		const AssetID light_fov_dot = 14;
		const AssetID light_pos = 15;
		const AssetID light_radius = 16;
		const AssetID light_vp = 17;
		const AssetID lighting_buffer = 18;
		const AssetID mv = 19;
		const AssetID mvp = 20;
		const AssetID noise_sampler = 21;
		const AssetID normal_buffer = 22;
		const AssetID p = 23;
		const AssetID shadow_map = 24;
		const AssetID ssao_buffer = 25;
		const AssetID uv_offset = 26;
		const AssetID uv_scale = 27;
		const AssetID v = 28;
	}
	namespace Shader
	{
		const int count = 13;
		const AssetID armature = 0;
		const AssetID composite = 1;
		const AssetID flat = 2;
		const AssetID flat_texture = 3;
		const AssetID point_light = 4;
		const AssetID skybox = 5;
		const AssetID spot_light = 6;
		const AssetID ssao = 7;
		const AssetID ssao_blur = 8;
		const AssetID ssao_downsample = 9;
		const AssetID standard = 10;
		const AssetID ui = 11;
		const AssetID ui_texture = 12;
	}
}

}