#include "asset.h"

const char* Asset::Model::filenames[] =
{
	"assets/Alpha.mdl",
	"assets/city3.mdl",
};
const char* Asset::Texture::filenames[] =
{
	"assets/test.png",
};
const char* Asset::Shader::filenames[] =
{
	"assets/Armature.glsl",
	"assets/Standard.glsl",
};
const char* Asset::Animation::filenames[] =
{
	"assets/idle.anm",
	"assets/jump.anm",
	"assets/run.anm",
	"assets/strafe_left.anm",
	"assets/strafe_right.anm",
	"assets/turn_left.anm",
	"assets/turn_right.anm",
	"assets/walk.anm",
};
const char* Asset::Uniform::filenames[] =
{
	"LightPosition_worldspace",
	"M",
	"MVP",
	"V",
	"myTextureSampler",
};
