#pragma once

#include <GL/glew.h>
#include "types.h"
#include "data/array.h"
#include "lmath.h"
#include "sync.h"
#include "asset.h"

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
	RenderOp_LoadTexture,
	RenderOp_UnloadTexture,
	RenderOp_LoadShader,
	RenderOp_UnloadShader,
	RenderOp_View,
	RenderOp_Clear,
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

	void op(RenderOp op)
	{
		write<RenderOp>(&op);
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

	Mesh meshes[Asset::Model::count];
	GLuint textures[Asset::Texture::count];
	GLuint shaders[Asset::Shader::count];

	GLData()
		: meshes(), textures(), shaders()
	{

	}
};

void render(SyncData*, GLData*);