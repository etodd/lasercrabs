#pragma once
#include "types.h"

namespace VI
{

namespace Asset
{
	namespace Uniform
	{
		const s32 count = 48;
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
		const AssetID far_plane = 12;
		const AssetID fog = 13;
		const AssetID fog_extent = 14;
		const AssetID fog_start = 15;
		const AssetID frustum = 16;
		const AssetID gravity = 17;
		const AssetID inv_buffer_size = 18;
		const AssetID inv_uv_scale = 19;
		const AssetID lifetime = 20;
		const AssetID light_color = 21;
		const AssetID light_direction = 22;
		const AssetID light_fov_dot = 23;
		const AssetID light_pos = 24;
		const AssetID light_radius = 25;
		const AssetID light_vp = 26;
		const AssetID lighting_buffer = 27;
		const AssetID mv = 28;
		const AssetID mvp = 29;
		const AssetID noise_sampler = 30;
		const AssetID normal_buffer = 31;
		const AssetID normal_map = 32;
		const AssetID p = 33;
		const AssetID player_light = 34;
		const AssetID range = 35;
		const AssetID range_center = 36;
		const AssetID shadow_map = 37;
		const AssetID size = 38;
		const AssetID ssao_buffer = 39;
		const AssetID time = 40;
		const AssetID type = 41;
		const AssetID uv_offset = 42;
		const AssetID uv_scale = 43;
		const AssetID v = 44;
		const AssetID viewport_scale = 45;
		const AssetID vp = 46;
		const AssetID wall_normal = 47;
	}
	namespace Shader
	{
		const s32 count = 27;
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
		const AssetID particle_eased = 10;
		const AssetID particle_spark = 11;
		const AssetID particle_standard = 12;
		const AssetID particle_textured = 13;
		const AssetID point_light = 14;
		const AssetID scan_lines = 15;
		const AssetID sky_decal = 16;
		const AssetID skybox = 17;
		const AssetID spot_light = 18;
		const AssetID ssao = 19;
		const AssetID ssao_blur = 20;
		const AssetID ssao_downsample = 21;
		const AssetID standard = 22;
		const AssetID standard_instanced = 23;
		const AssetID ui = 24;
		const AssetID ui_texture = 25;
		const AssetID water = 26;
	}
}

}