#include "load.h"
#include <stdio.h>
#include "lodepng/lodepng.h"
#include "vi_assert.h"
#include "asset/lookup.h"
#include "recast/Detour/Include/DetourCommon.h"
#include "recast/Recast/Include/Recast.h"
#include "recast/Detour/Include/DetourAlloc.h"
#include "recast/Detour/Include/DetourNavMesh.h"
#include "recast/Detour/Include/DetourNavMeshBuilder.h"
#include "recast/DetourTileCache/Include/DetourTileCache.h"
#include "recast/DetourTileCache/Include/DetourTileCacheBuilder.h"
#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include "cjson/cJSON.h"
#include "ai.h"

namespace VI
{

LoopSwapper* Loader::swapper;
// First entry in each array is empty
Array<Loader::Entry<Mesh> > Loader::meshes;
Array<Loader::Entry<Animation> > Loader::animations;
Array<Loader::Entry<Armature> > Loader::armatures;
Array<Loader::Entry<void*> > Loader::textures;
Array<Loader::Entry<void*> > Loader::shaders;
Array<Loader::Entry<Font> > Loader::fonts;
Array<Loader::Entry<void*> > Loader::dynamic_meshes;
Array<Loader::Entry<void*> > Loader::dynamic_textures;
Array<Loader::Entry<void*> > Loader::framebuffers;
Array<Loader::Entry<AkBankID> > Loader::soundbanks;
dtNavMesh* Loader::current_nav_mesh = nullptr;
dtTileCache* Loader::nav_tile_cache = nullptr;
dtTileCacheAlloc Loader::nav_tile_allocator;
FastLZCompressor Loader::nav_tile_compressor;
NavMeshProcess Loader::nav_tile_mesh_process;
AssetID Loader::current_nav_mesh_id = AssetNull;

s32 Loader::static_mesh_count = 0;
s32 Loader::static_texture_count = 0;

Settings Loader::settings_data = Settings();

struct Attrib
{
	RenderDataType type;
	s32 count;
	Array<char> data;
};

// Recast nav mesh structs

/// Contains triangle meshes that represent detailed height data associated 
/// with the polygons in its associated polygon mesh object.
struct rcPolyMeshDetail
{
	u32* meshes;	///< The sub-mesh data. [Size: 4*#nmeshes] 
	r32* verts;			///< The mesh vertices. [Size: 3*#nverts] 
	u8* tris;	///< The mesh triangles. [Size: 4*#ntris] 
	s32 nmeshes;			///< The number of sub-meshes defined by #meshes.
	s32 nverts;				///< The number of vertices in #verts.
	s32 ntris;				///< The number of triangles in #tris.
};

void Loader::init(LoopSwapper* s)
{
	swapper = s;

	const char* p;
	while ((p = AssetLookup::Mesh::names[static_mesh_count]))
		static_mesh_count++;
	while ((p = AssetLookup::Texture::names[static_texture_count]))
		static_texture_count++;

	RenderSync* sync = swapper->get();
	s32 i = 0;
	const char* uniform_name;
	while ((uniform_name = AssetLookup::Uniform::names[i]))
	{
		sync->write(RenderOp::AllocUniform);
		sync->write(i);
		s32 length = strlen(uniform_name);
		sync->write(length);
		sync->write(uniform_name, length);
		i++;
	}
}

Mesh* Loader::mesh(AssetID id)
{
	if (id == AssetNull)
		return 0;

	if (id >= meshes.length)
		meshes.resize(id + 1);
	if (meshes[id].type == AssetNone)
	{
		const char* path = AssetLookup::Mesh::values[id];
		Mesh* mesh = &meshes[id].data;

		FILE* f = fopen(path, "rb");
		if (!f)
		{
			fprintf(stderr, "Can't open msh file '%s'\n", path);
			return 0;
		}

		new (mesh)Mesh();

		// Read color
		fread(&mesh->color, sizeof(Vec4), 1, f);

		// Read bounding box
		fread(&mesh->bounds_min, sizeof(Vec3), 1, f);
		fread(&mesh->bounds_max, sizeof(Vec3), 1, f);
		fread(&mesh->bounds_radius, sizeof(r32), 1, f);

		// Read indices
		s32 index_count;
		fread(&index_count, sizeof(s32), 1, f);

		// Fill face indices
		mesh->indices.resize(index_count);
		fread(mesh->indices.data, sizeof(s32), index_count, f);

		s32 vertex_count;
		fread(&vertex_count, sizeof(s32), 1, f);

		// Fill vertices positions
		mesh->vertices.resize(vertex_count);
		fread(mesh->vertices.data, sizeof(Vec3), vertex_count, f);

		// Fill normals
		mesh->normals.resize(vertex_count);
		fread(mesh->normals.data, sizeof(Vec3), vertex_count, f);

		s32 extra_attrib_count;
		fread(&extra_attrib_count, sizeof(s32), 1, f);
		Array<Attrib> extra_attribs(extra_attrib_count, extra_attrib_count);
		for (s32 i = 0; i < extra_attribs.length; i++)
		{
			Attrib& a = extra_attribs[i];
			fread(&a.type, sizeof(RenderDataType), 1, f);
			fread(&a.count, sizeof(s32), 1, f);
			a.data.resize(mesh->vertices.length * a.count * render_data_type_size(a.type));
			fread(a.data.data, sizeof(char), a.data.length, f);
		}

		fclose(f);

		// GL
		RenderSync* sync = swapper->get();
		sync->write(RenderOp::AllocMesh);
		sync->write<s32>(id);
		sync->write<b8>(false); // Whether the buffers should be dynamic or not

		sync->write<s32>(2 + extra_attribs.length); // Attribute count

		sync->write(RenderDataType::Vec3); // Position
		sync->write<s32>(1); // Number of data elements per vertex

		sync->write(RenderDataType::Vec3); // Normal
		sync->write<s32>(1); // Number of data elements per vertex

		for (s32 i = 0; i < extra_attribs.length; i++)
		{
			Attrib* a = &extra_attribs[i];
			sync->write<RenderDataType>(a->type);
			sync->write<s32>(a->count);
		}

		sync->write(RenderOp::UpdateAttribBuffers);
		sync->write<s32>(id);

		sync->write<s32>(mesh->vertices.length);
		sync->write(mesh->vertices.data, mesh->vertices.length);
		sync->write(mesh->normals.data, mesh->vertices.length);

		for (s32 i = 0; i < extra_attribs.length; i++)
		{
			Attrib* a = &extra_attribs[i];
			sync->write(a->data.data, a->data.length);
		}

		sync->write(RenderOp::UpdateIndexBuffer);
		sync->write<s32>(id);
		sync->write<s32>(mesh->indices.length);
		sync->write(mesh->indices.data, mesh->indices.length);

		meshes[id].type = AssetTransient;
	}
	return &meshes[id].data;
}

Mesh* Loader::mesh_permanent(const AssetID id)
{
	Mesh* m = mesh(id);
	if (m)
		meshes[id].type = AssetPermanent;
	return m;
}

Mesh* Loader::mesh_instanced(AssetID id)
{
	Mesh* m = mesh(id);
	if (m && !m->instanced)
	{
		RenderSync* sync = swapper->get();
		sync->write(RenderOp::AllocInstances);
		sync->write<s32>(id);
		m->instanced = true;
	}
	return m;
}

void Loader::mesh_free(const AssetID id)
{
	if (id != AssetNull && meshes[id].type != AssetNone)
	{
		meshes[id].data.~Mesh();
		RenderSync* sync = swapper->get();
		sync->write(RenderOp::FreeMesh);
		sync->write<s32>(id);
		meshes[id].type = AssetNone;
	}
}

Armature* Loader::armature(AssetID id)
{
	if (id == AssetNull)
		return 0;

	if (id >= armatures.length)
		armatures.resize(id + 1);
	if (armatures[id].type == AssetNone)
	{
		const char* path = AssetLookup::Armature::values[id];
		FILE* f = fopen(path, "rb");
		if (!f)
		{
			fprintf(stderr, "Can't open arm file '%s'\n", path);
			return 0;
		}

		Armature* arm = &armatures[id].data;
		new (arm) Armature();

		s32 bones;
		fread(&bones, sizeof(s32), 1, f);
		arm->hierarchy.resize(bones);
		fread(arm->hierarchy.data, sizeof(s32), bones, f);
		arm->bind_pose.resize(bones);
		arm->inverse_bind_pose.resize(bones);
		arm->abs_bind_pose.resize(bones);
		fread(arm->bind_pose.data, sizeof(Bone), bones, f);
		fread(arm->inverse_bind_pose.data, sizeof(Mat4), bones, f);
		for (s32 i = 0; i < arm->inverse_bind_pose.length; i++)
			arm->abs_bind_pose[i] = arm->inverse_bind_pose[i].inverse();

		s32 bodies;
		fread(&bodies, sizeof(s32), 1, f);
		arm->bodies.resize(bodies);
		fread(arm->bodies.data, sizeof(BodyEntry), bodies, f);

		fclose(f);

		armatures[id].type = AssetTransient;
	}
	return &armatures[id].data;
}

Armature* Loader::armature_permanent(const AssetID id)
{
	Armature* m = armature(id);
	if (m)
		armatures[id].type = AssetPermanent;
	return m;
}

void Loader::armature_free(AssetID id)
{
	if (id != AssetNull && armatures[id].type != AssetNone)
	{
		armatures[id].data.~Armature();
		armatures[id].type = AssetNone;
	}
}

s32 Loader::dynamic_mesh(s32 attribs, b8 dynamic)
{
	s32 index = AssetNull;
	for (s32 i = 0; i < dynamic_meshes.length; i++)
	{
		if (dynamic_meshes[i].type == AssetNone)
		{
			index = static_mesh_count + i;
			break;
		}
	}

	if (index == AssetNull)
	{
		index = static_mesh_count + dynamic_meshes.length;
		dynamic_meshes.add();
	}

	dynamic_meshes[index - static_mesh_count].type = AssetTransient;

	RenderSync* sync = swapper->get();
	sync->write(RenderOp::AllocMesh);
	sync->write<s32>(index);
	sync->write<b8>(dynamic);
	sync->write<s32>(attribs);

	return index;
}

// Must be called immediately after dynamic_mesh() or dynamic_mesh_permanent()
void Loader::dynamic_mesh_attrib(RenderDataType type, s32 count)
{
	RenderSync* sync = swapper->get();
	sync->write(type);
	sync->write(count);
}

s32 Loader::dynamic_mesh_permanent(s32 attribs, b8 dynamic)
{
	s32 result = dynamic_mesh(attribs, dynamic);
	dynamic_meshes[result - static_mesh_count].type = AssetPermanent;
	return result;
}

void Loader::dynamic_mesh_free(s32 id)
{
	if (id != AssetNull && dynamic_meshes[id].type != AssetNone)
	{
		RenderSync* sync = swapper->get();
		sync->write(RenderOp::FreeMesh);
		sync->write<s32>(id);
		dynamic_meshes[id - static_mesh_count].type = AssetNone;
	}
}

Animation* Loader::animation(AssetID id)
{
	if (id == AssetNull)
		return 0;

	if (id >= animations.length)
		animations.resize(id + 1);
	if (animations[id].type == AssetNone)
	{
		const char* path = AssetLookup::Animation::values[id];
		FILE* f = fopen(path, "rb");
		if (!f)
		{
			fprintf(stderr, "Can't open anm file '%s'\n", path);
			return 0;
		}

		Animation* anim = &animations[id].data;
		new (anim)Animation();

		fread(&anim->duration, sizeof(r32), 1, f);

		s32 channel_count;
		fread(&channel_count, sizeof(s32), 1, f);
		anim->channels.reserve(channel_count);
		anim->channels.length = channel_count;

		for (s32 i = 0; i < channel_count; i++)
		{
			Channel* channel = &anim->channels[i];
			fread(&channel->bone_index, sizeof(s32), 1, f);
			s32 position_count;
			fread(&position_count, sizeof(s32), 1, f);
			channel->positions.reserve(position_count);
			channel->positions.length = position_count;
			fread(channel->positions.data, sizeof(Keyframe<Vec3>), position_count, f);

			s32 rotation_count;
			fread(&rotation_count, sizeof(s32), 1, f);
			channel->rotations.reserve(rotation_count);
			channel->rotations.length = rotation_count;
			fread(channel->rotations.data, sizeof(Keyframe<Quat>), rotation_count, f);

			s32 scale_count;
			fread(&scale_count, sizeof(s32), 1, f);
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

void Loader::animation_free(AssetID id)
{
	if (id != AssetNull && animations[id].type != AssetNone)
	{
		animations[id].data.~Animation();
		animations[id].type = AssetNone;
	}
}

void Loader::texture(AssetID id, RenderTextureWrap wrap, RenderTextureFilter filter)
{
	if (id == AssetNull)
		return;

	if (id >= textures.length)
		textures.resize(id + 1);
	if (textures[id].type == AssetNone)
	{
		textures[id].type = AssetTransient;

		const char* path = AssetLookup::Texture::values[id];
		u8* buffer;
		u32 width, height;

		u32 error = lodepng_decode32_file(&buffer, &width, &height, path);

		if (error)
		{
			fprintf(stderr, "Error loading texture '%s': %s\n", path, lodepng_error_text(error));
			return;
		}

		RenderSync* sync = swapper->get();
		sync->write(RenderOp::AllocTexture);
		sync->write<AssetID>(id);
		sync->write(RenderOp::LoadTexture);
		sync->write<AssetID>(id);
		sync->write(wrap);
		sync->write(filter);
		sync->write<u32>(&width);
		sync->write<u32>(&height);
		sync->write<u8>(buffer, 4 * width * height);
		free(buffer);
	}
}

void Loader::texture_permanent(AssetID id, RenderTextureWrap wrap, RenderTextureFilter filter)
{
	texture(id);
	if (id != AssetNull)
		textures[id].type = AssetPermanent;
}

void Loader::texture_free(AssetID id)
{
	if (id != AssetNull && textures[id].type != AssetNone)
	{
		RenderSync* sync = swapper->get();
		sync->write(RenderOp::FreeTexture);
		sync->write<AssetID>(id);
		textures[id].type = AssetNone;
	}
}

s32 Loader::dynamic_texture(s32 width, s32 height, RenderDynamicTextureType type, RenderTextureWrap wrap, RenderTextureFilter filter, RenderTextureCompare compare)
{
	s32 index = AssetNull;
	for (s32 i = 0; i < dynamic_textures.length; i++)
	{
		if (dynamic_textures[i].type == AssetNone)
		{
			index = static_texture_count + i;
			break;
		}
	}

	if (index == AssetNull)
	{
		index = static_texture_count + dynamic_textures.length;
		dynamic_textures.add();
	}

	dynamic_textures[index - static_texture_count].type = AssetTransient;

	RenderSync* sync = swapper->get();
	sync->write(RenderOp::AllocTexture);
	sync->write<AssetID>(index);
	sync->write(RenderOp::DynamicTexture);
	sync->write<AssetID>(index);
	sync->write<u32>(width);
	sync->write<u32>(height);
	sync->write<RenderDynamicTextureType>(type);
	sync->write<RenderTextureWrap>(wrap);
	sync->write<RenderTextureFilter>(filter);
	sync->write<RenderTextureCompare>(compare);

	return index;
}

s32 Loader::dynamic_texture_permanent(s32 width, s32 height, RenderDynamicTextureType type, RenderTextureWrap wrap, RenderTextureFilter filter, RenderTextureCompare compare)
{
	s32 id = dynamic_texture(width, height, type, wrap, filter, compare);
	if (id != AssetNull)
		dynamic_textures[id - static_texture_count].type = AssetPermanent;
	return id;
}

void Loader::dynamic_texture_free(s32 id)
{
	if (id != AssetNull && dynamic_textures[id - static_texture_count].type != AssetNone)
	{
		RenderSync* sync = swapper->get();
		sync->write(RenderOp::FreeTexture);
		sync->write<AssetID>(id);
		dynamic_textures[id - static_texture_count].type = AssetNone;
	}
}

s32 Loader::framebuffer(s32 attachments)
{
	s32 index = AssetNull;
	for (s32 i = 0; i < framebuffers.length; i++)
	{
		if (framebuffers[i].type == AssetNone)
		{
			index = i;
			break;
		}
	}

	if (index == AssetNull)
	{
		index = framebuffers.length;
		framebuffers.add();
	}

	framebuffers[index].type = AssetTransient;

	RenderSync* sync = swapper->get();
	sync->write(RenderOp::AllocFramebuffer);
	sync->write<s32>(index);
	sync->write<s32>(attachments);

	return index;
}

// Must be called immediately after framebuffer() or framebuffer_permanent()
void Loader::framebuffer_attach(RenderFramebufferAttachment attachment_type, s32 dynamic_texture)
{
	RenderSync* sync = swapper->get();
	sync->write<RenderFramebufferAttachment>(attachment_type);
	sync->write<s32>(dynamic_texture);
}

s32 Loader::framebuffer_permanent(s32 attachments)
{
	s32 id = framebuffer(attachments);
	if (id != AssetNull)
		framebuffers[id].type = AssetPermanent;
	return id;
}

void Loader::framebuffer_free(s32 id)
{
	if (id != AssetNull && framebuffers[id].type != AssetNone)
	{
		RenderSync* sync = swapper->get();
		sync->write(RenderOp::FreeFramebuffer);
		sync->write<s32>(id);
		framebuffers[id].type = AssetNone;
	}
}

void Loader::shader(AssetID id)
{
	if (id == AssetNull)
		return;

	if (id >= shaders.length)
		shaders.resize(id + 1);
	if (shaders[id].type == AssetNone)
	{
		shaders[id].type = AssetTransient;

		const char* path = AssetLookup::Shader::values[id];

		Array<char> code;
		FILE* f = fopen(path, "r");
		if (!f)
		{
			fprintf(stderr, "Can't open shader source file '%s'", path);
			return;
		}

		const s32 chunk_size = 4096;
		s32 i = 1;
		while (true)
		{
			code.reserve(i * chunk_size + 1); // extra char since this will be a null-terminated string
			s32 read = fread(&code.data[(i - 1) * chunk_size], sizeof(char), chunk_size, f);
			if (read < chunk_size)
			{
				code.length = ((i - 1) * chunk_size) + read;
				break;
			}
			i++;
		}
		fclose(f);

		RenderSync* sync = swapper->get();
		sync->write(RenderOp::LoadShader);
		sync->write<AssetID>(id);
		sync->write<s32>(&code.length);
		sync->write(code.data, code.length);
	}
}

void Loader::shader_permanent(AssetID id)
{
	shader(id);
	if (id != AssetNull)
		shaders[id].type = AssetPermanent;
}

void Loader::shader_free(AssetID id)
{
	if (id != AssetNull && shaders[id].type != AssetNone)
	{
		RenderSync* sync = swapper->get();
		sync->write(RenderOp::FreeShader);
		sync->write<AssetID>(id);
		shaders[id].type = AssetNone;
	}
}

Font* Loader::font(AssetID id)
{
	if (id == AssetNull)
		return 0;

	if (id >= fonts.length)
		fonts.resize(id + 1);
	if (fonts[id].type == AssetNone)
	{
		const char* path = AssetLookup::Font::values[id];
		FILE* f = fopen(path, "rb");
		if (!f)
		{
			fprintf(stderr, "Can't open fnt file '%s'\n", path);
			return 0;
		}

		Font* font = &fonts[id].data;
		new (font)Font();

		s32 j;

		fread(&j, sizeof(s32), 1, f);
		font->vertices.resize(j);
		fread(font->vertices.data, sizeof(Vec3), font->vertices.length, f);

		fread(&j, sizeof(s32), 1, f);
		font->indices.resize(j);
		fread(font->indices.data, sizeof(s32), font->indices.length, f);

		fread(&j, sizeof(s32), 1, f);
		Array<Font::Character> characters;
		characters.resize(j);
		fread(characters.data, sizeof(Font::Character), characters.length, f);
		for (s32 i = 0; i < characters.length; i++)
		{
			Font::Character* c = &characters[i];
			if (c->code >= font->characters.length)
				font->characters.resize(c->code + 1);
			font->characters[c->code] = *c;
		}
		font->characters[' '].code = ' ';
		font->characters[' '].max.x = 0.3f;
		font->characters['\t'].code = ' ';
		font->characters['\t'].max.x = 1.5f;

		fclose(f);

		fonts[id].type = AssetTransient;
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

void Loader::font_free(AssetID id)
{
	if (id != AssetNull && fonts[id].type != AssetNone)
	{
		fonts[id].data.~Font();
		fonts[id].type = AssetNone;
	}
}

cJSON* Loader::level(AssetID id)
{
	if (id == AssetNull)
		return 0;
	
	return Json::load(AssetLookup::Level::values[id]);
}

void Loader::level_free(cJSON* json)
{
	Json::json_free(json);
}

dtNavMesh* Loader::nav_mesh(AssetID id)
{
	// Only allow one nav mesh to be loaded at a time
	vi_assert(current_nav_mesh_id == AssetNull || current_nav_mesh_id == id);

	if (id == AssetNull)
		return nullptr;
	
	if (current_nav_mesh_id == AssetNull)
	{
		const char* path = AssetLookup::NavMesh::values[id];

		TileCacheData tiles;

		FILE* f = fopen(path, "rb");
		if (f)
		{
			if (fread(&tiles.min, sizeof(Vec3), 1, f))
			{
				fread(&tiles.width, sizeof(s32), 1, f);
				fread(&tiles.height, sizeof(s32), 1, f);
				tiles.cells.resize(tiles.width * tiles.height);
				for (s32 i = 0; i < tiles.cells.length; i++)
				{
					TileCacheCell& cell = tiles.cells[i];
					s32 layer_count;
					fread(&layer_count, sizeof(s32), 1, f);
					cell.layers.resize(layer_count);
					for (s32 j = 0; j < layer_count; j++)
					{
						TileCacheLayer& layer = cell.layers[j];
						fread(&layer.data_size, sizeof(s32), 1, f);
						layer.data = (u8*)dtAlloc(layer.data_size, dtAllocHint::DT_ALLOC_PERM);
						fread(layer.data, sizeof(u8), layer.data_size, f);
					}
				}
			}
			fclose(f);
		}

		if (tiles.width > 0)
		{
			// Create Detour navmesh

			current_nav_mesh = dtAllocNavMesh();
			vi_assert(current_nav_mesh);

			dtNavMeshParams params;
			memset(&params, 0, sizeof(params));
			rcVcopy(params.orig, (r32*)&tiles.min);
			params.tileWidth = nav_tile_size * nav_resolution;
			params.tileHeight = nav_tile_size * nav_resolution;

			s32 tileBits = rcMin((s32)dtIlog2(dtNextPow2(tiles.width * tiles.height * nav_expected_layers_per_tile)), 14);
			if (tileBits > 14) tileBits = 14;
			s32 polyBits = 22 - tileBits;
			params.maxTiles = 1 << tileBits;
			params.maxPolys = 1 << polyBits;

			{
				dtStatus status = current_nav_mesh->init(&params);
				vi_assert(dtStatusSucceed(status));
			}

			// Create Detour tile cache

			dtTileCacheParams tcparams;
			memset(&tcparams, 0, sizeof(tcparams));
			memcpy(&tcparams.orig, &tiles.min, sizeof(tiles.min));
			tcparams.cs = nav_resolution;
			tcparams.ch = nav_resolution;
			tcparams.width = (s32)nav_tile_size;
			tcparams.height = (s32)nav_tile_size;
			tcparams.walkableHeight = nav_agent_height;
			tcparams.walkableRadius = nav_agent_radius;
			tcparams.walkableClimb = nav_agent_max_climb;
			tcparams.maxSimplificationError = nav_mesh_max_error;
			tcparams.maxTiles = tiles.width * tiles.height * nav_expected_layers_per_tile;
			tcparams.maxObstacles = nav_max_obstacles;

			nav_tile_cache = dtAllocTileCache();
			vi_assert(nav_tile_cache);
			{
				dtStatus status = nav_tile_cache->init(&tcparams, &nav_tile_allocator, &nav_tile_compressor, &nav_tile_mesh_process);
				vi_assert(!dtStatusFailed(status));
			}

			for (s32 ty = 0; ty < tiles.height; ty++)
			{
				for (s32 tx = 0; tx < tiles.width; tx++)
				{
					TileCacheCell& cell = tiles.cells[tx + ty * tiles.width];
					for (s32 i = 0; i < cell.layers.length; i++)
					{
						TileCacheLayer& tile = cell.layers[i];
						dtStatus status = nav_tile_cache->addTile(tile.data, tile.data_size, DT_COMPRESSEDTILE_FREE_DATA, 0);
						vi_assert(dtStatusSucceed(status));
					}
				}
			}

			// Build initial meshes
			for (s32 ty = 0; ty < tiles.height; ty++)
			{
				for (s32 tx = 0; tx < tiles.width; tx++)
				{
					dtStatus status = nav_tile_cache->buildNavMeshTilesAt(tx, ty, current_nav_mesh);
					vi_assert(dtStatusSucceed(status));
				}
			}
		}
	}

	current_nav_mesh_id = id;

	return current_nav_mesh;
}

b8 Loader::soundbank(AssetID id)
{
	if (id == AssetNull)
		return false;

	if (id >= soundbanks.length)
		soundbanks.resize(id + 1);
	if (soundbanks[id].type == AssetNone)
	{
		soundbanks[id].type = AssetTransient;

		const char* path = AssetLookup::Soundbank::values[id];

		if (AK::SoundEngine::LoadBank(AssetLookup::Soundbank::values[id], AK_DEFAULT_POOL_ID, soundbanks[id].data) != AK_Success)
		{
			fprintf(stderr, "Failed to load soundbank '%s'\n", path);
			return false;
		}
	}

	return true;
}

b8 Loader::soundbank_permanent(AssetID id)
{
	b8 success = soundbank(id);
	if (success)
		soundbanks[id].type = AssetPermanent;
	return success;
}

void Loader::soundbank_free(AssetID id)
{
	if (id != AssetNull && soundbanks[id].type != AssetNone)
	{
		soundbanks[id].type = AssetNone;
		AK::SoundEngine::UnloadBank(soundbanks[id].data, nullptr);
	}
}

void Loader::transients_free()
{
	if (current_nav_mesh)
	{
		dtFreeNavMesh(current_nav_mesh);
		current_nav_mesh = nullptr;
	}
	current_nav_mesh_id = AssetNull;
	if (nav_tile_cache)
	{
		dtFreeTileCache(nav_tile_cache);
		nav_tile_cache = nullptr;
	}

	for (AssetID i = 0; i < meshes.length; i++)
	{
		if (meshes[i].type == AssetTransient)
			mesh_free(i);
	}

	for (AssetID i = 0; i < textures.length; i++)
	{
		if (textures[i].type == AssetTransient)
			texture_free(i);
	}

	for (AssetID i = 0; i < shaders.length; i++)
	{
		if (shaders[i].type == AssetTransient)
			shader_free(i);
	}

	for (AssetID i = 0; i < fonts.length; i++)
	{
		if (fonts[i].type == AssetTransient)
			font_free(i);
	}

	for (AssetID i = 0; i < dynamic_meshes.length; i++)
	{
		if (dynamic_meshes[i].type == AssetTransient)
			dynamic_mesh_free(static_mesh_count + i);
	}

	for (s32 i = 0; i < dynamic_textures.length; i++)
	{
		if (dynamic_textures[i].type == AssetTransient)
			dynamic_texture_free(static_texture_count + i);
	}

	for (s32 i = 0; i < framebuffers.length; i++)
	{
		if (framebuffers[i].type == AssetTransient)
			framebuffer_free(i);
	}

	for (AssetID i = 0; i < soundbanks.length; i++)
	{
		if (soundbanks[i].type == AssetTransient)
			soundbank_free(i);
	}
}

AssetID Loader::find(const char* name, const char** list)
{
	if (!name)
		return AssetNull;
	const char* p;
	s32 i = 0;
	while ((p = list[i]))
	{
		if (strcmp(name, p) == 0)
			return i;
		i++;
	}
	return AssetNull;
}

InputBinding input_binding(cJSON* parent, const char* key, const InputBinding& default_value)
{
	if (!parent)
		return default_value;

	cJSON* json = cJSON_GetObjectItem(parent, key);
	if (!json)
		return default_value;

	InputBinding binding;
	binding.key = (KeyCode)Json::get_s32(json, "key", (s32)default_value.key);
	binding.btn = (Gamepad::Btn)Json::get_s32(json, "btn", (s32)default_value.btn);
	return binding;
}

cJSON* input_binding_json(const InputBinding& binding)
{
	cJSON* json = cJSON_CreateObject();
	if (binding.key != KeyCode::None)
		cJSON_AddNumberToObject(json, "key", (s32)binding.key);
	if (binding.btn != Gamepad::Btn::None)
		cJSON_AddNumberToObject(json, "btn", (s32)binding.btn);
	return json;
}

Settings& Loader::settings()
{
	if (!settings_data.valid)
	{
		settings_data.valid = true;

		cJSON* json = Json::load("config.txt");
		settings_data.width = Json::get_s32(json, "width", 1920);
		settings_data.height = Json::get_s32(json, "height", 1080);
		settings_data.fullscreen = (b8)Json::get_s32(json, "fullscreen", 0);
		settings_data.sfx = (u8)Json::get_s32(json, "sfx", 100);
		settings_data.music = (u8)Json::get_s32(json, "music", 100);

		cJSON* bindings = cJSON_GetObjectItem(json, "bindings");
		settings_data.bindings.backward = input_binding(bindings, "backward", { KeyCode::S, Gamepad::Btn::None });
		settings_data.bindings.forward = input_binding(bindings, "forward", { KeyCode::W, Gamepad::Btn::None });
		settings_data.bindings.left = input_binding(bindings, "left", { KeyCode::A, Gamepad::Btn::None });
		settings_data.bindings.right = input_binding(bindings, "right", { KeyCode::D, Gamepad::Btn::None });
		settings_data.bindings.up = input_binding(bindings, "up", { KeyCode::Space, Gamepad::Btn::RightShoulder });
		settings_data.bindings.down = input_binding(bindings, "down", { KeyCode::LCtrl, Gamepad::Btn::LeftShoulder });
		settings_data.bindings.jump = input_binding(bindings, "jump", { KeyCode::Space, Gamepad::Btn::RightTrigger });
		settings_data.bindings.primary = input_binding(bindings, "primary", { KeyCode::MouseLeft, Gamepad::Btn::RightTrigger });
		settings_data.bindings.secondary = input_binding(bindings, "secondary", { KeyCode::MouseRight, Gamepad::Btn::LeftTrigger });
		settings_data.bindings.parkour = input_binding(bindings, "parkour", { KeyCode::LShift, Gamepad::Btn::LeftTrigger });
		settings_data.bindings.slide = input_binding(bindings, "slide", { KeyCode::MouseLeft, Gamepad::Btn::LeftClick });
		settings_data.bindings.ability1 = input_binding(bindings, "ability1", { KeyCode::Q, Gamepad::Btn::X });
		settings_data.bindings.ability1 = input_binding(bindings, "ability1", { KeyCode::E, Gamepad::Btn::Y });
		settings_data.bindings.upgrade = input_binding(bindings, "upgrade", { KeyCode::Tab, Gamepad::Btn::A });
	}
	return settings_data;
}

void Loader::settings_save()
{
	if (settings_data.valid)
	{
		cJSON* json = cJSON_CreateObject();
		cJSON_AddNumberToObject(json, "width", settings_data.width);
		cJSON_AddNumberToObject(json, "height", settings_data.height);
		cJSON_AddNumberToObject(json, "fullscreen", settings_data.fullscreen);
		cJSON_AddNumberToObject(json, "sfx", settings_data.sfx);
		cJSON_AddNumberToObject(json, "music", settings_data.music);

		cJSON* bindings = cJSON_CreateObject();
		cJSON_AddItemToObject(json, "bindings", bindings);

		cJSON_AddItemToObject(bindings, "backward", input_binding_json(settings_data.bindings.backward));
		cJSON_AddItemToObject(bindings, "forward", input_binding_json(settings_data.bindings.forward));
		cJSON_AddItemToObject(bindings, "left", input_binding_json(settings_data.bindings.left));
		cJSON_AddItemToObject(bindings, "right", input_binding_json(settings_data.bindings.right));
		cJSON_AddItemToObject(bindings, "up", input_binding_json(settings_data.bindings.up));
		cJSON_AddItemToObject(bindings, "down", input_binding_json(settings_data.bindings.down));
		cJSON_AddItemToObject(bindings, "jump", input_binding_json(settings_data.bindings.jump));
		cJSON_AddItemToObject(bindings, "primary", input_binding_json(settings_data.bindings.primary));
		cJSON_AddItemToObject(bindings, "secondary", input_binding_json(settings_data.bindings.secondary));
		cJSON_AddItemToObject(bindings, "parkour", input_binding_json(settings_data.bindings.parkour));
		cJSON_AddItemToObject(bindings, "slide", input_binding_json(settings_data.bindings.slide));
		cJSON_AddItemToObject(bindings, "ability1", input_binding_json(settings_data.bindings.ability1));
		cJSON_AddItemToObject(bindings, "ability2", input_binding_json(settings_data.bindings.ability2));
		cJSON_AddItemToObject(bindings, "upgrade", input_binding_json(settings_data.bindings.upgrade));

		Json::save(json, "config.txt");
		Json::json_free(json);
	}
}

}
