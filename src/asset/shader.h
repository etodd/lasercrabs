#pragma once
#include "types.h"

namespace VI
{

namespace Asset
{
	namespace Uniform
	{
		const s32 count = 60;
		const AssetID ambient_color = 0;
		const AssetID bones = 1;
		const AssetID buffer_size = 2;
		const AssetID camera_light_radius = 3;
		const AssetID camera_light_strength = 4;
		const AssetID cloud_alpha = 5;
		const AssetID cloud_height_diff_scaled = 6;
		const AssetID cloud_inv_uv_scale = 7;
		const AssetID cloud_map = 8;
		const AssetID cloud_uv_offset = 9;
		const AssetID color_buffer = 10;
		const AssetID cull_behind_wall = 11;
		const AssetID cull_center = 12;
		const AssetID cull_radius = 13;
		const AssetID depth_buffer = 14;
		const AssetID detail2_light_vp = 15;
		const AssetID detail2_shadow_map = 16;
		const AssetID detail_light_vp = 17;
		const AssetID detail_shadow_map = 18;
		const AssetID diffuse_color = 19;
		const AssetID diffuse_map = 20;
		const AssetID displacement = 21;
		const AssetID far_plane = 22;
		const AssetID fog = 23;
		const AssetID fog_extent = 24;
		const AssetID fog_start = 25;
		const AssetID frontface = 26;
		const AssetID frustum = 27;
		const AssetID gravity = 28;
		const AssetID inv_buffer_size = 29;
		const AssetID inv_uv_scale = 30;
		const AssetID lifetime = 31;
		const AssetID light_color = 32;
		const AssetID light_direction = 33;
		const AssetID light_fov_dot = 34;
		const AssetID light_pos = 35;
		const AssetID light_radius = 36;
		const AssetID light_vp = 37;
		const AssetID lighting_buffer = 38;
		const AssetID mv = 39;
		const AssetID mvp = 40;
		const AssetID noise_sampler = 41;
		const AssetID normal_buffer = 42;
		const AssetID normal_map = 43;
		const AssetID p = 44;
		const AssetID range = 45;
		const AssetID range_center = 46;
		const AssetID scan_line_interval = 47;
		const AssetID shadow_map = 48;
		const AssetID size = 49;
		const AssetID ssao_buffer = 50;
		const AssetID time = 51;
		const AssetID tri_shadow_cascade = 52;
		const AssetID type = 53;
		const AssetID uv_offset = 54;
		const AssetID uv_scale = 55;
		const AssetID v = 56;
		const AssetID viewport_scale = 57;
		const AssetID vp = 58;
		const AssetID wall_normal = 59;
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
		const AssetID flat_instanced = 11;
		const AssetID flat_texture = 12;
		const AssetID fresnel = 13;
		const AssetID global_light = 14;
		const AssetID particle_eased = 15;
		const AssetID particle_rain = 16;
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