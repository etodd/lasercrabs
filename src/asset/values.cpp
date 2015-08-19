#include "animation.h"
#include "armature.h"
#include "font.h"
#include "mesh.h"
#include "shader.h"
#include "level.h"
#include "lookup.h"

namespace VI
{ 


const char* AssetLookup::Mesh::values[] =
{
	"assets/Alpha.msh",
	"assets/lvl/city1.msh",
	"assets/lvl/city2.msh",
	"assets/lvl/city3.msh",
	"assets/lvl/city4_elevator.msh",
	"assets/lvl/city4_shell.msh",
	"assets/lvl/city4_shell_1.msh",
	"assets/lvl/city4_test.msh",
	"assets/cube.msh",
	"assets/skybox.msh",
};


const char* AssetLookup::Animation::values[] =
{
	"assets/walk.anm",
};


const char* AssetLookup::Armature::values[] =
{
	"assets/Alpha.arm",
};


const char* AssetLookup::Texture::values[] =
{
	"assets/skybox_horizon.png",
	"assets/test.png",
};


const char* AssetLookup::Shader::values[] =
{
	"assets/Armature.glsl",
	"assets/Standard.glsl",
	"assets/UI.glsl",
	"assets/flat_texture.glsl",
};


const char* AssetLookup::Uniform::values[] =
{
	"ambient_color",
	"bones",
	"diffuse_color",
	"diffuse_map",
	"light_color",
	"light_position",
	"light_radius",
	"m",
	"mvp",
};


const char* AssetLookup::Font::values[] =
{
	"assets/SegoeUISymbol.fnt",
	"assets/lowpoly.fnt",
};


const char* AssetLookup::Level::values[] =
{
	"assets/lvl/city1.lvl",
	"assets/lvl/city2.lvl",
	"assets/lvl/city3.lvl",
	"assets/lvl/city4.lvl",
};


}