#pragma once

#include "lmath.h"
#include "sync.h"

namespace VI
{

enum class RenderOp
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
};

enum class RenderPrimitiveMode
{
	Triangles,
	TriangleStrip,
	TriangleFan,
	Lines,
	LineStrip,
	LineLoop,
	Points,
};

enum class RenderBlendMode
{
	Opaque,
	Alpha,
	Additive,
	AlphaDestination,
	Multiply,
};

enum class RenderDynamicTextureType
{
	Color,
	ColorMultisample,
	Depth,
};

enum class RenderTextureFilter
{
	Nearest,
	Linear,
};

enum class RenderTextureWrap
{
	Repeat,
	Clamp,
};

enum class RenderTextureCompare
{
	None,
	RefToTexture,
};

enum class RenderFramebufferAttachment
{
	Color0,
	Color1,
	Color2,
	Color3,
	Depth,
};

enum class RenderCullMode
{
	Back,
	Front,
	None,
};

enum class RenderFillMode
{
	Fill,
	Line,
	Point,
};

typedef u8 RenderColorMask;
#define RENDER_COLOR_MASK_DEFAULT 31

struct RenderSync : public SyncBuffer
{
};

enum class RenderTextureType
{
	Texture2D,
};

void render_init();
void render(RenderSync*);
b8 compile_shader(const char*, const char*, s32, u32*, const char* = 0);

struct TechniquePrefixes
{
	static const char* all[];
};

enum class RenderTechnique
{
	Default,
	Shadow,
	count,
};

enum class RenderDataType
{
	R32,
	Vec2,
	Vec3,
	Vec4,
	S32,
	Mat4,
	Texture,
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
