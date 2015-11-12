#include "load.h"
#include <stdio.h>
#include "lodepng.h"
#include "vi_assert.h"
#include "asset/lookup.h"
#include "asset/mesh.h"
#include "asset/texture.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"
#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include "cJSON.h"

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

Settings Loader::settings_data = Settings();

struct Attrib
{
	RenderDataType type;
	int count;
	Array<char> data;
};

// Recast nav mesh structs

/// Contains triangle meshes that represent detailed height data associated 
/// with the polygons in its associated polygon mesh object.
struct rcPolyMeshDetail
{
	unsigned int* meshes;	///< The sub-mesh data. [Size: 4*#nmeshes] 
	float* verts;			///< The mesh vertices. [Size: 3*#nverts] 
	unsigned char* tris;	///< The mesh triangles. [Size: 4*#ntris] 
	int nmeshes;			///< The number of sub-meshes defined by #meshes.
	int nverts;				///< The number of vertices in #verts.
	int ntris;				///< The number of triangles in #tris.
};

void Loader::init(LoopSwapper* s)
{
	swapper = s;
	meshes.resize(Asset::Mesh::count);
	textures.resize(Asset::Texture::count);

	RenderSync* sync = swapper->get();
	int i = 0;
	const char* uniform_name;
	while ((uniform_name = AssetLookup::Uniform::names[i]))
	{
		sync->write(RenderOp::AllocUniform);
		sync->write(i);
		int length = strlen(uniform_name);
		sync->write(length);
		sync->write(uniform_name, length);
		i++;
	}
}

Mesh* Loader::mesh(const AssetID id)
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
		fread(&mesh->bounds_radius, sizeof(float), 1, f);

		// Read indices
		int index_count;
		fread(&index_count, sizeof(int), 1, f);

		// Fill face indices
		mesh->indices.resize(index_count);
		fread(mesh->indices.data, sizeof(int), index_count, f);

		int vertex_count;
		fread(&vertex_count, sizeof(int), 1, f);

		// Fill vertices positions
		mesh->vertices.resize(vertex_count);
		fread(mesh->vertices.data, sizeof(Vec3), vertex_count, f);

		// Fill normals
		mesh->normals.resize(vertex_count);
		fread(mesh->normals.data, sizeof(Vec3), vertex_count, f);

		int extra_attrib_count;
		fread(&extra_attrib_count, sizeof(int), 1, f);
		Array<Attrib> extra_attribs(extra_attrib_count, extra_attrib_count);
		for (int i = 0; i < extra_attribs.length; i++)
		{
			Attrib& a = extra_attribs[i];
			fread(&a.type, sizeof(RenderDataType), 1, f);
			fread(&a.count, sizeof(int), 1, f);
			a.data.resize(mesh->vertices.length * a.count * render_data_type_size(a.type));
			fread(a.data.data, sizeof(char), a.data.length, f);
		}

		fclose(f);

		// GL
		RenderSync* sync = swapper->get();
		sync->write(RenderOp::AllocMesh);
		sync->write<int>(id);
		sync->write<bool>(false); // Whether the buffers should be dynamic or not

		sync->write<int>(2 + extra_attribs.length); // Attribute count

		sync->write(RenderDataType::Vec3); // Position
		sync->write<int>(1); // Number of data elements per vertex

		sync->write(RenderDataType::Vec3); // Normal
		sync->write<int>(1); // Number of data elements per vertex

		for (int i = 0; i < extra_attribs.length; i++)
		{
			Attrib* a = &extra_attribs[i];
			sync->write<RenderDataType>(a->type);
			sync->write<int>(a->count);
		}

		sync->write(RenderOp::UpdateAttribBuffers);
		sync->write<int>(id);

		sync->write<int>(mesh->vertices.length);
		sync->write(mesh->vertices.data, mesh->vertices.length);
		sync->write(mesh->normals.data, mesh->vertices.length);

		for (int i = 0; i < extra_attribs.length; i++)
		{
			Attrib* a = &extra_attribs[i];
			sync->write(a->data.data, a->data.length);
		}

		sync->write(RenderOp::UpdateIndexBuffer);
		sync->write<int>(id);
		sync->write<int>(mesh->indices.length);
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

Mesh* Loader::mesh_instanced(const AssetID id)
{
	Mesh* m = mesh(id);
	if (m && !m->instanced)
	{
		RenderSync* sync = swapper->get();
		sync->write(RenderOp::AllocInstances);
		sync->write<int>(id);
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
		sync->write<int>(id);
		meshes[id].type = AssetNone;
	}
}

AssetID Loader::mesh_ref_to_id(const AssetID model, const AssetRef mesh)
{
	return Asset::mesh_refs[model][mesh];
}

Armature* Loader::armature(const AssetID id)
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

		int bones;
		fread(&bones, sizeof(int), 1, f);
		arm->hierarchy.resize(bones);
		fread(arm->hierarchy.data, sizeof(int), bones, f);
		arm->bind_pose.resize(bones);
		arm->inverse_bind_pose.resize(bones);
		arm->abs_bind_pose.resize(bones);
		fread(arm->bind_pose.data, sizeof(Bone), bones, f);
		fread(arm->inverse_bind_pose.data, sizeof(Mat4), bones, f);
		for (int i = 0; i < arm->inverse_bind_pose.length; i++)
			arm->abs_bind_pose[i] = arm->inverse_bind_pose[i].inverse();

		int bodies;
		fread(&bodies, sizeof(int), 1, f);
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

void Loader::armature_free(const AssetID id)
{
	if (id != AssetNull && armatures[id].type != AssetNone)
	{
		armatures[id].data.~Armature();
		armatures[id].type = AssetNone;
	}
}

int Loader::dynamic_mesh(int attribs, const bool dynamic)
{
	int index = AssetNull;
	for (int i = 0; i < dynamic_meshes.length; i++)
	{
		if (dynamic_meshes[i].type == AssetNone)
		{
			index = Asset::Mesh::count + i;
			break;
		}
	}

	if (index == AssetNull)
	{
		index = Asset::Mesh::count + dynamic_meshes.length;
		dynamic_meshes.add();
	}

	dynamic_meshes[index - Asset::Mesh::count].type = AssetTransient;

	RenderSync* sync = swapper->get();
	sync->write(RenderOp::AllocMesh);
	sync->write<int>(index);
	sync->write<bool>(dynamic);
	sync->write<int>(attribs);

	return index;
}

// Must be called immediately after dynamic_mesh() or dynamic_mesh_permanent()
void Loader::dynamic_mesh_attrib(RenderDataType type, int count)
{
	RenderSync* sync = swapper->get();
	sync->write(type);
	sync->write(count);
}

int Loader::dynamic_mesh_permanent(int attribs, const bool dynamic)
{
	int result = dynamic_mesh(attribs, dynamic);
	dynamic_meshes[result - Asset::Mesh::count].type = AssetPermanent;
	return result;
}

void Loader::dynamic_mesh_free(int id)
{
	if (id != AssetNull && dynamic_meshes[id].type != AssetNone)
	{
		RenderSync* sync = swapper->get();
		sync->write(RenderOp::FreeMesh);
		sync->write<int>(id);
		dynamic_meshes[id - Asset::Mesh::count].type = AssetNone;
	}
}

Animation* Loader::animation(const AssetID id)
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

		fread(&anim->duration, sizeof(float), 1, f);

		int channel_count;
		fread(&channel_count, sizeof(int), 1, f);
		anim->channels.reserve(channel_count);
		anim->channels.length = channel_count;

		for (int i = 0; i < channel_count; i++)
		{
			Channel* channel = &anim->channels[i];
			fread(&channel->bone_index, sizeof(int), 1, f);
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

Animation* Loader::animation_permanent(const AssetID id)
{
	Animation* anim = animation(id);
	if (anim)
		animations[id].type = AssetPermanent;
	return anim;
}

void Loader::animation_free(const AssetID id)
{
	if (id != AssetNull && animations[id].type != AssetNone)
	{
		animations[id].data.~Animation();
		animations[id].type = AssetNone;
	}
}

void Loader::texture(const AssetID id)
{
	if (id == AssetNull)
		return;

	if (id >= textures.length)
		textures.resize(id + 1);
	if (textures[id].type == AssetNone)
	{
		textures[id].type = AssetTransient;

		const char* path = AssetLookup::Texture::values[id];
		unsigned char* buffer;
		unsigned width, height;

		unsigned error = lodepng_decode32_file(&buffer, &width, &height, path);

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
		sync->write<unsigned>(&width);
		sync->write<unsigned>(&height);
		sync->write<unsigned char>(buffer, 4 * width * height);
		free(buffer);
	}
}

void Loader::texture_permanent(const AssetID id)
{
	texture(id);
	if (id != AssetNull)
		textures[id].type = AssetPermanent;
}

void Loader::texture_free(const AssetID id)
{
	if (id != AssetNull && textures[id].type != AssetNone)
	{
		RenderSync* sync = swapper->get();
		sync->write(RenderOp::FreeTexture);
		sync->write<AssetID>(id);
		textures[id].type = AssetNone;
	}
}

int Loader::dynamic_texture(const int width, const int height, const RenderDynamicTextureType type, const RenderTextureFilter filter)
{
	int index = AssetNull;
	for (int i = 0; i < dynamic_textures.length; i++)
	{
		if (dynamic_textures[i].type == AssetNone)
		{
			index = Asset::Texture::count + i;
			break;
		}
	}

	if (index == AssetNull)
	{
		index = Asset::Texture::count + dynamic_textures.length;
		dynamic_textures.add();
	}

	dynamic_textures[index - Asset::Texture::count].type = AssetTransient;

	RenderSync* sync = swapper->get();
	sync->write(RenderOp::AllocTexture);
	sync->write<AssetID>(index);
	sync->write(RenderOp::DynamicTexture);
	sync->write<AssetID>(index);
	sync->write<unsigned>(width);
	sync->write<unsigned>(height);
	sync->write<RenderDynamicTextureType>(type);
	sync->write<RenderTextureFilter>(filter);

	return index;
}

int Loader::dynamic_texture_permanent(const int width, const int height, const RenderDynamicTextureType type, const RenderTextureFilter filter)
{
	int id = dynamic_texture(width, height, type, filter);
	if (id != AssetNull)
		dynamic_textures[id - Asset::Texture::count].type = AssetPermanent;
	return id;
}

void Loader::dynamic_texture_free(const int id)
{
	if (id != AssetNull && dynamic_textures[id - Asset::Texture::count].type != AssetNone)
	{
		RenderSync* sync = swapper->get();
		sync->write(RenderOp::FreeTexture);
		sync->write<AssetID>(id);
		dynamic_textures[id - Asset::Texture::count].type = AssetNone;
	}
}

int Loader::framebuffer(const int attachments)
{
	int index = AssetNull;
	for (int i = 0; i < framebuffers.length; i++)
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
	sync->write<int>(index);
	sync->write<int>(attachments);

	return index;
}

// Must be called immediately after framebuffer() or framebuffer_permanent()
void Loader::framebuffer_attach(const RenderFramebufferAttachment attachment_type, const int dynamic_texture)
{
	RenderSync* sync = swapper->get();
	sync->write<RenderFramebufferAttachment>(attachment_type);
	sync->write<int>(dynamic_texture);
}

int Loader::framebuffer_permanent(const int attachments)
{
	int id = framebuffer(attachments);
	if (id != AssetNull)
		framebuffers[id].type = AssetPermanent;
	return id;
}

void Loader::framebuffer_free(const int id)
{
	if (id != AssetNull && framebuffers[id].type != AssetNone)
	{
		RenderSync* sync = swapper->get();
		sync->write(RenderOp::FreeFramebuffer);
		sync->write<int>(id);
		framebuffers[id].type = AssetNone;
	}
}

void Loader::shader(const AssetID id)
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

		const int chunk_size = 4096;
		int i = 1;
		while (true)
		{
			code.reserve(i * chunk_size + 1); // extra char since this will be a null-terminated string
			int read = fread(&code.data[(i - 1) * chunk_size], sizeof(char), chunk_size, f);
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
		sync->write<int>(&code.length);
		sync->write(code.data, code.length);
	}
}

void Loader::shader_permanent(const AssetID id)
{
	shader(id);
	if (id != AssetNull)
		shaders[id].type = AssetPermanent;
}

void Loader::shader_free(const AssetID id)
{
	if (id != AssetNull && shaders[id].type != AssetNone)
	{
		RenderSync* sync = swapper->get();
		sync->write(RenderOp::FreeShader);
		sync->write<AssetID>(id);
		shaders[id].type = AssetNone;
	}
}

Font* Loader::font(const AssetID id)
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

		int j;

		fread(&j, sizeof(int), 1, f);
		font->vertices.resize(j);
		fread(font->vertices.data, sizeof(Vec3), font->vertices.length, f);

		fread(&j, sizeof(int), 1, f);
		font->indices.resize(j);
		fread(font->indices.data, sizeof(int), font->indices.length, f);

		fread(&j, sizeof(int), 1, f);
		Array<Font::Character> characters;
		characters.resize(j);
		fread(characters.data, sizeof(Font::Character), characters.length, f);
		for (int i = 0; i < characters.length; i++)
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

Font* Loader::font_permanent(const AssetID id)
{
	Font* f = font(id);
	if (f)
		fonts[id].type = AssetPermanent;
	return f;
}

void Loader::font_free(const AssetID id)
{
	if (id != AssetNull && fonts[id].type != AssetNone)
	{
		fonts[id].data.~Font();
		fonts[id].type = AssetNone;
	}
}

cJSON* Loader::level(const AssetID id)
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
		mesh->verts = (unsigned short*)malloc(sizeof(unsigned short) * 3 * mesh->nverts);
		mesh->polys = (unsigned short*)malloc(sizeof(unsigned short) * 2 * mesh->nvp * mesh->npolys);
		mesh->regs = (unsigned short*)malloc(sizeof(unsigned short) * mesh->npolys);
		mesh->flags = (unsigned short*)malloc(sizeof(unsigned short) * mesh->npolys);
		mesh->areas = (unsigned char*)malloc(sizeof(unsigned char) * mesh->npolys);
		fread(mesh->verts, sizeof(unsigned short) * 3, mesh->nverts, f);
		fread(mesh->polys, sizeof(unsigned short) * 2 * mesh->nvp, mesh->npolys, f);
		fread(mesh->regs, sizeof(unsigned short), mesh->npolys, f);
		fread(mesh->flags, sizeof(unsigned short), mesh->npolys, f);
		fread(mesh->areas, sizeof(unsigned char), mesh->npolys, f);
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

dtNavMesh* Loader::nav_mesh(const AssetID id)
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
			mesh_detail.meshes = (unsigned int*)malloc(sizeof(unsigned int) * 4 * mesh_detail.nmeshes);
			mesh_detail.verts = (float*)malloc(sizeof(float) * 3 * mesh_detail.nverts);
			mesh_detail.tris = (unsigned char*)malloc(sizeof(unsigned char) * 4 * mesh_detail.ntris);
			fread(mesh_detail.meshes, sizeof(unsigned int) * 4, mesh_detail.nmeshes, f);
			fread(mesh_detail.verts, sizeof(float) * 3, mesh_detail.nverts, f);
			fread(mesh_detail.tris, sizeof(unsigned char) * 4, mesh_detail.ntris, f);

			float agent_height;
			float agent_radius;
			float agent_max_climb;

			fread(&agent_height, sizeof(float), 1, f);
			fread(&agent_radius, sizeof(float), 1, f);
			fread(&agent_max_climb, sizeof(float), 1, f);

			fclose(f);

			unsigned char* navData = 0;
			int navDataSize = 0;

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
				bool status = dtCreateNavMeshData(&params, &navData, &navDataSize);
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

bool Loader::soundbank(const AssetID id)
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

bool Loader::soundbank_permanent(const AssetID id)
{
	bool success = soundbank(id);
	if (success)
		soundbanks[id].type = AssetPermanent;
	return success;
}

void Loader::soundbank_free(const AssetID id)
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
			dynamic_mesh_free(Asset::Mesh::count + i);
	}

	for (int i = 0; i < dynamic_textures.length; i++)
	{
		if (dynamic_textures[i].type == AssetTransient)
			dynamic_texture_free(Asset::Texture::count + i);
	}

	for (int i = 0; i < framebuffers.length; i++)
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
	int i = 0;
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
	binding.key = (KeyCode)Json::get_int(json, "key", (int)default_value.key);
	binding.btn = (Gamepad::Btn)Json::get_int(json, "btn", (int)default_value.btn);
	return binding;
}

cJSON* input_binding_json(const InputBinding& binding)
{
	cJSON* json = cJSON_CreateObject();
	if (binding.key != KeyCode::None)
		cJSON_AddNumberToObject(json, "key", (int)binding.key);
	if (binding.btn != Gamepad::Btn::None)
		cJSON_AddNumberToObject(json, "btn", (int)binding.btn);
	return json;
}

Settings& Loader::settings()
{
	if (!settings_data.valid)
	{
		cJSON* json = Json::load("config.txt");
		settings_data.width = Json::get_int(json, "width", 1920);
		settings_data.height = Json::get_int(json, "height", 1080);
		settings_data.fullscreen = (bool)Json::get_int(json, "fullscreen", 0);
		settings_data.valid = true;

		cJSON* bindings = cJSON_GetObjectItem(json, "bindings");
		settings_data.bindings.backward = input_binding(bindings, "backward", { KeyCode::S, Gamepad::Btn::None });
		settings_data.bindings.forward = input_binding(bindings, "forward", { KeyCode::W, Gamepad::Btn::None });
		settings_data.bindings.left = input_binding(bindings, "left", { KeyCode::A, Gamepad::Btn::None });
		settings_data.bindings.right = input_binding(bindings, "right", { KeyCode::D, Gamepad::Btn::None });
		settings_data.bindings.up = input_binding(bindings, "up", { KeyCode::Space, Gamepad::Btn::RightShoulder });
		settings_data.bindings.down = input_binding(bindings, "down", { KeyCode::LCtrl, Gamepad::Btn::LeftShoulder });
		settings_data.bindings.primary = input_binding(bindings, "primary", { KeyCode::MouseLeft, Gamepad::Btn::RightTrigger });
		settings_data.bindings.secondary = input_binding(bindings, "secondary", { KeyCode::MouseRight, Gamepad::Btn::LeftTrigger });
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
		cJSON_AddItemToObject(bindings, "up", input_binding_json(settings_data.bindings.up));
		cJSON_AddItemToObject(bindings, "down", input_binding_json(settings_data.bindings.down));
		cJSON_AddItemToObject(bindings, "primary", input_binding_json(settings_data.bindings.primary));
		cJSON_AddItemToObject(bindings, "secondary", input_binding_json(settings_data.bindings.secondary));

		Json::save(json, "config.txt");
		Json::json_free(json);
	}
}

}
