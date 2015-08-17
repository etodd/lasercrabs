#include "asset.h"

namespace VI
{

AssetID const Asset::Mesh::Alpha = 0;
AssetID const Asset::Mesh::city1 = 1;
AssetID const Asset::Mesh::city2 = 2;
AssetID const Asset::Mesh::city3 = 3;
AssetID const Asset::Mesh::city4_elevator = 4;
AssetID const Asset::Mesh::city4_shell = 5;
AssetID const Asset::Mesh::city4_shell_1 = 6;
AssetID const Asset::Mesh::cube = 7;
AssetID const Asset::Mesh::skybox = 8;

const char* Asset::Mesh::values[] =
{
	"assets/Alpha.msh",
	"assets/city1.msh",
	"assets/city2.msh",
	"assets/city3.msh",
	"assets/city4_elevator.msh",
	"assets/city4_shell.msh",
	"assets/city4_shell_1.msh",
	"assets/cube.msh",
	"assets/skybox.msh",
};

AssetID const Asset::Animation::elevator1 = 0;
AssetID const Asset::Animation::walk = 1;

const char* Asset::Animation::values[] =
{
	"assets/elevator1.anm",
	"assets/walk.anm",
};

AssetID const Asset::Armature::Alpha = 0;
AssetID const Asset::Armature::city1 = 1;
AssetID const Asset::Armature::city2 = 2;
AssetID const Asset::Armature::city3 = 3;
AssetID const Asset::Armature::city4 = 4;
AssetID const Asset::Armature::cube = 5;
AssetID const Asset::Armature::skybox = 6;

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

AssetID const Asset::Bone::Alpha_Alpha_Head = 6;
AssetID const Asset::Bone::Alpha_Alpha_Hips = 0;
AssetID const Asset::Bone::Alpha_Alpha_Neck = 4;
AssetID const Asset::Bone::Alpha_Alpha_Neck1 = 5;
AssetID const Asset::Bone::Alpha_Alpha_Spine = 1;
AssetID const Asset::Bone::Alpha_Alpha_Spine1 = 2;
AssetID const Asset::Bone::Alpha_Alpha_Spine2 = 3;
AssetID const Asset::Bone::Alpha_Left_Arm = 8;
AssetID const Asset::Bone::Alpha_Left_Foot = 47;
AssetID const Asset::Bone::Alpha_Left_ForeArm = 9;
AssetID const Asset::Bone::Alpha_Left_Hand = 10;
AssetID const Asset::Bone::Alpha_Left_HandIndex1 = 17;
AssetID const Asset::Bone::Alpha_Left_HandIndex2 = 18;
AssetID const Asset::Bone::Alpha_Left_HandIndex3 = 19;
AssetID const Asset::Bone::Alpha_Left_HandMiddle1 = 14;
AssetID const Asset::Bone::Alpha_Left_HandMiddle2 = 15;
AssetID const Asset::Bone::Alpha_Left_HandMiddle3 = 16;
AssetID const Asset::Bone::Alpha_Left_HandPinky1 = 23;
AssetID const Asset::Bone::Alpha_Left_HandPinky2 = 24;
AssetID const Asset::Bone::Alpha_Left_HandPinky3 = 25;
AssetID const Asset::Bone::Alpha_Left_HandRing1 = 20;
AssetID const Asset::Bone::Alpha_Left_HandRing2 = 21;
AssetID const Asset::Bone::Alpha_Left_HandRing3 = 22;
AssetID const Asset::Bone::Alpha_Left_HandThumb1 = 11;
AssetID const Asset::Bone::Alpha_Left_HandThumb2 = 12;
AssetID const Asset::Bone::Alpha_Left_HandThumb3 = 13;
AssetID const Asset::Bone::Alpha_Left_Leg = 46;
AssetID const Asset::Bone::Alpha_Left_Shoulder = 7;
AssetID const Asset::Bone::Alpha_Left_ToeBase = 48;
AssetID const Asset::Bone::Alpha_Left_UpLeg = 45;
AssetID const Asset::Bone::Alpha_Right_Arm = 27;
AssetID const Asset::Bone::Alpha_Right_Foot = 51;
AssetID const Asset::Bone::Alpha_Right_ForeArm = 28;
AssetID const Asset::Bone::Alpha_Right_Hand = 29;
AssetID const Asset::Bone::Alpha_Right_HandIndex1 = 33;
AssetID const Asset::Bone::Alpha_Right_HandIndex2 = 34;
AssetID const Asset::Bone::Alpha_Right_HandIndex3 = 35;
AssetID const Asset::Bone::Alpha_Right_HandMiddle1 = 36;
AssetID const Asset::Bone::Alpha_Right_HandMiddle2 = 37;
AssetID const Asset::Bone::Alpha_Right_HandMiddle3 = 38;
AssetID const Asset::Bone::Alpha_Right_HandPinky1 = 42;
AssetID const Asset::Bone::Alpha_Right_HandPinky2 = 43;
AssetID const Asset::Bone::Alpha_Right_HandPinky3 = 44;
AssetID const Asset::Bone::Alpha_Right_HandRing1 = 39;
AssetID const Asset::Bone::Alpha_Right_HandRing2 = 40;
AssetID const Asset::Bone::Alpha_Right_HandRing3 = 41;
AssetID const Asset::Bone::Alpha_Right_HandThumb1 = 30;
AssetID const Asset::Bone::Alpha_Right_HandThumb2 = 31;
AssetID const Asset::Bone::Alpha_Right_HandThumb3 = 32;
AssetID const Asset::Bone::Alpha_Right_Leg = 50;
AssetID const Asset::Bone::Alpha_Right_Shoulder = 26;
AssetID const Asset::Bone::Alpha_Right_ToeBase = 52;
AssetID const Asset::Bone::Alpha_Right_UpLeg = 49;
AssetID const Asset::Bone::city1_Camera = 2;
AssetID const Asset::Bone::city1_Lamp = 1;
AssetID const Asset::Bone::city1_city1 = 0;
AssetID const Asset::Bone::city2_city2 = 0;
AssetID const Asset::Bone::city3_Camera = 2;
AssetID const Asset::Bone::city3_Lamp = 1;
AssetID const Asset::Bone::city3_city3 = 0;
AssetID const Asset::Bone::city4_Empty = 0;
AssetID const Asset::Bone::city4_elevator = 2;
AssetID const Asset::Bone::city4_shell = 3;
AssetID const Asset::Bone::city4_spawn = 1;
AssetID const Asset::Bone::cube_Camera = 2;
AssetID const Asset::Bone::cube_Lamp = 1;
AssetID const Asset::Bone::cube_cube = 0;
AssetID const Asset::Bone::skybox_Camera = 2;
AssetID const Asset::Bone::skybox_Lamp = 1;
AssetID const Asset::Bone::skybox_skybox = 0;
AssetID const Asset::Metadata::AspectH = 0;
AssetID const Asset::Metadata::AspectW = 1;
AssetID const Asset::Metadata::PlayerSpawn = 2;
AssetID const Asset::Metadata::Sentinel = 3;
AssetID const Asset::Metadata::StaticGeom = 4;
AssetID const Asset::Texture::skybox_horizon = 0;
AssetID const Asset::Texture::test = 1;

const char* Asset::Texture::values[] =
{
	"assets/skybox_horizon.png",
	"assets/test.png",
};

AssetID const Asset::Shader::Armature = 0;
AssetID const Asset::Shader::Standard = 1;
AssetID const Asset::Shader::UI = 2;
AssetID const Asset::Shader::flat_texture = 3;

const char* Asset::Shader::values[] =
{
	"assets/Armature.glsl",
	"assets/Standard.glsl",
	"assets/UI.glsl",
	"assets/flat_texture.glsl",
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

AssetID const Asset::Font::SegoeUISymbol = 0;
AssetID const Asset::Font::lowpoly = 1;

const char* Asset::Font::values[] =
{
	"assets/SegoeUISymbol.fnt",
	"assets/lowpoly.fnt",
};

const AssetID Asset::mesh_refs[][3] =
{
	{
		0,
	},
	{
		1,
	},
	{
		2,
	},
	{
		3,
	},
	{
		4,
		5,
		6,
	},
	{
		7,
	},
	{
		8,
	},
};
const AssetID Asset::metadata_refs[][3] =
{
	{
	},
	{
		0,
		1,
	},
	{
	},
	{
		0,
		1,
	},
	{
		2,
		3,
		4,
	},
	{
		0,
		1,
	},
	{
		0,
		1,
	},
};

}