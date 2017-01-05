#pragma once
#include "types.h"

namespace VI
{

namespace Asset
{
	namespace Uniform
	{
		const s32 count = 51;
		const AssetID ambient_color = 0;
		const AssetID bones = 1;
		const AssetID buffer_size = 2;
		const AssetID color_buffer = 3;
		const AssetID cull_behind_wall = 4;
		const AssetID cull_center = 5;
		const AssetID cull_radius = 6;
		const AssetID depth_buffer = 7;
		const AssetID detail_light_vp = 8;
		const AssetID detail_shadow_map = 9;
		const AssetID diffuse_color = 10;
		const AssetID diffuse_map = 11;
		const AssetID displacement = 12;
		const AssetID far_plane = 13;
		const AssetID fog = 14;
		const AssetID fog_extent = 15;
		const AssetID fog_start = 16;
		const AssetID frustum = 17;
		const AssetID gravity = 18;
		const AssetID inv_buffer_size = 19;
		const AssetID inv_uv_scale = 20;
		const AssetID lifetime = 21;
		const AssetID light_color = 22;
		const AssetID light_direction = 23;
		const AssetID light_fov_dot = 24;
		const AssetID light_pos = 25;
		const AssetID light_radius = 26;
		const AssetID light_vp = 27;
		const AssetID lighting_buffer = 28;
		const AssetID mv = 29;
		const AssetID mvp = 30;
		const AssetID noise_sampler = 31;
		const AssetID normal_buffer = 32;
		const AssetID normal_map = 33;
		const AssetID p = 34;
		const AssetID plane = 35;
		const AssetID player_light = 36;
		const AssetID range = 37;
		const AssetID range_center = 38;
		const AssetID scan_line_interval = 39;
		const AssetID shadow_map = 40;
		const AssetID size = 41;
		const AssetID ssao_buffer = 42;
		const AssetID time = 43;
		const AssetID type = 44;
		const AssetID uv_offset = 45;
		const AssetID uv_scale = 46;
		const AssetID v = 47;
		const AssetID viewport_scale = 48;
		const AssetID vp = 49;
		const AssetID wall_normal = 50;
	}
	namespace Shader
	{
		const s32 count = 34;
		const AssetID armature = 0;
		const AssetID blit = 1;
		const AssetID blit_depth = 2;
		const AssetID bloom_downsample = 3;
		const AssetID blur = 4;
		const AssetID composite = 5;
		const AssetID culled = 6;
		const AssetID debug_depth = 7;
		const AssetID downsample = 8;
		const AssetID flat = 9;
		const AssetID flat_clipped = 10;
		const AssetID flat_instanced = 11;
		const AssetID flat_texture = 12;
		const AssetID fresnel = 13;
		const AssetID global_light = 14;
		const AssetID particle_eased = 15;
		const AssetID particle_spark = 16;
		const AssetID particle_standard = 17;
		const AssetID particle_textured = 18;
		const AssetID point_light = 19;
		const AssetID scan_lines = 20;
		const AssetID sky_decal = 21;
		const AssetID skybox = 22;
		const AssetID spot_light = 23;
		const AssetID ssao = 24;
		const AssetID ssao_blur = 25;
		const AssetID ssao_downsample = 26;
		const AssetID standard = 27;
		const AssetID standard_flat = 28;
		const AssetID standard_instanced = 29;
		const AssetID ui = 30;
		const AssetID ui_texture = 31;
		const AssetID underwater = 32;
		const AssetID water = 33;
	}
}

}