#pragma once

#include "types.h"
#include "data/array.h"
#include "lmath.h"
#include "sync.h"
#include "data/import_common.h"
#include "input.h"

namespace VI
{

struct ScreenRect
{
	int x, y, width, height;
};

struct Camera
{
	static const int max_cameras = 4;
	static Camera all[max_cameras];

	static Camera* add();

	bool active;
	Mat4 projection;
	Mat4 projection_inverse;
	float near_plane;
	float far_plane;
	Vec3 pos;
	Quat rot;
	ScreenRect viewport;

	Camera()
		: active(), projection(), projection_inverse(), pos(), rot(), viewport(), near_plane(), far_plane()
	{

	}
	void perspective(const float, const float, const float, const float);
	void orthographic(const float, const float, const float, const float);
	void projection_frustum(Vec3*) const;
	Mat4 view() const;
	void remove();
};

enum RenderOp
{
	RenderOp_Viewport,
	RenderOp_AllocMesh,
	RenderOp_FreeMesh,
	RenderOp_UpdateAttribBuffers,
	RenderOp_UpdateIndexBuffer,
	RenderOp_AllocTexture,
	RenderOp_DynamicTexture,
	RenderOp_LoadTexture,
	RenderOp_FreeTexture,
	RenderOp_LoadShader,
	RenderOp_FreeShader,
	RenderOp_ColorMask,
	RenderOp_DepthMask,
	RenderOp_DepthTest,
	RenderOp_Shader,
	RenderOp_Uniform,
	RenderOp_Mesh,
	RenderOp_Clear,
	RenderOp_BlendMode,
	RenderOp_CullMode,
	RenderOp_AllocFramebuffer,
	RenderOp_BindFramebuffer,
	RenderOp_FreeFramebuffer,
};

enum RenderBlendMode
{
	RenderBlend_Opaque,
	RenderBlend_Alpha,
	RenderBlend_Additive,
};

enum RenderDynamicTextureType
{
	RenderDynamicTexture_Color,
	RenderDynamicTexture_Depth,
};

enum RenderTextureFilter
{
	RenderTextureFilter_Nearest,
	RenderTextureFilter_Linear,
};

enum RenderFramebufferAttachment
{
	RenderFramebufferAttachment_Color0,
	RenderFramebufferAttachment_Color1,
	RenderFramebufferAttachment_Color2,
	RenderFramebufferAttachment_Color3,
	RenderFramebufferAttachment_Depth,
};

enum RenderCullMode
{
	RenderCull_Back,
	RenderCull_Front,
	RenderCull_None,
};

struct RenderSync
{
	bool quit;
	bool focus;
	GameTime time;
	InputState input;
	Array<char> queue;
	int read_pos;

	RenderSync()
		: quit(), time(), queue(), read_pos()
	{
		memset(&input, 0, sizeof(InputState));
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
		int size = sizeof(T) * count;

		int pos = queue.length;
		queue.length = pos + size;
		queue.reserve(queue.length);
		
		void* destination = (void*)(queue.data + pos);

		memcpy(destination, data, size);
	}

	template<typename T>
	const T* read(int count = 1)
	{
		T* result = (T*)(queue.data + read_pos);
		read_pos += sizeof(T) * count;
		return result;
	}
};

typedef Sync<RenderSync>::Swapper RenderSwapper;

enum RenderTextureType
{
	RenderTexture2D,
};

struct RenderSync;
struct RenderParams
{
	const Camera* camera;
	Mat4 view;
	Mat4 view_projection;
	RenderTechnique technique;
	RenderSync* sync;
};

void render_init();
void render(RenderSync*);

}
