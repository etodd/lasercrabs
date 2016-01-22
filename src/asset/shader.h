#pragma once
#include "types.h"

namespace VI
{

namespace Asset
{
	namespace Uniform
	{
		const s32 count = 38;
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
		const AssetID fog = 10;
		const AssetID fog_extent = 11;
		const AssetID fog_start = 12;
		const AssetID frustum = 13;
		const AssetID inv_buffer_size = 14;
		const AssetID inv_uv_scale = 15;
		const AssetID light_color = 16;
		const AssetID light_direction = 17;
		const AssetID light_fov_dot = 18;
		const AssetID light_pos = 19;
		const AssetID light_radius = 20;
		const AssetID light_vp = 21;
		const AssetID lighting_buffer = 22;
		const AssetID mv = 23;
		const AssetID mvp = 24;
		const AssetID noise_sampler = 25;
		const AssetID normal_buffer = 26;
		const AssetID p = 27;
		const AssetID range = 28;
		const AssetID shadow_map = 29;
		const AssetID ssao_buffer = 30;
		const AssetID type = 31;
		const AssetID uv_offset = 32;
		const AssetID uv_scale = 33;
		const AssetID v = 34;
		const AssetID vp = 35;
		const AssetID wall_normal = 36;
		const AssetID zenith_color = 37;
	}
	namespace Shader
	{
		const s32 count = 20;
		const AssetID armature = 0;
		const AssetID bloom_downsample = 1;
		const AssetID blur = 2;
		const AssetID composite = 3;
		const AssetID debug_depth = 4;
		const AssetID edge_detect = 5;
		const AssetID flat = 6;
		const AssetID flat_texture = 7;
		const AssetID global_light = 8;
		const AssetID point_light = 9;
		const AssetID sky_decal = 10;
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
