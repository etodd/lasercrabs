#include "load.h"
#include <stdio.h>
#include <GL/glew.h>
#include "lodepng.h"
#include "vi_assert.h"

namespace VI
{

RenderSync::Swapper* Loader::swapper;
// First entry in each array is empty
Loader::Entry<Mesh> Loader::meshes[Asset::Model::count];
Loader::Entry<Animation> Loader::animations[Asset::Animation::count];
Loader::Entry<void*> Loader::textures[Asset::Texture::count];
Loader::Entry<void*> Loader::shaders[Asset::Shader::count];
Loader::Entry<Font> Loader::fonts[Asset::Font::count];
Array<Loader::Entry<void*>> Loader::dynamic_meshes = Array<Loader::Entry<void*>>();

Mesh* Loader::mesh(AssetID id)
{
	if (id == Asset::Nothing)
		return 0;

	if (meshes[id].type == AssetNone)
	{
		const char* path = Asset::Model::filenames[id];
		FILE* f = fopen(path, "rb");
		if (!f)
		{
			fprintf(stderr, "Can't open mdl file '%s'\n", path);
			return 0;
		}

		Mesh* mesh = &meshes[id].data;
		new (mesh)Mesh();

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
			mesh->bone_indices.reserve(vertex_count);
			fread(mesh->bone_indices.data, sizeof(int[MAX_BONE_WEIGHTS]), vertex_count, f);

			mesh->bone_weights.reserve(vertex_count);
			fread(mesh->bone_weights.data, sizeof(float[MAX_BONE_WEIGHTS]), vertex_count, f);

			mesh->bone_hierarchy.resize(bone_count);
			fread(mesh->bone_hierarchy.data, sizeof(int), bone_count, f);
			mesh->inverse_bind_pose.resize(bone_count);
			fread(mesh->inverse_bind_pose.data, sizeof(Mat4), bone_count, f);
		}

		fclose(f);

		// Physics
		new (&mesh->physics) btTriangleIndexVertexArray(mesh->indices.length / 3, mesh->indices.data, 3 * sizeof(int), mesh->vertices.length, (btScalar*)mesh->vertices.data, sizeof(Vec3));

		// GL
		SyncData* sync = swapper->get();
		sync->write(RenderOp_AllocMesh);
		sync->write<size_t>(id);

		sync->write<int>(bone_count > 0 ? 5 : 3); // Attribute count

		sync->write(RenderDataType_Vec3);
		sync->write<int>(1); // Number of data elements per vertex

		sync->write(RenderDataType_Vec2);
		sync->write<int>(1); // Number of data elements per vertex

		sync->write(RenderDataType_Vec3);
		sync->write<int>(1); // Number of data elements per vertex

		if (bone_count > 0)
		{
			sync->write(RenderDataType_Int);
			sync->write<int>(MAX_BONE_WEIGHTS); // Number of data elements per vertex

			sync->write(RenderDataType_Float);
			sync->write<int>(MAX_BONE_WEIGHTS); // Number of data elements per vertex
		}

		sync->write(RenderOp_UpdateAttribBuffers);
		sync->write<size_t>(id);

		sync->write<int>(mesh->vertices.length);
		sync->write(mesh->vertices.data, mesh->vertices.length);
		sync->write(mesh->uvs.data, mesh->vertices.length);
		sync->write(mesh->normals.data, mesh->vertices.length);

		if (bone_count > 0)
		{
			sync->write(mesh->bone_indices.data, mesh->vertices.length);
			sync->write(mesh->bone_weights.data, mesh->vertices.length);
		}

		sync->write(RenderOp_UpdateIndexBuffer);
		sync->write<size_t>(id);
		sync->write<int>(mesh->indices.length);
		sync->write(mesh->indices.data, mesh->indices.length);

		meshes[id].type = AssetTransient;
	}
	return &meshes[id].data;
}

Mesh* Loader::mesh_permanent(AssetID id)
{
	Mesh* m = mesh(id);
	if (m)
		meshes[id].type = AssetPermanent;
	return m;
}

void Loader::mesh_unload(AssetID id)
{
	if (id != Asset::Nothing && meshes[id].type != AssetNone)
	{
		meshes[id].data.~Mesh();
		SyncData* sync = swapper->get();
		sync->write(RenderOp_DeallocMesh);
		sync->write<size_t>(id);
		meshes[id].type = AssetNone;
	}
}

Animation* Loader::animation(AssetID id)
{
	if (id == Asset::Nothing)
		return 0;

	if (animations[id].type == AssetNone)
	{
		const char* path = Asset::Animation::filenames[id];
		FILE* f = fopen(path, "rb");
		if (!f)
		{
			fprintf(stderr, "Can't open anm file '%s'\n", path);
			return 0;
		}

		Animation* anim = &animations[id].data;
		new (anim)Animation();

		fread(&anim->duration, sizeof(float), 1, f);

		int channel_count;
		fread(&channel_count, sizeof(int), 1, f);
		anim->channels.reserve(channel_count);
		anim->channels.length = channel_count;

		for (int i = 0; i < channel_count; i++)
		{
			Channel* channel = &anim->channels[i];
			int position_count;
			fread(&position_count, sizeof(int), 1, f);
			channel->positions.reserve(position_count);
			channel->positions.length = position_count;
			fread(channel->positions.data, sizeof(Keyframe<Vec3>), position_count, f);

			int rotation_count;
			fread(&rotation_count, sizeof(int), 1, f);
			channel->rotations.reserve(rotation_count);
			channel->rotations.length = rotation_count;
			fread(channel->rotations.data, sizeof(Keyframe<Quat>), rotation_count, f);

			int scale_count;
			fread(&scale_count, sizeof(int), 1, f);
			channel->scales.reserve(scale_count);
			channel->scales.length = scale_count;
			fread(channel->scales.data, sizeof(Keyframe<Vec3>), scale_count, f);
		}

		fclose(f);

		animations[id].type = AssetTransient;
	}
	return &animations[id].data;
}

Animation* Loader::animation_permanent(AssetID id)
{
	Animation* anim = animation(id);
	if (anim)
		animations[id].type = AssetPermanent;
	return anim;
}

void Loader::animation_unload(AssetID id)
{
	if (id != Asset::Nothing && animations[id].type != AssetNone)
	{
		animations[id].data.~Animation();
		animations[id].type = AssetNone;
	}
}

void Loader::texture(AssetID id)
{
	if (id != Asset::Nothing && textures[id].type == AssetNone)
	{
		textures[id].type = AssetTransient;

		const char* path = Asset::Texture::filenames[id];
		unsigned char* buffer;
		unsigned width, height;

		unsigned error = lodepng_decode32_file(&buffer, &width, &height, path);

		if (error)
		{
			fprintf(stderr, "Error loading texture '%s': %s\n", path, lodepng_error_text(error));
			return;
		}

		SyncData* sync = swapper->get();
		sync->write(RenderOp_LoadTexture);
		sync->write<AssetID>(&id);
		sync->write<unsigned>(&width);
		sync->write<unsigned>(&height);
		sync->write<unsigned char>(buffer, 4 * width * height);
		free(buffer);
	}
}

void Loader::texture_permanent(AssetID id)
{
	texture(id);
	if (id != Asset::Nothing)
		textures[id].type = AssetPermanent;
}

void Loader::texture_unload(AssetID id)
{
	if (id != Asset::Nothing && textures[id].type != AssetNone)
	{
		SyncData* sync = swapper->get();
		sync->write(RenderOp_UnloadTexture);
		sync->write<AssetID>(&id);
		textures[id].type = AssetNone;
	}
}

void Loader::shader(AssetID id)
{
	if (id != Asset::Nothing && shaders[id].type == AssetNone)
	{
		shaders[id].type = AssetTransient;

		const char* path = Asset::Shader::filenames[id];

		Array<char> code;
		FILE* f = fopen(path, "r");
		if (!f)
		{
			fprintf(stderr, "Can't open shader source file '%s'", path);
			return;
		}

		const size_t chunk_size = 4096;
		int i = 1;
		while (true)
		{
			code.reserve(i * chunk_size + 1); // extra char since this will be a null-terminated string
			size_t read = fread(&code.data[(i - 1) * chunk_size], sizeof(char), chunk_size, f);
			if (read < chunk_size)
			{
				code.length = ((i - 1) * chunk_size) + read;
				break;
			}
			i++;
		}
		fclose(f);

		SyncData* sync = swapper->get();
		sync->write(RenderOp_LoadShader);
		sync->write<AssetID>(&id);
		sync->write<size_t>(&code.length);
		sync->write(code.data, code.length);
	}
}

void Loader::shader_permanent(AssetID id)
{
	shader(id);
	if (id != Asset::Nothing)
		shaders[id].type = AssetPermanent;
}

void Loader::shader_unload(AssetID id)
{
	if (id != Asset::Nothing && shaders[id].type != AssetNone)
	{
		SyncData* sync = swapper->get();
		sync->write(RenderOp_UnloadShader);
		sync->write(&id);
		shaders[id].type = AssetNone;
	}
}

Font* Loader::font(AssetID id)
{
	if (id == Asset::Nothing)
		return 0;

	if (fonts[id].type == AssetNone)
	{
		const char* path = Asset::Font::filenames[id];
		FILE* f = fopen(path, "rb");
		if (!f)
		{
			fprintf(stderr, "Can't open fnt file '%s'\n", path);
			return 0;
		}

		Font* font = &fonts[id].data;
		new (font)Font();

		int j;

		fread(&j, sizeof(int), 1, f);
		font->indices.resize(j);
		fread(font->indices.data, sizeof(int), font->indices.length, f);

		fread(&j, sizeof(int), 1, f);
		font->vertices.resize(j);
		fread(font->vertices.data, sizeof(Vec3), font->vertices.length, f);

		fread(&j, sizeof(int), 1, f);
		Array<Font::Character> characters;
		characters.resize(j);
		fread(characters.data, sizeof(Font::Character), characters.length, f);
		for (size_t i = 0; i < characters.length; i++)
		{
			Font::Character* c = &characters[i];
			font->characters[c->code] = *c;
		}

		fclose(f);
	}
	return &fonts[id].data;
}

Font* Loader::font_permanent(AssetID id)
{
	Font* f = font(id);
	if (f)
		fonts[id].type = AssetPermanent;
	return f;
}

void Loader::font_unload(AssetID id)
{
	if (id != Asset::Nothing && fonts[id].type != AssetNone)
	{
		fonts[id].data.~Font();
		fonts[id].type = AssetNone;
	}
}

void Loader::unload_transients()
{
	// First entry in each array is empty
	// That way ID 0 is invalid, and we don't have to do [id - 1] all the time

	for (AssetID i = 0; i < Asset::Model::count; i++)
	{
		if (meshes[i].type == AssetTransient)
			mesh_unload(i);
	}

	for (AssetID i = 0; i < Asset::Texture::count; i++)
	{
		if (textures[i].type == AssetTransient)
			texture_unload(i);
	}

	for (AssetID i = 0; i < Asset::Shader::count; i++)
	{
		if (shaders[i].type == AssetTransient)
			shader_unload(i);
	}

	for (AssetID i = 0; i < Asset::Font::count; i++)
	{
		if (fonts[i].type == AssetTransient)
			font_unload(i);
	}
}

}
