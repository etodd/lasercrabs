#pragma once

#include "data/array.h"
#include "lmath.h"

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

enum class RenderBlendMode
{
	Opaque,
	Alpha,
	Additive,
	AlphaDestination,
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

struct RenderSync
{
	Array<char> queue;
	s32 read_pos;

	RenderSync()
		: queue(), read_pos()
	{
	}

	// IMPORTANT: don't do this: T something; write(&something);
	// It will resolve to write<T*> rather than write<T>, so you'll get the wrong size.
	// Use write<T>(&something) or write(something)

	template<typename T>
	void write(const T& data)
	{
		write(&data);
	}

	template<typename T>
	void write(const T* data, const s32 count = 1)
	{
		T* destination = alloc<T>(count);

		memcpy((void*)destination, data, sizeof(T) * count);
	}

	template<typename T>
	T* alloc(const s32 count = 1)
	{
		s32 pos = queue.length;
		queue.resize(pos + sizeof(T) * count);
		return (T*)(queue.data + pos);
	}

	template<typename T>
	const T* read(s32 count = 1)
	{
		T* result = (T*)(queue.data + read_pos);
		read_pos += sizeof(T) * count;
		return result;
	}
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
