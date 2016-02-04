#include "lookup.h"

namespace VI
{ 


const char* AssetLookup::Mesh::values[] =
{
	"assets/awk.msh",
	"assets/character_gun.msh",
	"assets/character_headless.msh",
	"assets/character_mesh.msh",
	"assets/compass_inner.msh",
	"assets/compass_outer.msh",
	"assets/cone.msh",
	"assets/cube.msh",
	"assets/cursor.msh",
	"assets/logo.msh",
	"assets/sky_decal.msh",
	"assets/skybox.msh",
	"assets/socket.msh",
	"assets/soren_case.msh",
	"assets/soren_rim.msh",
	"assets/soren_screen.msh",
	"assets/spawn.msh",
	"assets/sphere.msh",
	"assets/target.msh",
	"assets/tri_tube.msh",
	"assets/turret.msh",
	"assets/vision_cone.msh",
	"assets/lvl/containment_09_Cube.msh",
	"assets/lvl/containment_09_Cube_001.msh",
	"assets/lvl/containment_09_Cube_001_1.msh",
	"assets/lvl/containment_09_Cube_002.msh",
	"assets/lvl/containment_09_Cube_002_1.msh",
	"assets/lvl/containment_09_Cube_003.msh",
	"assets/lvl/containment_09_Cube_003_1.msh",
	"assets/lvl/containment_09_Cube_004.msh",
	"assets/lvl/containment_09_Cube_004_1.msh",
	"assets/lvl/containment_09_Cube_005.msh",
	"assets/lvl/containment_09_Cube_005_1.msh",
	"assets/lvl/containment_09_Cube_1.msh",
	"assets/lvl/containment_09_door.msh",
	"assets/lvl/test_Cube.msh",
	"assets/lvl/test_Cube_1.msh",
	"assets/lvl/test_scenery.msh",
	0,
};


const char* AssetLookup::Mesh::names[] =
{
	"awk",
	"character_gun",
	"character_headless",
	"character_mesh",
	"compass_inner",
	"compass_outer",
	"cone",
	"cube",
	"cursor",
	"logo",
	"sky_decal",
	"skybox",
	"socket",
	"soren_case",
	"soren_rim",
	"soren_screen",
	"spawn",
	"sphere",
	"target",
	"tri_tube",
	"turret",
	"vision_cone",
	"containment_09_Cube",
	"containment_09_Cube_001",
	"containment_09_Cube_001_1",
	"containment_09_Cube_002",
	"containment_09_Cube_002_1",
	"containment_09_Cube_003",
	"containment_09_Cube_003_1",
	"containment_09_Cube_004",
	"containment_09_Cube_004_1",
	"containment_09_Cube_005",
	"containment_09_Cube_005_1",
	"containment_09_Cube_1",
	"containment_09_door",
	"test_Cube",
	"test_Cube_1",
	"test_scenery",
	0,
};


const char* AssetLookup::Animation::values[] =
{
	"assets/awk_fly.anm",
	"assets/character_idle.anm",
	"assets/character_idle_gun.anm",
	"assets/character_mantle.anm",
	"assets/character_run.anm",
	"assets/character_walk.anm",
	0,
};


const char* AssetLookup::Animation::names[] =
{
	"awk_fly",
	"character_idle",
	"character_idle_gun",
	"character_mantle",
	"character_run",
	"character_walk",
	0,
};


const char* AssetLookup::Armature::values[] =
{
	"assets/awk.arm",
	"assets/character_headless.arm",
	"assets/character_mesh.arm",
	"assets/turret.arm",
	0,
};


const char* AssetLookup::Armature::names[] =
{
	"awk",
	"character_headless",
	"character_mesh",
	"turret",
	0,
};


const char* AssetLookup::Texture::values[] =
{
	"assets/blank.png",
	"assets/flare.png",
	"assets/gradient.png",
	"assets/noise.png",
	"assets/skybox_horizon.png",
	"assets/smoke.png",
	"assets/soren.png",
	0,
};


const char* AssetLookup::Texture::names[] =
{
	"blank",
	"flare",
	"gradient",
	"noise",
	"skybox_horizon",
	"smoke",
	"soren",
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
	"assets/point_light.glsl",
	"assets/sky_decal.glsl",
	"assets/skybox.glsl",
	"assets/spark.glsl",
	"assets/spot_light.glsl",
	"assets/ssao.glsl",
	"assets/ssao_blur.glsl",
	"assets/ssao_downsample.glsl",
	"assets/standard.glsl",
	"assets/standard_instanced.glsl",
	"assets/standard_particle.glsl",
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
	"point_light",
	"sky_decal",
	"skybox",
	"spark",
	"spot_light",
	"ssao",
	"ssao_blur",
	"ssao_downsample",
	"standard",
	"standard_instanced",
	"standard_particle",
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
	"fog",
	"fog_extent",
	"fog_start",
	"frustum",
	"gravity",
	"inv_buffer_size",
	"inv_uv_scale",
	"lifetime",
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
	"range",
	"shadow_map",
	"size",
	"ssao_buffer",
	"time",
	"type",
	"uv_offset",
	"uv_scale",
	"v",
	"viewport_scale",
	"vp",
	"wall_normal",
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
	"fog",
	"fog_extent",
	"fog_start",
	"frustum",
	"gravity",
	"inv_buffer_size",
	"inv_uv_scale",
	"lifetime",
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
	"range",
	"shadow_map",
	"size",
	"ssao_buffer",
	"time",
	"type",
	"uv_offset",
	"uv_scale",
	"v",
	"viewport_scale",
	"vp",
	"wall_normal",
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
	"assets/lvl/containment_09.lvl",
	"assets/lvl/end.lvl",
	"assets/lvl/menu.lvl",
	"assets/lvl/start.lvl",
	"assets/lvl/test.lvl",
	"assets/lvl/title.lvl",
	0,
};


const char* AssetLookup::Level::names[] =
{
	"connect",
	"containment_09",
	"end",
	"menu",
	"start",
	"test",
	"title",
	0,
};


const char* AssetLookup::NavMesh::values[] =
{
	"assets/lvl/connect.nav",
	"assets/lvl/containment_09.nav",
	"assets/lvl/end.nav",
	"assets/lvl/menu.nav",
	"assets/lvl/start.nav",
	"assets/lvl/test.nav",
	"assets/lvl/title.nav",
	0,
};


const char* AssetLookup::NavMesh::names[] =
{
	"connect",
	"containment_09",
	"end",
	"menu",
	"start",
	"test",
	"title",
	0,
};


}