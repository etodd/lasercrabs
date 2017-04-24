#pragma once

#include "lmath.h"
#include "sync.h"

namespace VI
{

enum class RenderOp : s8
{
	AllocUniform,
	Viewport,
	AllocMesh,
	FreeMesh,
	AllocInstances,
	FreeInstances,
	UpdateAttribBuffers,
	UpdateAttribSubBuffers,
	UpdateAttribBuffer,
	UpdateAttribSubBuffer,
	UpdateIndexBuffer,
	UpdateEdgesIndexBuffer,
	AllocTexture,
	DynamicTexture,
	LoadTexture,
	FreeTexture,
	LoadShader,
	FreeShader,
	ColorMask,
	DepthMask,
	DepthTest,
	Shader,
	Uniform,
	Mesh,
	MeshEdges,
	SubMesh,
	Instances,
	Clear,
	BlendMode,
	CullMode,
	FillMode,
	PointSize,
	LineWidth,
	AllocFramebuffer,
	BindFramebuffer,
	FreeFramebuffer,
	BlitFramebuffer,
	count,
};

enum class RenderPrimitiveMode : s8
{
	Triangles,
	TriangleStrip,
	TriangleFan,
	Lines,
	LineStrip,
	LineLoop,
	Points,
	count,
};

enum class RenderBlendMode : s8
{
	Opaque,
	Alpha,
	Additive,
	AlphaDestination,
	Multiply,
	count,
};

enum class RenderDynamicTextureType : s8
{
	Color,
	ColorMultisample,
	Depth,
	DepthMultisample,
	count,
};

enum class RenderTextureFilter
{
	Nearest,
	Linear,
	count,
};

enum class RenderTextureWrap : s8
{
	Repeat,
	Clamp,
	count,
};

enum class RenderTextureCompare : s8
{
	None,
	RefToTexture,
	count,
};

enum class RenderFramebufferAttachment : s8
{
	Color0,
	Color1,
	Color2,
	Color3,
	Depth,
	count,
};

enum class RenderCullMode : s8
{
	Back,
	Front,
	None,
	count,
};

enum class RenderFillMode : s8
{
	Fill,
	Line,
	Point,
	count,
};

typedef s8 RenderColorMask;
#define RENDER_COLOR_MASK_DEFAULT 31

struct RenderSync : public SyncBuffer
{
};

enum class RenderTextureType : s8
{
	Texture2D,
	count,
};

void render_init();
void render(RenderSync*);
b8 compile_shader(const char*, const char*, s32, u32*, const char* = 0);

enum class RenderTechnique : s8
{
	Default,
	Shadow,
	count,
};

struct TechniquePrefixes
{
	static const char* all[(s32)RenderTechnique::count]; // defined in import_common.cpp
};

enum class RenderDataType : s8
{
	R32,
	Vec2,
	Vec3,
	Vec4,
	S32,
	Mat4,
	Texture,
	count,
};

inline size_t render_data_type_size(RenderDataType type)
{
	switch (type)
	{
		case RenderDataType::R32:
			return sizeof(r32);
		case RenderDataType::Vec2:
			return sizeof(Vec2);
		case RenderDataType::Vec3:
			return sizeof(Vec3);
		case RenderDataType::Vec4:
			return sizeof(Vec4);
		case RenderDataType::S32:
			return sizeof(s32);
		case RenderDataType::Mat4:
			return sizeof(Mat4);
		case RenderDataType::Texture:
			return sizeof(s32);
	}
	vi_assert(false);
	return 0;
}

}