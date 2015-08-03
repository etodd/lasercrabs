#pragma once

#include <GL/glew.h>
#include "types.h"
#include "data/array.h"
#include "lmath.h"
#include "sync.h"
#include "asset.h"

namespace VI
{

struct Camera
{
	static Camera main;
	Camera()
		: projection(), pos(), rot()
	{

	}
	Mat4 view();
	Mat4 projection;
	Vec3 pos;
	Quat rot;
};

struct RenderSync;

enum RenderOp
{
	RenderOp_AllocMesh,
	RenderOp_FreeMesh,
	RenderOp_UpdateAttribBuffers,
	RenderOp_UpdateIndexBuffer,
	RenderOp_LoadTexture,
	RenderOp_FreeTexture,
	RenderOp_LoadShader,
	RenderOp_FreeShader,
	RenderOp_Mesh,
	RenderOp_Clear,
};

enum RenderDataType
{
	RenderDataType_Float,
	RenderDataType_Vec2,
	RenderDataType_Vec3,
	RenderDataType_Vec4,
	RenderDataType_Int,
	RenderDataType_Mat4,
	RenderDataType_Texture,
};

struct SyncData
{
	bool quit;
	bool focus;
	GameTime time;
	InputState input;
	Array<char> queue;
	int read_pos;
	bool ready[SwapType_count];
	mutable std::mutex mutex;
	std::condition_variable condition;

	SyncData()
		: quit(), time(), input(), queue(), read_pos(), ready(), mutex(), condition()
	{

	}

	template<typename T>
	void write(T* data, int count = 1)
	{
		int size = sizeof(T) * count;

		int pos = queue.length;
		queue.length = pos + size;
		queue.reserve(queue.length);
		
		void* destination = (void*)(queue.data + pos);

		memcpy(destination, data, size);
	}

	template<typename T>
	void write(const T& data)
	{
		write(&data);
	}

	template<typename T>
	T* read(int count = 1)
	{
		T* result = (T*)(queue.data + read_pos);
		read_pos += sizeof(T) * count;
		return result;
	}
};


struct RenderSync
{
	static const int count = 2;
	typedef Swapper<SyncData, count> Swapper;
	SyncData data[count];

	RenderSync()
		: data()
	{
	}

	Swapper swapper(int);
};

struct Loader;

struct GLData
{
	struct Mesh
	{
		struct Attrib
		{
			int total_element_size;
			int element_count;
			RenderDataType data_type;
			GLuint gl_type;
			GLuint handle;
		};

		Array<Attrib> attribs;
		GLuint index_buffer;
		GLuint vertex_array;
		int index_count;

		Mesh()
			: attribs(), index_buffer(), vertex_array(), index_count()
		{
			
		}
	};

	struct Shader
	{
		GLuint handle;
		GLuint uniforms[Asset::Uniform::count];
	};

	GLuint textures[Asset::Texture::count];
	Shader shaders[Asset::Shader::count];
	Array<Mesh> meshes;

	GLData()
		: meshes(), textures(), shaders()
	{

	}
};

enum RenderTechnique
{
	RenderTechnique_Default,
};

struct SyncData;
struct RenderParams
{
	Mat4 view;
	Mat4 projection;
	Vec3 camera_pos;
	Quat camera_rot;
	int width;
	int height;
	Mat4 view_projection;
	GLbitfield clear;
	RenderTechnique technique;
	SyncData* sync;
};

void render(SyncData*, GLData*);

}
