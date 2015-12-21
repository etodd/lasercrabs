#pragma once

#include "data/array.h"
#include "lmath.h"

namespace VI
{

struct ScreenRect
{
	int x, y, width, height;
};

enum class RenderOp
{
	AllocUniform,
	Viewport,
	AllocMesh,
	FreeMesh,
	AllocInstances,
	FreeInstances,
	UpdateAttribBuffers,
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
	Instances,
	Clear,
	BlendMode,
	CullMode,
	FillMode,
	PointSize,
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
	int read_pos;

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
	void write(const T* data, const int count = 1)
	{
		T* destination = alloc<T>(count);

		memcpy((void*)destination, data, sizeof(T) * count);
	}

	template<typename T>
	T* alloc(const int count = 1)
	{
		int pos = queue.length;
		queue.resize(pos + sizeof(T) * count);
		return (T*)(queue.data + pos);
	}

	template<typename T>
	const T* read(int count = 1)
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
bool compile_shader(const char*, const char*, int, unsigned int*, const char* = 0);

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
	Float,
	Vec2,
	Vec3,
	Vec4,
	Int,
	Mat4,
	Texture,
};

inline size_t render_data_type_size(RenderDataType type)
{
	switch (type)
	{
		case RenderDataType::Float:
			return sizeof(float);
		case RenderDataType::Vec2:
			return sizeof(Vec2);
		case RenderDataType::Vec3:
			return sizeof(Vec3);
		case RenderDataType::Vec4:
			return sizeof(Vec4);
		case RenderDataType::Int:
			return sizeof(int);
		case RenderDataType::Mat4:
			return sizeof(Mat4);
		case RenderDataType::Texture:
			return sizeof(int);
	}
	vi_assert(false);
	return 0;
}

}
