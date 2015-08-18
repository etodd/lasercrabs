#include "animation.h"
#include "armature.h"
#include "font.h"
#include "mesh.h"
#include "shader.h"
#include "lookup.h"

namespace VI
{ 


const char* Asset::Mesh::values[] =
{
	"assets/Alpha.msh",
	"assets/city1.msh",
	"assets/city2.msh",
	"assets/city3.msh",
	"assets/city4_elevator.msh",
	"assets/city4_shell.msh",
	"assets/city4_shell_1.msh",
	"assets/city4_test.msh",
	"assets/cube.msh",
	"assets/skybox.msh",
};


const char* Asset::Animation::values[] =
{
	"assets/walk.anm",
};


const char* Asset::Armature::values[] =
{
	"assets/Alpha.arm",
	"assets/city1.arm",
	"assets/city2.arm",
	"assets/city3.arm",
	"assets/city4.arm",
	"assets/cube.arm",
	"assets/skybox.arm",
};


const char* Asset::Texture::values[] =
{
	"assets/skybox_horizon.png",
	"assets/test.png",
};


const char* Asset::Shader::values[] =
{
	"assets/Armature.glsl",
	"assets/Standard.glsl",
	"assets/UI.glsl",
	"assets/flat_texture.glsl",
};


const char* Asset::Uniform::values[] =
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


const char* Asset::Font::values[] =
{
	"assets/SegoeUISymbol.fnt",
	"assets/lowpoly.fnt",
};


}