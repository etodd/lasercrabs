#pragma once
#include "types.h"

namespace VI
{

namespace Asset
{
	namespace Uniform
	{
		const s32 count = 45;
		const AssetID ambient_color = 0;
		const AssetID bones = 1;
		const AssetID color_buffer = 2;
		const AssetID cull_center = 3;
		const AssetID cull_radius = 4;
		const AssetID depth_buffer = 5;
		const AssetID detail_light_vp = 6;
		const AssetID detail_shadow_map = 7;
		const AssetID diffuse_color = 8;
		const AssetID diffuse_map = 9;
		const AssetID far_plane = 10;
		const AssetID fog = 11;
		const AssetID fog_extent = 12;
		const AssetID fog_start = 13;
		const AssetID frustum = 14;
		const AssetID gravity = 15;
		const AssetID inv_buffer_size = 16;
		const AssetID inv_uv_scale = 17;
		const AssetID lifetime = 18;
		const AssetID light_color = 19;
		const AssetID light_direction = 20;
		const AssetID light_fov_dot = 21;
		const AssetID light_pos = 22;
		const AssetID light_radius = 23;
		const AssetID light_vp = 24;
		const AssetID lighting_buffer = 25;
		const AssetID mv = 26;
		const AssetID mvp = 27;
		const AssetID noise_sampler = 28;
		const AssetID normal_buffer = 29;
		const AssetID p = 30;
		const AssetID player_light = 31;
		const AssetID range = 32;
		const AssetID range_center = 33;
		const AssetID shadow_map = 34;
		const AssetID size = 35;
		const AssetID ssao_buffer = 36;
		const AssetID time = 37;
		const AssetID type = 38;
		const AssetID uv_offset = 39;
		const AssetID uv_scale = 40;
		const AssetID v = 41;
		const AssetID viewport_scale = 42;
		const AssetID vp = 43;
		const AssetID wall_normal = 44;
	}
	namespace Shader
	{
		const s32 count = 24;
		const AssetID armature = 0;
		const AssetID bloom_downsample = 1;
		const AssetID blur = 2;
		const AssetID composite = 3;
		const AssetID culled = 4;
		const AssetID debug_depth = 5;
		const AssetID edge_detect = 6;
		const AssetID flat = 7;
		const AssetID flat_texture = 8;
		const AssetID global_light = 9;
		const AssetID particle_spark = 10;
		const AssetID particle_standard = 11;
		const AssetID particle_textured = 12;
		const AssetID point_light = 13;
		const AssetID sky_decal = 14;
		const AssetID skybox = 15;
		const AssetID spot_light = 16;
		const AssetID ssao = 17;
		const AssetID ssao_blur = 18;
		const AssetID ssao_downsample = 19;
		const AssetID standard = 20;
		const AssetID standard_instanced = 21;
		const AssetID ui = 22;
		const AssetID ui_texture = 23;
	}
}

}