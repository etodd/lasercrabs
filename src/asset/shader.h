#pragma once
#include "types.h"

namespace VI
{

namespace Asset
{
	namespace Uniform
	{
		const s32 count = 42;
		const AssetID ambient_color = 0;
		const AssetID bones = 1;
		const AssetID color_buffer = 2;
		const AssetID depth_buffer = 3;
		const AssetID detail_light_vp = 4;
		const AssetID detail_shadow_map = 5;
		const AssetID diffuse_color = 6;
		const AssetID diffuse_map = 7;
		const AssetID far_plane = 8;
		const AssetID fog = 9;
		const AssetID fog_extent = 10;
		const AssetID fog_start = 11;
		const AssetID frustum = 12;
		const AssetID gravity = 13;
		const AssetID inv_buffer_size = 14;
		const AssetID inv_uv_scale = 15;
		const AssetID lifetime = 16;
		const AssetID light_color = 17;
		const AssetID light_direction = 18;
		const AssetID light_fov_dot = 19;
		const AssetID light_pos = 20;
		const AssetID light_radius = 21;
		const AssetID light_vp = 22;
		const AssetID lighting_buffer = 23;
		const AssetID mv = 24;
		const AssetID mvp = 25;
		const AssetID noise_sampler = 26;
		const AssetID normal_buffer = 27;
		const AssetID p = 28;
		const AssetID range = 29;
		const AssetID shadow_map = 30;
		const AssetID size = 31;
		const AssetID ssao_buffer = 32;
		const AssetID time = 33;
		const AssetID type = 34;
		const AssetID uv_offset = 35;
		const AssetID uv_scale = 36;
		const AssetID v = 37;
		const AssetID viewport_scale = 38;
		const AssetID vp = 39;
		const AssetID wall_normal = 40;
		const AssetID zenith_color = 41;
	}
	namespace Shader
	{
		const s32 count = 22;
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
		const AssetID spark = 12;
		const AssetID spot_light = 13;
		const AssetID ssao = 14;
		const AssetID ssao_blur = 15;
		const AssetID ssao_downsample = 16;
		const AssetID standard = 17;
		const AssetID standard_instanced = 18;
		const AssetID standard_particle = 19;
		const AssetID ui = 20;
		const AssetID ui_texture = 21;
	}
}

}