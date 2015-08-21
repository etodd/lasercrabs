#include "load.h"
#include <stdio.h>
#include <GL/glew.h>
#include "lodepng.h"
#include "vi_assert.h"
#include "asset/lookup.h"
#include "asset/mesh.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"

namespace VI
{

RenderSync::Swapper* Loader::swapper;
// First entry in each array is empty
Array<Loader::Entry<Mesh> > Loader::meshes;
Array<Loader::Entry<Animation> > Loader::animations;
Array<Loader::Entry<Armature> > Loader::armatures;
Array<Loader::Entry<void*> > Loader::textures;
Array<Loader::Entry<void*> > Loader::shaders;
Array<Loader::Entry<Font> > Loader::fonts;
Array<Loader::Entry<void*> > Loader::dynamic_meshes;
dtNavMesh* Loader::current_nav_mesh;
AssetID Loader::current_nav_mesh_id = AssetNull;

struct Attrib
{
	RenderDataType type;
	int count;
	Array<char> data;
};

// Recast nav mesh structs

/// Represents a polygon mesh suitable for use in building a navigation mesh. 
struct rcPolyMesh
{
	unsigned short* verts;	///< The mesh vertices. [Form: (x, y, z) * #nverts]
	unsigned short* polys;	///< Polygon and neighbor data. [Length: #maxpolys * 2 * #nvp]
	unsigned short* regs;	///< The region id assigned to each polygon. [Length: #maxpolys]
	unsigned short* flags;	///< The user defined flags for each polygon. [Length: #maxpolys]
	unsigned char* areas;	///< The area id assigned to each polygon. [Length: #maxpolys]
	int nverts;				///< The number of vertices.
	int npolys;				///< The number of polygons.
	int maxpolys;			///< The number of allocated polygons.
	int nvp;				///< The maximum number of vertices per polygon.
	float bmin[3];			///< The minimum bounds in world space. [(x, y, z)]
	float bmax[3];			///< The maximum bounds in world space. [(x, y, z)]
	float cs;				///< The size of each cell. (On the xz-plane.)
	float ch;				///< The height of each cell. (The minimum increment along the y-axis.)
	int borderSize;			///< The AABB border size used to generate the source data from which the mesh was derived.
};

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

void Loader::init(RenderSync::Swapper* s)
{
	swapper = s;
	meshes.resize(Asset::Mesh::count);
	dynamic_meshes = Array<Loader::Entry<void*> >();
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
			fprintf(stderr, "Can't open mdl file '%s'\n", path);
			return 0;
		}

		new (mesh)Mesh();

		// Read color
		fread(&mesh->color, sizeof(Vec4), 1, f);

		// Read bounding box
		fread(&mesh->bounds_min, sizeof(Vec3), 1, f);
		fread(&mesh->bounds_max, sizeof(Vec3), 1, f);

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

		// Fill vertices normals
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

		int bone_count;
		fread(&bone_count, sizeof(int), 1, f);
		mesh->inverse_bind_pose.resize(bone_count);
		fread(mesh->inverse_bind_pose.data, sizeof(Mat4), bone_count, f);

		fclose(f);

		// GL
		SyncData* sync = swapper->get();
		sync->write(RenderOp_AllocMesh);
		sync->write<int>(id);

		sync->write<int>(2 + extra_attribs.length); // Attribute count

		sync->write(RenderDataType_Vec3); // Position
		sync->write<int>(1); // Number of data elements per vertex

		sync->write(RenderDataType_Vec3); // Normal
		sync->write<int>(1); // Number of data elements per vertex

		for (int i = 0; i < extra_attribs.length; i++)
		{
			Attrib* a = &extra_attribs[i];
			sync->write<RenderDataType>(a->type);
			sync->write<int>(a->count);
		}

		sync->write(RenderOp_UpdateAttribBuffers);
		sync->write<int>(id);

		sync->write<int>(mesh->vertices.length);
		sync->write(mesh->vertices.data, mesh->vertices.length);
		sync->write(mesh->normals.data, mesh->vertices.length);

		for (int i = 0; i < extra_attribs.length; i++)
		{
			Attrib* a = &extra_attribs[i];
			sync->write(a->data.data, a->data.length);
		}

		sync->write(RenderOp_UpdateIndexBuffer);
		sync->write<int>(id);
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

void Loader::mesh_free(AssetID id)
{
	if (id != AssetNull && meshes[id].type != AssetNone)
	{
		meshes[id].data.~Mesh();
		SyncData* sync = swapper->get();
		sync->write(RenderOp_FreeMesh);
		sync->write<int>(id);
		meshes[id].type = AssetNone;
	}
}

AssetID Loader::mesh_ref_to_id(AssetID model, AssetRef mesh)
{
	return Asset::mesh_refs[model][mesh];
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

		int bones;
		fread(&bones, sizeof(int), 1, f);
		arm->hierarchy.resize(bones);
		fread(arm->hierarchy.data, sizeof(int), bones, f);
		arm->bind_pose.resize(bones);
		fread(arm->bind_pose.data, sizeof(Bone), bones, f);

		fclose(f);

		armatures[id].type = AssetTransient;
	}
	return &armatures[id].data;
}

Armature* Loader::armature_permanent(AssetID id)
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

int Loader::dynamic_mesh(int attribs)
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

	SyncData* sync = swapper->get();
	sync->write(RenderOp_AllocMesh);
	sync->write<int>(index);
	sync->write<int>(attribs);

	return index;
}

void Loader::dynamic_mesh_attrib(RenderDataType type, int count)
{
	SyncData* sync = swapper->get();
	sync->write(type);
	sync->write(count);
}

int Loader::dynamic_mesh_permanent(int attribs)
{
	int result = dynamic_mesh(attribs);
	dynamic_meshes[result - Asset::Mesh::count].type = AssetPermanent;
	return result;
}

void Loader::dynamic_mesh_free(int id)
{
	if (id != AssetNull && dynamic_meshes[id].type != AssetNone)
	{
		SyncData* sync = swapper->get();
		sync->write(RenderOp_FreeMesh);
		sync->write<int>(id);
		dynamic_meshes[id - Asset::Mesh::count].type = AssetNone;
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

void Loader::texture(AssetID id)
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
	if (id != AssetNull)
		textures[id].type = AssetPermanent;
}

void Loader::texture_free(AssetID id)
{
	if (id != AssetNull && textures[id].type != AssetNone)
	{
		SyncData* sync = swapper->get();
		sync->write(RenderOp_FreeTexture);
		sync->write<AssetID>(&id);
		textures[id].type = AssetNone;
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

		SyncData* sync = swapper->get();
		sync->write(RenderOp_LoadShader);
		sync->write<AssetID>(&id);
		sync->write<int>(&code.length);
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
		SyncData* sync = swapper->get();
		sync->write(RenderOp_FreeShader);
		sync->write(&id);
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
		fread(&mesh, sizeof(rcPolyMesh), 1, f);
		mesh.verts = (unsigned short*)malloc(sizeof(unsigned short) * 3 * mesh.nverts);
		mesh.polys = (unsigned short*)malloc(sizeof(unsigned short) * 2 * mesh.nvp * mesh.maxpolys);
		mesh.regs = (unsigned short*)malloc(sizeof(unsigned short) * mesh.maxpolys);
		mesh.flags = (unsigned short*)malloc(sizeof(unsigned short) * mesh.maxpolys);
		mesh.areas = (unsigned char*)malloc(sizeof(unsigned char) * mesh.maxpolys);
		fread(mesh.verts, sizeof(unsigned short) * 3, mesh.nverts, f);
		fread(mesh.polys, sizeof(unsigned short) * 2 * mesh.nvp, mesh.maxpolys, f);
		fread(mesh.regs, sizeof(unsigned short), mesh.maxpolys, f);
		fread(mesh.flags, sizeof(unsigned short), mesh.maxpolys, f);
		fread(mesh.areas, sizeof(unsigned char), mesh.maxpolys, f);

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
		
		if (!dtCreateNavMeshData(&params, &navData, &navDataSize))
			return 0;
		
		current_nav_mesh = dtAllocNavMesh();
		if (!current_nav_mesh)
		{
			dtFree(navData);
			return 0;
		}
		
		dtStatus status = current_nav_mesh->init(navData, navDataSize, DT_TILE_FREE_DATA);
		if (dtStatusFailed(status))
		{
			dtFree(navData);
			return 0;
		}
		
		free(mesh.verts);
		free(mesh.polys);
		free(mesh.regs);
		free(mesh.flags);
		free(mesh.areas);
		free(mesh_detail.meshes);
		free(mesh_detail.verts);
		free(mesh_detail.tris);

		current_nav_mesh_id = id;
	}

	return current_nav_mesh;
}

void Loader::transients_free()
{
	if (current_nav_mesh)
	{
		dtFreeNavMesh(current_nav_mesh);
		current_nav_mesh = 0;
		current_nav_mesh_id = AssetNull;
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

	for (int i = 0; i < dynamic_meshes.length; i++)
	{
		if (dynamic_meshes[i].type == AssetTransient)
			dynamic_mesh_free(Asset::Mesh::count + i);
	}
}

}
