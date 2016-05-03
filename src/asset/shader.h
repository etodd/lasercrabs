#pragma once
#include "types.h"

namespace VI
{

namespace Asset
{
	namespace Uniform
	{
		const s32 count = 47;
		const AssetID ambient_color = 0;
		const AssetID bones = 1;
		const AssetID color_buffer = 2;
		const AssetID cull_center = 3;
		const AssetID cull_normal = 4;
		const AssetID cull_radius = 5;
		const AssetID depth_buffer = 6;
		const AssetID detail_light_vp = 7;
		const AssetID detail_shadow_map = 8;
		const AssetID diffuse_color = 9;
		const AssetID diffuse_map = 10;
		const AssetID far_plane = 11;
		const AssetID fog = 12;
		const AssetID fog_extent = 13;
		const AssetID fog_start = 14;
		const AssetID frustum = 15;
		const AssetID gravity = 16;
		const AssetID inv_buffer_size = 17;
		const AssetID inv_uv_scale = 18;
		const AssetID lifetime = 19;
		const AssetID light_color = 20;
		const AssetID light_direction = 21;
		const AssetID light_fov_dot = 22;
		const AssetID light_pos = 23;
		const AssetID light_radius = 24;
		const AssetID light_vp = 25;
		const AssetID lighting_buffer = 26;
		const AssetID mv = 27;
		const AssetID mvp = 28;
		const AssetID noise_sampler = 29;
		const AssetID normal_buffer = 30;
		const AssetID p = 31;
		const AssetID player_light = 32;
		const AssetID range = 33;
		const AssetID range_center = 34;
		const AssetID shadow_map = 35;
		const AssetID size = 36;
		const AssetID ssao_buffer = 37;
		const AssetID time = 38;
		const AssetID type = 39;
		const AssetID uv_offset = 40;
		const AssetID uv_scale = 41;
		const AssetID v = 42;
		const AssetID viewport_scale = 43;
		const AssetID vp = 44;
		const AssetID wall_normal = 45;
		const AssetID zenith_color = 46;
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
		const AssetID point_light = 10;
		const AssetID sky_decal = 11;
		const AssetID skybox = 12;
		const AssetID spark = 13;
		const AssetID spark_teleport = 14;
		const AssetID spot_light = 15;
		const AssetID ssao = 16;
		const AssetID ssao_blur = 17;
		const AssetID ssao_downsample = 18;
		const AssetID standard = 19;
		const AssetID standard_instanced = 20;
		const AssetID standard_particle = 21;
		const AssetID ui = 22;
		const AssetID ui_texture = 23;
	}
}

}