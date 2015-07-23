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
		: projection(), view()
	{

	}
	Mat4 projection;
	Mat4 view;
};

struct RenderSync;

enum RenderOp
{
	RenderOp_LoadMesh,
	RenderOp_UnloadMesh,
	RenderOp_LoadDynamicMesh,
	RenderOp_UnloadDynamicMesh,
	RenderOp_LoadTexture,
	RenderOp_UnloadTexture,
	RenderOp_LoadShader,
	RenderOp_UnloadShader,
	RenderOp_Mesh,
	RenderOp_DynamicMesh,
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
	GameTime time;
	InputState input;
	Array<char> queue;
	size_t read_pos;
	bool ready[SwapType_count];
	mutable std::mutex mutex;
	std::condition_variable condition;

	SyncData()
		: quit(), time(), input(), queue(), read_pos(), ready(), mutex(), condition()
	{

	}

	template<typename T>
	void write(T* data, size_t count = 1)
	{
		size_t size = sizeof(T) * count;

		size_t pos = queue.length;
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
	T* read(size_t count = 1)
	{
		T* result = (T*)(queue.data + read_pos);
		read_pos += sizeof(T) * count;
		return result;
	}
};


struct RenderSync
{
	static const size_t count = 2;
	typedef Swapper<SyncData, count> Swapper;
	SyncData data[count];

	RenderSync()
		: data()
	{
	}

	Swapper swapper(size_t);
};

struct Loader;

struct GLData
{
	struct Mesh
	{
		struct Attrib
		{
			int element_size;
			GLuint type;
			GLuint handle;
		};

		Array<Attrib> attribs;
		GLuint index_buffer;
		GLuint vertex_array;
		size_t index_count;

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

	Mesh meshes[Asset::Model::count];
	GLuint textures[Asset::Texture::count];
	Shader shaders[Asset::Shader::count];
	Array<Mesh> dynamic_meshes;

	GLData()
		: meshes(), textures(), shaders(), dynamic_meshes()
	{

	}
};

void render(SyncData*, GLData*);

}