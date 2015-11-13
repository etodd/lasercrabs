#include "lookup.h"

namespace VI
{ 


const char* AssetLookup::Mesh::values[] =
{
	"assets/Alpha.msh",
	"assets/awk.msh",
	"assets/compass_inner.msh",
	"assets/compass_outer.msh",
	"assets/cone.msh",
	"assets/cube.msh",
	"assets/icon_power.msh",
	"assets/logo.msh",
	"assets/msg.msh",
	"assets/point_grid.msh",
	"assets/skybox.msh",
	"assets/socket.msh",
	"assets/sphere.msh",
	"assets/tri_tube.msh",
	"assets/lvl/level1_Plane.msh",
	"assets/lvl/level1_city2.msh",
	"assets/lvl/level1_city2_001.msh",
	"assets/lvl/level1_city2_002.msh",
	"assets/lvl/level1_main.msh",
	"assets/lvl/level1_main_001.msh",
	"assets/lvl/level1_main_001_1.msh",
	"assets/lvl/level1_main_1.msh",
	"assets/lvl/level1_scenery1.msh",
	"assets/lvl/level1_scenery2.msh",
	"assets/lvl/level2_Plane_001.msh",
	"assets/lvl/level2_door.msh",
	"assets/lvl/level2_door_001.msh",
	"assets/lvl/level2_door_002.msh",
	"assets/lvl/level2_env_000.msh",
	"assets/lvl/level2_env_001.msh",
	"assets/lvl/level2_env_001_1.msh",
	"assets/lvl/level2_env_002.msh",
	"assets/lvl/level2_env_002_1.msh",
	"assets/lvl/level3_city2_door.msh",
	"assets/lvl/level3_city2_scenery.msh",
	"assets/lvl/level3_half1.msh",
	"assets/lvl/level3_half1_1.msh",
	"assets/lvl/level3_half2.msh",
	"assets/lvl/level3_half2_1.msh",
	"assets/lvl/level3_half3.msh",
	"assets/lvl/level3_half3_1.msh",
	"assets/lvl/level4_shell.msh",
	"assets/lvl/level4_shell_1.msh",
	"assets/lvl/pvp_pvp_half1.msh",
	"assets/lvl/pvp_pvp_half1_1.msh",
	"assets/lvl/pvp_pvp_half2.msh",
	"assets/lvl/pvp_pvp_half2_1.msh",
	"assets/lvl/pvp_pvp_scenery.msh",
	"assets/lvl/title_city2_scenery.msh",
	"assets/lvl/title_half1.msh",
	"assets/lvl/title_half1_1.msh",
	"assets/lvl/title_half2.msh",
	"assets/lvl/title_half2_1.msh",
	"assets/lvl/title_half3.msh",
	"assets/lvl/title_half3_1.msh",
	0,
};


const char* AssetLookup::Mesh::names[] =
{
	"Alpha",
	"awk",
	"compass_inner",
	"compass_outer",
	"cone",
	"cube",
	"icon_power",
	"logo",
	"msg",
	"point_grid",
	"skybox",
	"socket",
	"sphere",
	"tri_tube",
	"level1_Plane",
	"level1_city2",
	"level1_city2_001",
	"level1_city2_002",
	"level1_main",
	"level1_main_001",
	"level1_main_001_1",
	"level1_main_1",
	"level1_scenery1",
	"level1_scenery2",
	"level2_Plane_001",
	"level2_door",
	"level2_door_001",
	"level2_door_002",
	"level2_env_000",
	"level2_env_001",
	"level2_env_001_1",
	"level2_env_002",
	"level2_env_002_1",
	"level3_city2_door",
	"level3_city2_scenery",
	"level3_half1",
	"level3_half1_1",
	"level3_half2",
	"level3_half2_1",
	"level3_half3",
	"level3_half3_1",
	"level4_shell",
	"level4_shell_1",
	"pvp_pvp_half1",
	"pvp_pvp_half1_1",
	"pvp_pvp_half2",
	"pvp_pvp_half2_1",
	"pvp_pvp_scenery",
	"title_city2_scenery",
	"title_half1",
	"title_half1_1",
	"title_half2",
	"title_half2_1",
	"title_half3",
	"title_half3_1",
	0,
};


const char* AssetLookup::Animation::values[] =
{
	"assets/fly.anm",
	"assets/idle.anm",
	"assets/run.anm",
	"assets/walk.anm",
	0,
};


const char* AssetLookup::Animation::names[] =
{
	"fly",
	"idle",
	"run",
	"walk",
	0,
};


const char* AssetLookup::Armature::values[] =
{
	"assets/Alpha.arm",
	"assets/awk.arm",
	0,
};


const char* AssetLookup::Armature::names[] =
{
	"Alpha",
	"awk",
	0,
};


const char* AssetLookup::Texture::values[] =
{
	"assets/blank.png",
	"assets/gradient.png",
	"assets/noise.png",
	"assets/skybox_horizon.png",
	0,
};


const char* AssetLookup::Texture::names[] =
{
	"blank",
	"gradient",
	"noise",
	"skybox_horizon",
	0,
};


const char* AssetLookup::Soundbank::values[] =
{
	"assets/Init.bnk",
	"assets/SOUNDBANK.bnk",
	0,
};


const char* AssetLookup::Soundbank::names[] =
{
	"Init",
	"SOUNDBANK",
	0,
};


const char* AssetLookup::Shader::values[] =
{
	"assets/armature.glsl",
	"assets/bloom_downsample.glsl",
	"assets/blur.glsl",
	"assets/composite.glsl",
	"assets/debug_depth.glsl",
	"assets/edge_detect.glsl",
	"assets/flat.glsl",
	"assets/flat_texture.glsl",
	"assets/global_light.glsl",
	"assets/point_grid.glsl",
	"assets/point_light.glsl",
	"assets/skybox.glsl",
	"assets/spot_light.glsl",
	"assets/ssao.glsl",
	"assets/ssao_blur.glsl",
	"assets/ssao_downsample.glsl",
	"assets/standard.glsl",
	"assets/standard_instanced.glsl",
	"assets/ui.glsl",
	"assets/ui_texture.glsl",
	0,
};


const char* AssetLookup::Shader::names[] =
{
	"armature",
	"bloom_downsample",
	"blur",
	"composite",
	"debug_depth",
	"edge_detect",
	"flat",
	"flat_texture",
	"global_light",
	"point_grid",
	"point_light",
	"skybox",
	"spot_light",
	"ssao",
	"ssao_blur",
	"ssao_downsample",
	"standard",
	"standard_instanced",
	"ui",
	"ui_texture",
	0,
};


const char* AssetLookup::Uniform::values[] =
{
	"ambient_color",
	"bones",
	"color_buffer",
	"depth_buffer",
	"detail_light_vp",
	"detail_shadow_map",
	"diffuse_color",
	"diffuse_map",
	"far_plane",
	"film_grain_size",
	"frustum",
	"inv_buffer_size",
	"inv_uv_scale",
	"light_color",
	"light_direction",
	"light_fov_dot",
	"light_pos",
	"light_radius",
	"light_vp",
	"lighting_buffer",
	"mv",
	"mvp",
	"noise_sampler",
	"normal_buffer",
	"p",
	"shadow_map",
	"shockwave",
	"ssao_buffer",
	"uv_offset",
	"uv_scale",
	"v",
	"vp",
	"zenith_color",
	0,
};


const char* AssetLookup::Uniform::names[] =
{
	"ambient_color",
	"bones",
	"color_buffer",
	"depth_buffer",
	"detail_light_vp",
	"detail_shadow_map",
	"diffuse_color",
	"diffuse_map",
	"far_plane",
	"film_grain_size",
	"frustum",
	"inv_buffer_size",
	"inv_uv_scale",
	"light_color",
	"light_direction",
	"light_fov_dot",
	"light_pos",
	"light_radius",
	"light_vp",
	"lighting_buffer",
	"mv",
	"mvp",
	"noise_sampler",
	"normal_buffer",
	"p",
	"shadow_map",
	"shockwave",
	"ssao_buffer",
	"uv_offset",
	"uv_scale",
	"v",
	"vp",
	"zenith_color",
	0,
};


const char* AssetLookup::Font::values[] =
{
	"assets/lowpoly.fnt",
	0,
};


const char* AssetLookup::Font::names[] =
{
	"lowpoly",
	0,
};


const char* AssetLookup::Level::values[] =
{
	"assets/lvl/connect.lvl",
	"assets/lvl/end.lvl",
	"assets/lvl/game_over.lvl",
	"assets/lvl/level1.lvl",
	"assets/lvl/level2.lvl",
	"assets/lvl/level3.lvl",
	"assets/lvl/level4.lvl",
	"assets/lvl/menu.lvl",
	"assets/lvl/pvp.lvl",
	"assets/lvl/title.lvl",
	0,
};


const char* AssetLookup::Level::names[] =
{
	"connect",
	"end",
	"game_over",
	"level1",
	"level2",
	"level3",
	"level4",
	"menu",
	"pvp",
	"title",
	0,
};


const char* AssetLookup::NavMesh::values[] =
{
	"assets/lvl/connect.nav",
	"assets/lvl/end.nav",
	"assets/lvl/game_over.nav",
	"assets/lvl/level1.nav",
	"assets/lvl/level2.nav",
	"assets/lvl/level3.nav",
	"assets/lvl/level4.nav",
	"assets/lvl/menu.nav",
	"assets/lvl/pvp.nav",
	"assets/lvl/title.nav",
	0,
};


const char* AssetLookup::NavMesh::names[] =
{
	"connect",
	"end",
	"game_over",
	"level1",
	"level2",
	"level3",
	"level4",
	"menu",
	"pvp",
	"title",
	0,
};


}