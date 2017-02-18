#pragma once
#include "types.h"

namespace VI
{

namespace Asset
{
	namespace Uniform
	{
		const s32 count = 56;
		const AssetID ambient_color = 0;
		const AssetID bones = 1;
		const AssetID buffer_size = 2;
		const AssetID cloud_alpha = 3;
		const AssetID cloud_height_diff_scaled = 4;
		const AssetID cloud_inv_uv_scale = 5;
		const AssetID cloud_map = 6;
		const AssetID cloud_uv_offset = 7;
		const AssetID color_buffer = 8;
		const AssetID cull_behind_wall = 9;
		const AssetID cull_center = 10;
		const AssetID cull_radius = 11;
		const AssetID depth_buffer = 12;
		const AssetID detail_light_vp = 13;
		const AssetID detail_shadow_map = 14;
		const AssetID diffuse_color = 15;
		const AssetID diffuse_map = 16;
		const AssetID displacement = 17;
		const AssetID far_plane = 18;
		const AssetID fog = 19;
		const AssetID fog_extent = 20;
		const AssetID fog_start = 21;
		const AssetID frustum = 22;
		const AssetID gravity = 23;
		const AssetID inv_buffer_size = 24;
		const AssetID inv_uv_scale = 25;
		const AssetID lifetime = 26;
		const AssetID light_color = 27;
		const AssetID light_direction = 28;
		const AssetID light_fov_dot = 29;
		const AssetID light_pos = 30;
		const AssetID light_radius = 31;
		const AssetID light_vp = 32;
		const AssetID lighting_buffer = 33;
		const AssetID mv = 34;
		const AssetID mvp = 35;
		const AssetID noise_sampler = 36;
		const AssetID normal_buffer = 37;
		const AssetID normal_map = 38;
		const AssetID p = 39;
		const AssetID plane = 40;
		const AssetID player_light = 41;
		const AssetID range = 42;
		const AssetID range_center = 43;
		const AssetID scan_line_interval = 44;
		const AssetID shadow_map = 45;
		const AssetID size = 46;
		const AssetID ssao_buffer = 47;
		const AssetID time = 48;
		const AssetID type = 49;
		const AssetID uv_offset = 50;
		const AssetID uv_scale = 51;
		const AssetID v = 52;
		const AssetID viewport_scale = 53;
		const AssetID vp = 54;
		const AssetID wall_normal = 55;
	}
	namespace Shader
	{
		const s32 count = 35;
		const AssetID armature = 0;
		const AssetID blit = 1;
		const AssetID blit_depth = 2;
		const AssetID bloom_downsample = 3;
		const AssetID blur = 4;
		const AssetID clouds = 5;
		const AssetID composite = 6;
		const AssetID culled = 7;
		const AssetID debug_depth = 8;
		const AssetID downsample = 9;
		const AssetID flat = 10;
		const AssetID flat_clipped = 11;
		const AssetID flat_instanced = 12;
		const AssetID flat_texture = 13;
		const AssetID fresnel = 14;
		const AssetID global_light = 15;
		const AssetID particle_eased = 16;
		const AssetID particle_spark = 17;
		const AssetID particle_standard = 18;
		const AssetID particle_textured = 19;
		const AssetID point_light = 20;
		const AssetID scan_lines = 21;
		const AssetID sky_decal = 22;
		const AssetID skybox = 23;
		const AssetID spot_light = 24;
		const AssetID ssao = 25;
		const AssetID ssao_blur = 26;
		const AssetID ssao_downsample = 27;
		const AssetID standard = 28;
		const AssetID standard_flat = 29;
		const AssetID standard_instanced = 30;
		const AssetID ui = 31;
		const AssetID ui_texture = 32;
		const AssetID underwater = 33;
		const AssetID water = 34;
	}
}

}