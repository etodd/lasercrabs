#include "load.h"
#include <stdio.h>
#include "lodepng/lodepng.h"
#include "vi_assert.h"
#include "asset/lookup.h"
#include "recast/Detour/Include/DetourNavMesh.h"
#include "recast/Detour/Include/DetourNavMeshBuilder.h"
#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include "cjson/cJSON.h"

namespace VI
{

LoopSwapper* Loader::swapper;
// First entry in each array is empty
Array<Loader::Entry<Mesh> > Loader::meshes = Array<Loader::Entry<Mesh> >();
Array<Loader::Entry<Animation> > Loader::animations = Array<Loader::Entry<Animation> >();
Array<Loader::Entry<Armature> > Loader::armatures = Array<Loader::Entry<Armature> >();
Array<Loader::Entry<void*> > Loader::textures = Array<Loader::Entry<void*> >();
Array<Loader::Entry<void*> > Loader::shaders = Array<Loader::Entry<void*> >();
Array<Loader::Entry<Font> > Loader::fonts = Array<Loader::Entry<Font> >();
Array<Loader::Entry<void*> > Loader::dynamic_meshes = Array<Loader::Entry<void*> >();
Array<Loader::Entry<void*> > Loader::dynamic_textures = Array<Loader::Entry<void*> >();
Array<Loader::Entry<void*> > Loader::framebuffers = Array<Loader::Entry<void*> >();
Array<Loader::Entry<AkBankID> > Loader::soundbanks = Array<Loader::Entry<AkBankID> >();
dtNavMesh* Loader::current_nav_mesh;
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

void base_nav_mesh_read(FILE* f, rcPolyMesh* mesh)
{
	if (!fread(mesh, sizeof(rcPolyMesh), 1, f))
		memset(mesh, 0, sizeof(rcPolyMesh));
	else
	{
		mesh->verts = (u16*)malloc(sizeof(u16) * 3 * mesh->nverts);
		mesh->polys = (u16*)malloc(sizeof(u16) * 2 * mesh->nvp * mesh->npolys);
		mesh->regs = (u16*)malloc(sizeof(u16) * mesh->npolys);
		mesh->flags = (u16*)malloc(sizeof(u16) * mesh->npolys);
		mesh->areas = (u8*)malloc(sizeof(u8) * mesh->npolys);
		fread(mesh->verts, sizeof(u16) * 3, mesh->nverts, f);
		fread(mesh->polys, sizeof(u16) * 2 * mesh->nvp, mesh->npolys, f);
		fread(mesh->regs, sizeof(u16), mesh->npolys, f);
		fread(mesh->flags, sizeof(u16), mesh->npolys, f);
		fread(mesh->areas, sizeof(u8), mesh->npolys, f);
	}
}

// Debug function for rendering the nav mesh.
// You're responsible for keeping track of the result.
void Loader::base_nav_mesh(AssetID id, rcPolyMesh* mesh)
{
	if (id == AssetNull)
		return;

	const char* path = AssetLookup::NavMesh::values[id];

	FILE* f = fopen(path, "rb");
	if (!f)
	{
		fprintf(stderr, "Can't open nav file '%s'\n", path);
		return;
	}

	base_nav_mesh_read(f, mesh);

	fclose(f);
}

void Loader::base_nav_mesh_free(rcPolyMesh* mesh)
{
	free(mesh->verts);
	free(mesh->polys);
	free(mesh->regs);
	free(mesh->flags);
	free(mesh->areas);
}

dtNavMesh* Loader::nav_mesh(AssetID id)
{
	// Only allow one nav mesh to be loaded at a time
	vi_assert(current_nav_mesh_id == AssetNull || current_nav_mesh_id == id);

	if (id == AssetNull)
		return 0;
	
	if (current_nav_mesh_id == AssetNull)
	{
		const char* path = AssetLookup::NavMesh::values[id];

		FILE* f = fopen(path, "rb");
		if (!f)
		{
			fprintf(stderr, "Can't open nav file '%s'\n", path);
			return 0;
		}

		rcPolyMesh mesh;
		base_nav_mesh_read(f, &mesh);

		if (mesh.npolys > 0)
		{
			rcPolyMeshDetail mesh_detail;
			fread(&mesh_detail, sizeof(rcPolyMeshDetail), 1, f);
			mesh_detail.meshes = (u32*)malloc(sizeof(u32) * 4 * mesh_detail.nmeshes);
			mesh_detail.verts = (r32*)malloc(sizeof(r32) * 3 * mesh_detail.nverts);
			mesh_detail.tris = (u8*)malloc(sizeof(u8) * 4 * mesh_detail.ntris);
			fread(mesh_detail.meshes, sizeof(u32) * 4, mesh_detail.nmeshes, f);
			fread(mesh_detail.verts, sizeof(r32) * 3, mesh_detail.nverts, f);
			fread(mesh_detail.tris, sizeof(u8) * 4, mesh_detail.ntris, f);

			r32 agent_height;
			r32 agent_radius;
			r32 agent_max_climb;

			fread(&agent_height, sizeof(r32), 1, f);
			fread(&agent_radius, sizeof(r32), 1, f);
			fread(&agent_max_climb, sizeof(r32), 1, f);

			fclose(f);

			u8* navData = 0;
			s32 navDataSize = 0;

			dtNavMeshCreateParams params;
			memset(&params, 0, sizeof(params));
			params.verts = mesh.verts;
			params.vertCount = mesh.nverts;
			params.polys = mesh.polys;
			params.polyAreas = mesh.areas;
			params.polyFlags = mesh.flags;
			params.polyCount = mesh.npolys;
			params.nvp = mesh.nvp;
			params.detailMeshes = mesh_detail.meshes;
			params.detailVerts = mesh_detail.verts;
			params.detailVertsCount = mesh_detail.nverts;
			params.detailTris = mesh_detail.tris;
			params.detailTriCount = mesh_detail.ntris;
			params.offMeshConVerts = 0;
			params.offMeshConRad = 0;
			params.offMeshConDir = 0;
			params.offMeshConAreas = 0;
			params.offMeshConFlags = 0;
			params.offMeshConUserID = 0;
			params.offMeshConCount = 0;
			params.walkableHeight = agent_height;
			params.walkableRadius = agent_radius;
			params.walkableClimb = agent_max_climb;
			params.bmin[0] = mesh.bmin[0];
			params.bmin[1] = mesh.bmin[1];
			params.bmin[2] = mesh.bmin[2];
			params.bmax[0] = mesh.bmax[0];
			params.bmax[1] = mesh.bmax[1];
			params.bmax[2] = mesh.bmax[2];
			params.cs = mesh.cs;
			params.ch = mesh.ch;
			params.buildBvTree = true;

			{
				b8 status = dtCreateNavMeshData(&params, &navData, &navDataSize);
				vi_assert(status);
			}

			current_nav_mesh = dtAllocNavMesh();
			vi_assert(current_nav_mesh);

			{
				dtStatus status = current_nav_mesh->init(navData, navDataSize, DT_TILE_FREE_DATA);
				vi_assert(!dtStatusFailed(status));
			}

			base_nav_mesh_free(&mesh);
			free(mesh_detail.meshes);
			free(mesh_detail.verts);
			free(mesh_detail.tris);
		}
		else
			current_nav_mesh = 0;

		current_nav_mesh_id = id;
	}

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
		current_nav_mesh = 0;
	}
	current_nav_mesh_id = AssetNull;

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
		cJSON* json = Json::load("config.txt");
		settings_data.width = Json::get_s32(json, "width", 1920);
		settings_data.height = Json::get_s32(json, "height", 1080);
		settings_data.fullscreen = (b8)Json::get_s32(json, "fullscreen", 0);
		settings_data.valid = true;

		cJSON* bindings = cJSON_GetObjectItem(json, "bindings");
		settings_data.bindings.backward = input_binding(bindings, "backward", { KeyCode::S, Gamepad::Btn::None });
		settings_data.bindings.forward = input_binding(bindings, "forward", { KeyCode::W, Gamepad::Btn::None });
		settings_data.bindings.left = input_binding(bindings, "left", { KeyCode::A, Gamepad::Btn::None });
		settings_data.bindings.right = input_binding(bindings, "right", { KeyCode::D, Gamepad::Btn::None });
		settings_data.bindings.up_jump = input_binding(bindings, "up_jump", { KeyCode::Space, Gamepad::Btn::RightShoulder });
		settings_data.bindings.down = input_binding(bindings, "down", { KeyCode::LCtrl, Gamepad::Btn::LeftShoulder });
		settings_data.bindings.primary = input_binding(bindings, "primary", { KeyCode::MouseLeft, Gamepad::Btn::RightTrigger });
		settings_data.bindings.secondary = input_binding(bindings, "secondary", { KeyCode::MouseRight, Gamepad::Btn::LeftTrigger });
		settings_data.bindings.parkour = input_binding(bindings, "parkour", { KeyCode::LShift, Gamepad::Btn::LeftShoulder });
		settings_data.bindings.walk = input_binding(bindings, "walk", { KeyCode::LCtrl, Gamepad::Btn::LeftClick });
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

		cJSON* bindings = cJSON_CreateObject();
		cJSON_AddItemToObject(json, "bindings", bindings);

		cJSON_AddItemToObject(bindings, "backward", input_binding_json(settings_data.bindings.backward));
		cJSON_AddItemToObject(bindings, "forward", input_binding_json(settings_data.bindings.forward));
		cJSON_AddItemToObject(bindings, "left", input_binding_json(settings_data.bindings.left));
		cJSON_AddItemToObject(bindings, "right", input_binding_json(settings_data.bindings.right));
		cJSON_AddItemToObject(bindings, "up_jump", input_binding_json(settings_data.bindings.up_jump));
		cJSON_AddItemToObject(bindings, "down", input_binding_json(settings_data.bindings.down));
		cJSON_AddItemToObject(bindings, "primary", input_binding_json(settings_data.bindings.primary));
		cJSON_AddItemToObject(bindings, "secondary", input_binding_json(settings_data.bindings.secondary));
		cJSON_AddItemToObject(bindings, "parkour", input_binding_json(settings_data.bindings.parkour));
		cJSON_AddItemToObject(bindings, "walk", input_binding_json(settings_data.bindings.walk));

		Json::save(json, "config.txt");
		Json::json_free(json);
	}
}

}
