#pragma once
#include "types.h"

namespace VI
{

namespace Asset
{
	namespace Uniform
	{
		const s32 count = 62;
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
		const AssetID fade_in = 22;
		const AssetID far_plane = 23;
		const AssetID fog = 24;
		const AssetID fog_extent = 25;
		const AssetID fog_start = 26;
		const AssetID frontface = 27;
		const AssetID frustum = 28;
		const AssetID gravity = 29;
		const AssetID inv_buffer_size = 30;
		const AssetID inv_uv_scale = 31;
		const AssetID lifetime = 32;
		const AssetID light_color = 33;
		const AssetID light_direction = 34;
		const AssetID light_fov_dot = 35;
		const AssetID light_pos = 36;
		const AssetID light_radius = 37;
		const AssetID light_vp = 38;
		const AssetID lighting_buffer = 39;
		const AssetID mv = 40;
		const AssetID mvp = 41;
		const AssetID noise_sampler = 42;
		const AssetID normal_buffer = 43;
		const AssetID normal_map = 44;
		const AssetID p = 45;
		const AssetID radius = 46;
		const AssetID range = 47;
		const AssetID range_center = 48;
		const AssetID scan_line_interval = 49;
		const AssetID shadow_map = 50;
		const AssetID size = 51;
		const AssetID ssao_buffer = 52;
		const AssetID time = 53;
		const AssetID tri_shadow_cascade = 54;
		const AssetID type = 55;
		const AssetID uv_offset = 56;
		const AssetID uv_scale = 57;
		const AssetID v = 58;
		const AssetID viewport_scale = 59;
		const AssetID vp = 60;
		const AssetID wall_normal = 61;
	}
	namespace Shader
	{
		const s32 count = 39;
		const AssetID armature = 0;
		const AssetID blit = 1;
		const AssetID bloom_downsample = 2;
		const AssetID blur = 3;
		const AssetID clouds = 4;
		const AssetID composite = 5;
		const AssetID culled = 6;
		const AssetID debug_depth = 7;
		const AssetID downsample = 8;
		const AssetID flat = 9;
		const AssetID flat_instanced = 10;
		const AssetID flat_texture = 11;
		const AssetID flat_texture_offset = 12;
		const AssetID fresnel = 13;
		const AssetID global_light = 14;
		const AssetID nav_dots = 15;
		const AssetID particle_alpha = 16;
		const AssetID particle_eased = 17;
		const AssetID particle_limited_size = 18;
		const AssetID particle_rain = 19;
		const AssetID particle_spark = 20;
		const AssetID particle_standard = 21;
		const AssetID particle_textured = 22;
		const AssetID point_light = 23;
		const AssetID scan_lines = 24;
		const AssetID sky_decal = 25;
		const AssetID skybox = 26;
		const AssetID spot_light = 27;
		const AssetID ssao = 28;
		const AssetID ssao_blur = 29;
		const AssetID ssao_downsample = 30;
		const AssetID standard = 31;
		const AssetID standard_flat = 32;
		const AssetID standard_instanced = 33;
		const AssetID stencil_back_faces = 34;
		const AssetID ui = 35;
		const AssetID ui_texture = 36;
		const AssetID underwater = 37;
		const AssetID water = 38;
	}
}

}