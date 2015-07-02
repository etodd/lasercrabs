#include "load.h"
#include <stdio.h>
#include <GL/glew.h>
#include "lodepng.h"
#include "vi_assert.h"

RenderSync::Swapper* Loader::swapper;
// First entry in each array is empty
Loader::Entry<Mesh> Loader::meshes[Asset::Model::count];
Loader::Entry<void*> Loader::textures[Asset::Texture::count];
Loader::Entry<void*> Loader::shaders[Asset::Shader::count];

AssetID Loader::mesh(AssetID id)
{
	if (id && meshes[id].type == AssetNone)
	{
		const char* path = Asset::Model::filenames[id];
		FILE* f = fopen(path, "rb");
		if (!f)
		{
			fprintf(stderr, "Can't open mdl file '%s'\n", path);
			return 0;
		}

		Mesh* mesh = &meshes[id].data;
		new (mesh) Mesh();

		// Read indices
		int index_count;
		fread(&index_count, sizeof(int), 1, f);

		// Fill face indices
		mesh->indices.reserve(index_count);
		mesh->indices.length = index_count;
		fread(mesh->indices.data, sizeof(int), index_count, f);

		int vertex_count;
		fread(&vertex_count, sizeof(int), 1, f);

		// Fill vertices positions
		mesh->vertices.reserve(vertex_count);
		mesh->vertices.length = vertex_count;
		fread(mesh->vertices.data, sizeof(Vec3), vertex_count, f);

		// Fill vertices texture coordinates
		mesh->uvs.reserve(vertex_count);
		mesh->uvs.length = vertex_count;
		fread(mesh->uvs.data, sizeof(Vec2), vertex_count, f);

		// Fill vertices normals
		mesh->normals.reserve(vertex_count);
		mesh->normals.length = vertex_count;
		fread(mesh->normals.data, sizeof(Vec3), vertex_count, f);

		int bone_count;
		fread(&bone_count, sizeof(int), 1, f);

		if (bone_count > 0)
		{
			for (int i = 0; i < MAX_BONE_WEIGHTS; i++)
			{
				mesh->bone_indices[i].reserve(mesh->vertices.length);
				fread(mesh->bone_indices[i].data, sizeof(int), vertex_count, f);
			}
			for (int i = 0; i < MAX_BONE_WEIGHTS; i++)
			{
				mesh->bone_weights[i].reserve(mesh->vertices.length);
				fread(mesh->bone_weights[i].data, sizeof(float), vertex_count, f);
			}
			mesh->bone_hierarchy.reserve(bone_count);
			mesh->bone_hierarchy.length = bone_count;
			fread(mesh->bone_hierarchy.data, sizeof(int), bone_count, f);
			mesh->inverse_bind_pose.reserve(bone_count);
			mesh->inverse_bind_pose.length = bone_count;
			fread(mesh->inverse_bind_pose.data, sizeof(Mat4), bone_count, f);
		}

		fclose(f);

		// Physics
		new (&mesh->physics) btTriangleIndexVertexArray(mesh->indices.length / 3, mesh->indices.data, 3 * sizeof(int), mesh->vertices.length, (btScalar*)mesh->vertices.data, sizeof(Vec3));

		// GL
		SyncData* sync = swapper->get();
		sync->op(RenderOp_LoadMesh);
		sync->write<AssetID>(&id);
		sync->write<size_t>(&mesh->vertices.length);
		sync->write<Vec3>(mesh->vertices.data, mesh->vertices.length);
		sync->write<Vec2>(mesh->uvs.data, mesh->uvs.length);
		sync->write<Vec3>(mesh->normals.data, mesh->normals.length);
		sync->write<size_t>(&mesh->indices.length);
		sync->write<int>(mesh->indices.data, mesh->indices.length);

		meshes[id].type = AssetTransient;
	}
	return id;
}

AssetID Loader::mesh_permanent(AssetID id)
{
	id = mesh(id);
	if (id)
		meshes[id].type = AssetPermanent;
	return id;
}

void Loader::unload_mesh(AssetID id)
{
	if (id)
	{
		vi_assert(meshes[id].refs > 0);
		if (meshes[id].type != AssetNone)
		{
			meshes[id].data.~Mesh();
			SyncData* sync = swapper->get();
			sync->op(RenderOp_UnloadMesh);
			sync->write<AssetID>(&id);
			meshes[id].type = AssetNone;
		}
	}
}

AssetID Loader::texture(AssetID id)
{
	if (id && textures[id].type == AssetNone)
	{
		const char* path = Asset::Texture::filenames[id];
		unsigned char* buffer;
		unsigned width, height;

		unsigned error = lodepng_decode32_file(&buffer, &width, &height, path);

		if (error)
		{
			fprintf(stderr, "Error loading texture '%s': %s\n", path, lodepng_error_text(error));
			return 0;
		}

		SyncData* sync = swapper->get();
		sync->op(RenderOp_LoadTexture);
		sync->write<AssetID>(&id);
		sync->write<unsigned>(&width);
		sync->write<unsigned>(&height);
		sync->write<unsigned char>(buffer, 4 * width * height);
		free(buffer);

		textures[id].type = AssetTransient;
	}
	return id;
}

AssetID Loader::texture_permanent(AssetID id)
{
	id = texture(id);
	if (id)
		textures[id].type = AssetPermanent;
	return id;
}

void Loader::unload_texture(AssetID id)
{
	if (id)
	{
		vi_assert(textures[id].refs > 0);
		if (textures[id].type != AssetNone)
		{
			SyncData* sync = swapper->get();
			sync->op(RenderOp_UnloadTexture);
			sync->write<AssetID>(&id);
			textures[id].type = AssetNone;
		}
	}
}

AssetID Loader::shader(AssetID id)
{
	if (id && shaders[id].type == AssetNone)
	{
		const char* path = Asset::Shader::filenames[id];

		Array<char> code;
		FILE* f = fopen(path, "r");
		if (!f)
		{
			fprintf(stderr, "Can't open shader source file '%s'", path);
			return 0;
		}

		const size_t chunk_size = 4096;
		int i = 1;
		while (true)
		{
			code.reserve(i * chunk_size + 1); // extra char since this will be a null-terminated string
			size_t read = fread(code.data, sizeof(char), chunk_size, f);
			if (read < chunk_size)
			{
				code.length = ((i - 1) * chunk_size) + read;
				break;
			}
			i++;
		}
		fclose(f);

		SyncData* sync = swapper->get();
		sync->op(RenderOp_LoadShader);
		sync->write<AssetID>(&id);
		sync->write<size_t>(&code.length);
		sync->write<char>(code.data, code.length);

		shaders[id].type = AssetTransient;
	}
	return id;
}

AssetID Loader::shader_permanent(AssetID id)
{
	id = shader(id);
	if (id)
		shaders[id].type = AssetPermanent;
	return id;
}

void Loader::unload_shader(AssetID id)
{
	if (id)
	{
		vi_assert(shaders[id].refs > 0);
		if (shaders[id].type != AssetNone)
		{
			SyncData* sync = swapper->get();
			sync->op(RenderOp_UnloadShader);
			sync->write<AssetID>(&id);
			shaders[id].type = AssetNone;
		}
	}
}

void Loader::unload_transients()
{
	// First entry in each array is empty
	// That way ID 0 is invalid, and we don't have to do [id - 1] all the time

	for (AssetID i = 1; i < Asset::Model::count; i++)
	{
		if (meshes[i].type == AssetTransient)
			unload_mesh(i);
	}

	for (AssetID i = 1; i < Asset::Texture::count; i++)
	{
		if (textures[i].type == AssetTransient)
			unload_texture(i);
	}

	for (AssetID i = 1; i < Asset::Shader::count; i++)
	{
		if (shaders[i].type == AssetTransient)
			unload_shader(i);
	}
}
