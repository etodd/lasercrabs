#include "import_common.h"

#include "cjson/cJSON.h"
#include <stdio.h>

#include "recast/Recast/Include/Recast.h"
#include "recast/DetourTileCache/Include/DetourTileCache.h"
#include "recast/DetourTileCache/Include/DetourTileCacheBuilder.h"
#include "fastlz/fastlz.h"

namespace VI
{

const char* TechniquePrefixes::all[(s32)RenderTechnique::count] =
{
	"", // Default
	"#define SHADOW\n", // Shadow
};

namespace Json
{
	cJSON* load(const char* path)
	{
		FILE* f = fopen(path, "rb");
		if (!f)
		{
			fprintf(stderr, "Can't open file '%s'\n", path);
			return 0;
		}
		fseek(f, 0, SEEK_END);
		long len = ftell(f);
		fseek(f, 0, SEEK_SET);

		char* data = (char*)calloc(sizeof(char), len + 1);
		fread(data, 1, len, f);
		data[len] = '\0';
		fclose(f);

		cJSON* output = cJSON_Parse(data);
		if (!output)
			fprintf(stderr, "Can't parse json file '%s': %s\n", path, cJSON_GetErrorPtr());
		free(data);
		return output;
	}

	void save(cJSON* json, const char* path)
	{
		FILE* f = fopen(path, "wb");
		if (!f)
		{
			fprintf(stderr, "Can't open file '%s'\n", path);
			vi_assert(false);
		}

		char* data = cJSON_Print(json);
		fprintf(f, "%s", data);
		fclose(f);

		free(data);
	}

	void json_free(cJSON* json)
	{
		if (json)
			cJSON_Delete(json);
	}

	Vec3 get_vec3(cJSON* parent, const char* key, const Vec3& default_value)
	{
		if (!parent)
			return default_value;

		cJSON* json = cJSON_GetObjectItem(parent, key);
		if (!json)
			return default_value;
		cJSON* x = json->child;
		cJSON* y = x->next;
		cJSON* z = y->next;
		return Vec3(x->valuedouble, y->valuedouble, z->valuedouble);
	}

	Vec4 get_vec4(cJSON* parent, const char* key, const Vec4& default_value)
	{
		if (!parent)
			return default_value;

		cJSON* json = cJSON_GetObjectItem(parent, key);
		if (!json)
			return default_value;
		cJSON* x = json->child;
		cJSON* y = x->next;
		cJSON* z = y->next;
		cJSON* w = z->next;
		return Vec4(x->valuedouble, y->valuedouble, z->valuedouble, w->valuedouble);
	}

	Quat get_quat(cJSON* parent, const char* key, const Quat& default_value)
	{
		if (!parent)
			return default_value;

		cJSON* json = cJSON_GetObjectItem(parent, key);
		if (!json)
			return default_value;
		cJSON* w = json->child;
		cJSON* x = w->next;
		cJSON* y = x->next;
		cJSON* z = y->next;
		return Quat(w->valuedouble, x->valuedouble, y->valuedouble, z->valuedouble);
	}

	r32 get_r32(cJSON* parent, const char* key, const r32 default_value)
	{
		if (!parent)
			return default_value;

		cJSON* json = cJSON_GetObjectItem(parent, key);
		if (json)
			return json->valuedouble;
		else
			return default_value;
	}

	s32 get_s32(cJSON* parent, const char* key, const s32 default_value)
	{
		if (!parent)
			return default_value;

		cJSON* json = cJSON_GetObjectItem(parent, key);
		if (json)
			return json->valueint;
		else
			return default_value;
	}

	const char* get_string(cJSON* parent, const char* key, const char* default_value)
	{
		if (!parent)
			return default_value;

		cJSON* json = cJSON_GetObjectItem(parent, key);
		if (json)
			return json->valuestring;
		else
			return default_value;
	}

	s32 get_enum(cJSON* parent, const char* key, const char** search, const s32 default_value)
	{
		if (!parent)
			return default_value;

		cJSON* json = cJSON_GetObjectItem(parent, key);
		if (json)
		{
			const char* string = json->valuestring;
			s32 i = 0;
			while (search[i])
			{
				if (strcmp(string, search[i]) == 0)
					return i;
				i++;
			}
			return default_value;
		}
		else
			return default_value;
	}
}

void TileCacheData::free()
{
	for (s32 i = 0; i < cells.length; i++)
	{
		TileCacheCell& cell = cells[i];
		for (s32 j = 0; j < cell.layers.length; j++)
			dtFree(cell.layers[j].data);
	}
}

TileCacheData::~TileCacheData()
{
	for (s32 i = 0; i < cells.length; i++)
	{
		TileCacheCell& cell = cells[i];
		cell.~TileCacheCell();
	}
}

s32 Font::codepoint(const char* s)
{
	if (0xf0 == (0xf8 & s[0]))
		return ((0x07 & s[0]) << 18)
			| ((0x3f & s[1]) << 12)
			| ((0x3f & s[2]) << 6)
			| (0x3f & s[3]); // 4 byte utf8 codepoint
	else if (0xe0 == (0xf0 & s[0]))
		return ((0x0f & s[0]) << 12)
			| ((0x3f & s[1]) << 6)
			| (0x3f & s[2]); // 3 byte utf8 codepoint
	else if (0xc0 == (0xe0 & s[0]))
		return ((0x1f & s[0]) << 6) | (0x3f & s[1]); // 2 byte utf8 codepoint
	else
		return 0xff & s[0]; // 1 byte utf8 codepoint otherwise
}

const char* Font::codepoint_next(const char* s)
{
	if (0xf0 == (0xf8 & s[0]))
		s += 4;
	else if (0xe0 == (0xf0 & s[0]))
		s += 3;
	else if (0xc0 == (0xe0 & s[0]))
		s += 2;
	else
		s += 1;

	return s;
}

s32 Font::codepoint_count(const char* s)
{
	s32 count = 0;
	while (*s)
	{
		count++;
		s = codepoint_next(s);
	}
	return count;
}

Font::Character::Character()
	: min(),
	max(),
	index_start(),
	index_count(),
	vertex_start(),
	vertex_count(),
	codepoint()
{
}

const Font::Character& Font::get(const char* character) const
{
	auto i = characters.find(codepoint(character));
	if (i == characters.end())
		return characters.find(codepoint(" "))->second;
	else
		return i->second;
}

s32 FastLZCompressor::maxCompressedSize(const int bufferSize)
{
	return s32(bufferSize * 1.05f);
}

dtStatus FastLZCompressor::compress(const unsigned char* buffer, const int bufferSize,
	unsigned char* compressed, const int maxCompressedSize, int* compressedSize)
{
	*compressedSize = fastlz_compress((const void *const)buffer, bufferSize, compressed);
	return DT_SUCCESS;
}

dtStatus FastLZCompressor::decompress(const unsigned char* compressed, const int compressedSize,
	unsigned char* buffer, const int maxBufferSize, int* bufferSize)
{
	*bufferSize = fastlz_decompress(compressed, compressedSize, buffer, maxBufferSize);
	return *bufferSize < 0 ? DT_FAILURE : DT_SUCCESS;
}

void AwkNavMeshAdjacency::remove(s32 i)
{
	if (i != neighbors.length - 1)
		flag(i, flag(neighbors.length - 1));
	flag(neighbors.length - 1, false);
	neighbors.remove(i);
}

b8 AwkNavMeshAdjacency::flag(s32 i) const
{
	return flags & ((u64)1 << i);
}

void AwkNavMeshAdjacency::flag(s32 i, b8 value)
{
	if (value)
		flags |= ((u64)1 << i);
	else
		flags &= ~((u64)1 << i);
}

Armature::Armature()
	: hierarchy(), bind_pose(), inverse_bind_pose(), abs_bind_pose(), bodies()
{

}

void Mesh::reset()
{
	indices.length = 0;
	edge_indices.length = 0;
	vertices.length = 0;
	normals.length = 0;
	armature.hierarchy.length = 0;
	armature.bind_pose.length = 0;
	armature.inverse_bind_pose.length = 0;
	instanced = false;
}

void Mesh::read(Mesh* mesh, const char* path, Array<Attrib>* extra_attribs)
{
	new (mesh) Mesh();

	FILE* f = fopen(path, "rb");
	if (!f)
	{
		fprintf(stderr, "Can't open msh file '%s'\n", path);
		return;
	}

	// Read color
	fread(&mesh->color, sizeof(Vec4), 1, f);

	// Read bounding box
	fread(&mesh->bounds_min, sizeof(Vec3), 1, f);
	fread(&mesh->bounds_max, sizeof(Vec3), 1, f);
	fread(&mesh->bounds_radius, sizeof(r32), 1, f);

	// read indices
	s32 index_count;
	fread(&index_count, sizeof(s32), 1, f);

	// fill face indices
	mesh->indices.resize(index_count);
	fread(mesh->indices.data, sizeof(s32), index_count, f);

	// read edge indices
	s32 edge_index_count;
	fread(&edge_index_count, sizeof(s32), 1, f);

	// fill face indices
	mesh->edge_indices.resize(edge_index_count);
	fread(mesh->edge_indices.data, sizeof(s32), edge_index_count, f);

	s32 vertex_count;
	fread(&vertex_count, sizeof(s32), 1, f);

	// fill vertices positions
	mesh->vertices.resize(vertex_count);
	fread(mesh->vertices.data, sizeof(Vec3), vertex_count, f);

	// fill normals
	mesh->normals.resize(vertex_count);
	fread(mesh->normals.data, sizeof(Vec3), vertex_count, f);

	if (extra_attribs)
	{
		s32 extra_attrib_count;
		fread(&extra_attrib_count, sizeof(s32), 1, f);
		extra_attribs->resize(extra_attrib_count);
		for (s32 i = 0; i < extra_attribs->length; i++)
		{
			Attrib* a = &(*extra_attribs)[i];
			fread(&a->type, sizeof(RenderDataType), 1, f);
			fread(&a->count, sizeof(s32), 1, f);
			a->data.resize(mesh->vertices.length * a->count * render_data_type_size(a->type));
			fread(a->data.data, sizeof(char), a->data.length, f);
		}
	}

	fclose(f);
}


}