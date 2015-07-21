#include "asset.h"

AssetID const Asset::Model::Alpha = 0;
AssetID const Asset::Model::city3 = 1;
AssetID const Asset::Model::cube = 2;

const char* Asset::Model::filenames[] =
{
	"assets/Alpha.mdl",
	"assets/city3.mdl",
	"assets/cube.mdl",
};

AssetID const Asset::Texture::test = 0;

const char* Asset::Texture::filenames[] =
{
	"assets/test.png",
};

AssetID const Asset::Shader::Armature = 0;
AssetID const Asset::Shader::Standard = 1;

const char* Asset::Shader::filenames[] =
{
	"assets/Armature.glsl",
	"assets/Standard.glsl",
};

AssetID const Asset::Animation::idle = 0;
AssetID const Asset::Animation::jump = 1;
AssetID const Asset::Animation::run = 2;
AssetID const Asset::Animation::strafe_left = 3;
AssetID const Asset::Animation::strafe_right = 4;
AssetID const Asset::Animation::turn_left = 5;
AssetID const Asset::Animation::turn_right = 6;
AssetID const Asset::Animation::walk = 7;

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

AssetID const Asset::Uniform::Bones = 0;
AssetID const Asset::Uniform::LightPosition_worldspace = 1;
AssetID const Asset::Uniform::M = 2;
AssetID const Asset::Uniform::MVP = 3;
AssetID const Asset::Uniform::V = 4;
AssetID const Asset::Uniform::myTextureSampler = 5;

const char* Asset::Uniform::filenames[] =
{
	"Bones",
	"LightPosition_worldspace",
	"M",
	"MVP",
	"V",
	"myTextureSampler",
};

