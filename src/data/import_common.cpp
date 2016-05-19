#include "import_common.h"

#include "cjson/cJSON.h"
#include <stdio.h>

#include "recast/Recast/Include/Recast.h"
#include "recast/DetourTileCache/Include/DetourTileCache.h"
#include "recast/DetourTileCache/Include/DetourTileCacheBuilder.h"
#include <glew/include/GL/glew.h>
#include "fastlz/fastlz.h"
#include "utf8/utf8.h"

namespace VI
{

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

		char* data = (char*)malloc(len + 1);
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
				if (utf8cmp(string, search[i]) == 0)
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

const Font::Character& Font::get(const void* character) const
{
	// TODO: unicode
	char c = *((char*)character);
	return characters[c];
}

int FastLZCompressor::maxCompressedSize(const int bufferSize)
{
	return (int)(bufferSize* 1.05f);
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


}
