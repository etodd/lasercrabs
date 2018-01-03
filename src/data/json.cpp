#include "json.h"
#include "cjson/cJSON.h"
#include "vi_assert.h"
#include <stdio.h>

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
	return Vec3(r32(x->valuedouble), r32(y->valuedouble), r32(z->valuedouble));
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
	return Vec4(r32(x->valuedouble), r32(y->valuedouble), r32(z->valuedouble), r32(w->valuedouble));
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
	return Quat(r32(w->valuedouble), r32(x->valuedouble), r32(y->valuedouble), r32(z->valuedouble));
}

r32 get_r32(cJSON* parent, const char* key, r32 default_value)
{
	if (!parent)
		return default_value;

	cJSON* json = cJSON_GetObjectItem(parent, key);
	if (json)
		return r32(json->valuedouble);
	else
		return default_value;
}

s32 get_s32(cJSON* parent, const char* key, s32 default_value)
{
	if (!parent)
		return default_value;

	cJSON* json = cJSON_GetObjectItem(parent, key);
	if (json)
		return json->valueint;
	else
		return default_value;
}

s64 get_s64(cJSON* parent, const char* key, s64 default_value)
{
	if (!parent)
		return default_value;

	cJSON* json = cJSON_GetObjectItem(parent, key);
	if (json)
		return json->valuedouble;
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

s32 get_enum(cJSON* parent, const char* key, const char** search, s32 default_value)
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


}
