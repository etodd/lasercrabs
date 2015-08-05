#include "asset.h"

namespace VI
{

AssetID const Asset::Model::Alpha = 0;
AssetID const Asset::Model::city1 = 1;
AssetID const Asset::Model::city2 = 2;
AssetID const Asset::Model::city3 = 3;
AssetID const Asset::Model::city4 = 4;
AssetID const Asset::Model::city4_1 = 5;
AssetID const Asset::Model::cube = 6;
AssetID const Asset::Model::skybox = 7;

const char* Asset::Model::filenames[] =
{
	"assets/Alpha.mdl",
	"assets/city1.mdl",
	"assets/city2.mdl",
	"assets/city3.mdl",
	"assets/city4.mdl",
	"assets/city4_1.mdl",
	"assets/cube.mdl",
	"assets/skybox.mdl",
};

AssetID const Asset::Texture::skybox_horizon = 0;
AssetID const Asset::Texture::test = 1;

const char* Asset::Texture::filenames[] =
{
	"assets/skybox_horizon.png",
	"assets/test.png",
};

AssetID const Asset::Shader::Armature = 0;
AssetID const Asset::Shader::Standard = 1;
AssetID const Asset::Shader::UI = 2;
AssetID const Asset::Shader::flat_texture = 3;

const char* Asset::Shader::filenames[] =
{
	"assets/Armature.glsl",
	"assets/Standard.glsl",
	"assets/UI.glsl",
	"assets/flat_texture.glsl",
};

AssetID const Asset::Animation::walk = 0;

const char* Asset::Animation::filenames[] =
{
	"assets/walk.anm",
};

AssetID const Asset::Uniform::ambient_color = 0;
AssetID const Asset::Uniform::bones = 1;
AssetID const Asset::Uniform::diffuse_color = 2;
AssetID const Asset::Uniform::diffuse_map = 3;
AssetID const Asset::Uniform::light_color = 4;
AssetID const Asset::Uniform::light_position = 5;
AssetID const Asset::Uniform::light_radius = 6;
AssetID const Asset::Uniform::m = 7;
AssetID const Asset::Uniform::mvp = 8;

const char* Asset::Uniform::filenames[] =
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

AssetID const Asset::Font::SegoeUISymbol = 0;
AssetID const Asset::Font::lowpoly = 1;

const char* Asset::Font::filenames[] =
{
	"assets/SegoeUISymbol.fnt",
	"assets/lowpoly.fnt",
};



}