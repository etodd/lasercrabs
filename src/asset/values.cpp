#include "lookup.h"

namespace VI
{ 


const char* AssetLookup::Mesh::values[] =
{
	"assets/Alpha.msh",
	"assets/awk.msh",
	"assets/lvl/city1.msh",
	"assets/lvl/city2.msh",
	"assets/lvl/city3_city3.msh",
	"assets/lvl/city3_city3_1.msh",
	"assets/lvl/city4_elevator.msh",
	"assets/lvl/city4_shell.msh",
	"assets/lvl/city4_shell_1.msh",
	"assets/cone.msh",
	"assets/cube.msh",
	"assets/skybox.msh",
	"assets/sphere.msh",
	0,
};


const char* AssetLookup::Mesh::names[] =
{
	"Alpha",
	"awk",
	"city1",
	"city2",
	"city3_city3",
	"city3_city3_1",
	"city4_elevator",
	"city4_shell",
	"city4_shell_1",
	"cone",
	"cube",
	"skybox",
	"sphere",
	0,
};


const char* AssetLookup::Animation::values[] =
{
	"assets/idle.anm",
	"assets/run.anm",
	"assets/walk.anm",
	0,
};


const char* AssetLookup::Animation::names[] =
{
	"idle",
	"run",
	"walk",
	0,
};


const char* AssetLookup::Armature::values[] =
{
	"assets/Alpha.arm",
	0,
};


const char* AssetLookup::Armature::names[] =
{
	"Alpha",
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
	"assets/edge_detect.glsl",
	"assets/flat.glsl",
	"assets/flat_texture.glsl",
	"assets/point_light.glsl",
	"assets/skybox.glsl",
	"assets/spot_light.glsl",
	"assets/ssao.glsl",
	"assets/ssao_blur.glsl",
	"assets/ssao_downsample.glsl",
	"assets/standard.glsl",
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
	"edge_detect",
	"flat",
	"flat_texture",
	"point_light",
	"skybox",
	"spot_light",
	"ssao",
	"ssao_blur",
	"ssao_downsample",
	"standard",
	"ui",
	"ui_texture",
	0,
};


const char* AssetLookup::Uniform::values[] =
{
	"ambient_color",
	"bones",
	"buffer_size",
	"color_buffer",
	"depth_buffer",
	"diffuse_color",
	"diffuse_map",
	"far_plane",
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
	"uv_offset",
	"uv_scale",
	0,
};


const char* AssetLookup::Uniform::names[] =
{
	"ambient_color",
	"bones",
	"buffer_size",
	"color_buffer",
	"depth_buffer",
	"diffuse_color",
	"diffuse_map",
	"far_plane",
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
	"uv_offset",
	"uv_scale",
	0,
};


const char* AssetLookup::Font::values[] =
{
	"assets/SegoeUISymbol.fnt",
	"assets/lowpoly.fnt",
	0,
};


const char* AssetLookup::Font::names[] =
{
	"SegoeUISymbol",
	"lowpoly",
	0,
};


const char* AssetLookup::Level::values[] =
{
	"assets/lvl/city1.lvl",
	"assets/lvl/city2.lvl",
	"assets/lvl/city3.lvl",
	"assets/lvl/city4.lvl",
	0,
};


const char* AssetLookup::Level::names[] =
{
	"city1",
	"city2",
	"city3",
	"city4",
	0,
};


const char* AssetLookup::NavMesh::values[] =
{
	"assets/lvl/city1.nav",
	"assets/lvl/city2.nav",
	"assets/lvl/city3.nav",
	"assets/lvl/city4.nav",
	0,
};


const char* AssetLookup::NavMesh::names[] =
{
	"city1",
	"city2",
	"city3",
	"city4",
	0,
};


}