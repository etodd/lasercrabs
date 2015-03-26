#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "types.h"
#include "data/array.h"
#include <mutex>
#include <condition_variable>

enum RenderTechnique
{
	RenderTechnique_Default,
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
};

enum SwapType
{
	SwapType_Read,
	SwapType_Write,
	SwapType_count,
};

struct SyncData
{
	bool quit;
	char keys[GLFW_KEY_LAST + 1];
	double cursor_x;
	double cursor_y;
	bool mouse;
	GameTime time;
	Array<char> queue;
	size_t read_pos;
	bool ready[SwapType_count];
	mutable std::mutex mutex;
	std::condition_variable condition;

	template<typename T>
	void send(T* data, size_t count = 1)
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
		send<RenderOp>(&op);
	}

	template<typename T>
	T* read(size_t count = 1)
	{
		T* result = (T*)(queue.data + read_pos);
		read_pos += sizeof(T) * count;
		return result;
	}
};

struct RenderParams
{
	Mat4 view;
	Mat4 projection;
	GLbitfield clear;
	RenderTechnique technique;
	SyncData* sync;
};

struct Swapper;

struct RenderSync
{
	static const size_t count = 2;
	SyncData data[count];

	RenderSync();
	Swapper swapper(size_t);
};

struct Swapper
{
	RenderSync* sync;
	size_t current;
	SyncData* data();
	template<SwapType swap_type> SyncData* swap()
	{
		{
			std::lock_guard<std::mutex> lock(sync->data[current].mutex);
			sync->data[current].ready[!swap_type] = true;
		}
		sync->data[current].condition.notify_all();

		size_t next = (current + 1) % RenderSync::count;
		std::unique_lock<std::mutex> lock(sync->data[next].mutex);
		while (!sync->data[next].ready[swap_type])
			sync->data[next].condition.wait(lock);
		sync->data[next].ready[swap_type] = false;
		current = next;
		return &sync->data[next];
	}
};

struct Loader;

void render(SyncData*, Loader*);
