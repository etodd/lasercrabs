#include "lookup.h"

namespace VI
{ 


const char* AssetLookup::Mesh::values[] =
{
	"assets/Alpha.msh",
	"assets/Alpha_headless.msh",
	"assets/awk.msh",
	"assets/compass_inner.msh",
	"assets/compass_outer.msh",
	"assets/cone.msh",
	"assets/cube.msh",
	"assets/icon_power.msh",
	"assets/msg.msh",
	"assets/point_grid.msh",
	"assets/sky_decal.msh",
	"assets/skybox.msh",
	"assets/socket.msh",
	"assets/sphere.msh",
	"assets/target.msh",
	"assets/tri_tube.msh",
	"assets/vision_cone.msh",
	"assets/lvl/level1_Plane.msh",
	"assets/lvl/level1_city2_002.msh",
	"assets/lvl/level1_main.msh",
	"assets/lvl/level1_main_001.msh",
	"assets/lvl/level1_main_001_1.msh",
	"assets/lvl/level1_main_002.msh",
	"assets/lvl/level1_main_002_1.msh",
	"assets/lvl/level1_main_1.msh",
	"assets/lvl/level1_scenery2_000.msh",
	"assets/lvl/level1_scenery2_001.msh",
	"assets/lvl/level1_scenery2_001_1.msh",
	"assets/lvl/level1_scenery2_002.msh",
	"assets/lvl/level1_scenery2_002_1.msh",
	"assets/lvl/level1_scenery2_003.msh",
	"assets/lvl/level1_scenery2_003_1.msh",
	"assets/lvl/level1_scenery2_004.msh",
	"assets/lvl/level1_scenery2_004_1.msh",
	"assets/lvl/level1_scenery2_005.msh",
	"assets/lvl/level1_scenery2_005_1.msh",
	"assets/lvl/level1_scenery2_006.msh",
	"assets/lvl/level1_scenery2_007.msh",
	"assets/lvl/level1_scenery2_008.msh",
	"assets/lvl/level2_Circle_000.msh",
	"assets/lvl/level2_Circle_001.msh",
	"assets/lvl/level2_Circle_002.msh",
	"assets/lvl/level2_Circle_003.msh",
	"assets/lvl/level2_Plane.msh",
	"assets/lvl/level2_Plane_001.msh",
	"assets/lvl/level2_city2_000.msh",
	"assets/lvl/level2_city2_001.msh",
	"assets/lvl/level2_city2_001_1.msh",
	"assets/lvl/level2_city2_002.msh",
	"assets/lvl/level2_city2_003.msh",
	"assets/lvl/level2_city2_087.msh",
	"assets/lvl/level2_door.msh",
	"assets/lvl/level2_exit.msh",
	"assets/lvl/level2_main_001.msh",
	"assets/lvl/level2_main_001_1.msh",
	"assets/lvl/level2_scenery2_000.msh",
	"assets/lvl/level2_scenery2_000_1.msh",
	"assets/lvl/level2_scenery2_001.msh",
	"assets/lvl/level2_scenery2_001_1.msh",
	"assets/lvl/level2_scenery2_002.msh",
	"assets/lvl/level2_scenery2_002_1.msh",
	"assets/lvl/level2_scenery2_003.msh",
	"assets/lvl/level2_scenery2_003_1.msh",
	"assets/lvl/level2_scenery2_004.msh",
	"assets/lvl/level2_scenery2_004_1.msh",
	"assets/lvl/level2_scenery2_005.msh",
	"assets/lvl/level2_scenery2_005_1.msh",
	"assets/lvl/level2_scenery2_006.msh",
	"assets/lvl/level2_scenery2_006_1.msh",
	"assets/lvl/level2_scenery2_007.msh",
	"assets/lvl/level2_scenery2_007_1.msh",
	"assets/lvl/level2_scenery2_008.msh",
	"assets/lvl/level2_scenery2_008_1.msh",
	"assets/lvl/level3_Plane_001.msh",
	"assets/lvl/level3_door.msh",
	"assets/lvl/level3_door_001.msh",
	"assets/lvl/level3_door_002.msh",
	"assets/lvl/level3_env_000.msh",
	"assets/lvl/level3_env_001.msh",
	"assets/lvl/level3_env_001_1.msh",
	"assets/lvl/level3_env_002.msh",
	"assets/lvl/level3_env_002_1.msh",
	"assets/lvl/level4_city2_door.msh",
	"assets/lvl/level4_city2_scenery.msh",
	"assets/lvl/level4_half1.msh",
	"assets/lvl/level4_half1_1.msh",
	"assets/lvl/level4_half2.msh",
	"assets/lvl/level4_half2_1.msh",
	"assets/lvl/level4_half3.msh",
	"assets/lvl/level4_half3_1.msh",
	"assets/lvl/level5_shell.msh",
	"assets/lvl/level5_shell_1.msh",
	"assets/lvl/pvp_city2_door.msh",
	"assets/lvl/pvp_city2_scenery.msh",
	"assets/lvl/pvp_half1.msh",
	"assets/lvl/pvp_half1_1.msh",
	"assets/lvl/pvp_half2.msh",
	"assets/lvl/pvp_half2_1.msh",
	"assets/lvl/pvp_half3.msh",
	"assets/lvl/pvp_half3_1.msh",
	"assets/lvl/test_city2_scenery.msh",
	"assets/lvl/test_half1.msh",
	"assets/lvl/test_half1_1.msh",
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
	"Alpha_headless",
	"awk",
	"compass_inner",
	"compass_outer",
	"cone",
	"cube",
	"icon_power",
	"msg",
	"point_grid",
	"sky_decal",
	"skybox",
	"socket",
	"sphere",
	"target",
	"tri_tube",
	"vision_cone",
	"level1_Plane",
	"level1_city2_002",
	"level1_main",
	"level1_main_001",
	"level1_main_001_1",
	"level1_main_002",
	"level1_main_002_1",
	"level1_main_1",
	"level1_scenery2_000",
	"level1_scenery2_001",
	"level1_scenery2_001_1",
	"level1_scenery2_002",
	"level1_scenery2_002_1",
	"level1_scenery2_003",
	"level1_scenery2_003_1",
	"level1_scenery2_004",
	"level1_scenery2_004_1",
	"level1_scenery2_005",
	"level1_scenery2_005_1",
	"level1_scenery2_006",
	"level1_scenery2_007",
	"level1_scenery2_008",
	"level2_Circle_000",
	"level2_Circle_001",
	"level2_Circle_002",
	"level2_Circle_003",
	"level2_Plane",
	"level2_Plane_001",
	"level2_city2_000",
	"level2_city2_001",
	"level2_city2_001_1",
	"level2_city2_002",
	"level2_city2_003",
	"level2_city2_087",
	"level2_door",
	"level2_exit",
	"level2_main_001",
	"level2_main_001_1",
	"level2_scenery2_000",
	"level2_scenery2_000_1",
	"level2_scenery2_001",
	"level2_scenery2_001_1",
	"level2_scenery2_002",
	"level2_scenery2_002_1",
	"level2_scenery2_003",
	"level2_scenery2_003_1",
	"level2_scenery2_004",
	"level2_scenery2_004_1",
	"level2_scenery2_005",
	"level2_scenery2_005_1",
	"level2_scenery2_006",
	"level2_scenery2_006_1",
	"level2_scenery2_007",
	"level2_scenery2_007_1",
	"level2_scenery2_008",
	"level2_scenery2_008_1",
	"level3_Plane_001",
	"level3_door",
	"level3_door_001",
	"level3_door_002",
	"level3_env_000",
	"level3_env_001",
	"level3_env_001_1",
	"level3_env_002",
	"level3_env_002_1",
	"level4_city2_door",
	"level4_city2_scenery",
	"level4_half1",
	"level4_half1_1",
	"level4_half2",
	"level4_half2_1",
	"level4_half3",
	"level4_half3_1",
	"level5_shell",
	"level5_shell_1",
	"pvp_city2_door",
	"pvp_city2_scenery",
	"pvp_half1",
	"pvp_half1_1",
	"pvp_half2",
	"pvp_half2_1",
	"pvp_half3",
	"pvp_half3_1",
	"test_city2_scenery",
	"test_half1",
	"test_half1_1",
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
	"assets/Alpha_headless.arm",
	"assets/awk.arm",
	0,
};


const char* AssetLookup::Armature::names[] =
{
	"Alpha",
	"Alpha_headless",
	"awk",
	0,
};


const char* AssetLookup::Texture::values[] =
{
	"assets/blank.png",
	"assets/flare.png",
	"assets/gradient.png",
	"assets/noise.png",
	"assets/skybox_horizon.png",
	"assets/skybox_horizon2.png",
	0,
};


const char* AssetLookup::Texture::names[] =
{
	"blank",
	"flare",
	"gradient",
	"noise",
	"skybox_horizon",
	"skybox_horizon2",
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
	"ssao_buffer",
	"type",
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
	"ssao_buffer",
	"type",
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
	"assets/lvl/level5.lvl",
	"assets/lvl/menu.lvl",
	"assets/lvl/pvp.lvl",
	"assets/lvl/start.lvl",
	"assets/lvl/test.lvl",
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
	"level5",
	"menu",
	"pvp",
	"start",
	"test",
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
	"assets/lvl/level5.nav",
	"assets/lvl/menu.nav",
	"assets/lvl/pvp.nav",
	"assets/lvl/start.nav",
	"assets/lvl/test.nav",
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
	"level5",
	"menu",
	"pvp",
	"start",
	"test",
	"title",
	0,
};


}