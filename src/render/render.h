#pragma once

#include <GL/glew.h>
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
	Vec3 pos;
	Quat rot;
	ScreenRect viewport;

	Camera()
		: active(), projection(), pos(), rot(), viewport()
	{

	}
	Mat4 view() const;
	void remove();
};

struct RenderSync;

enum RenderOp
{
	RenderOp_Viewport,
	RenderOp_AllocMesh,
	RenderOp_FreeMesh,
	RenderOp_UpdateAttribBuffers,
	RenderOp_UpdateIndexBuffer,
	RenderOp_LoadTexture,
	RenderOp_FreeTexture,
	RenderOp_LoadShader,
	RenderOp_FreeShader,
	RenderOp_DepthMask,
	RenderOp_Mesh,
	RenderOp_Clear,
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
		: quit(), time(), queue(), read_pos(), ready(), mutex(), condition()
	{
		memset(&input, 0, sizeof(InputState));
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
		Array<GLuint> uniforms;
	};

	Array<GLuint> textures;
	Array<Shader> shaders;
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
	const Camera* camera;
	Mat4 view;
	Mat4 view_projection;
	GLbitfield clear;
	RenderTechnique technique;
	SyncData* sync;
};

void render(SyncData*, GLData*);

}
