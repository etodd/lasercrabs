#include "import_common.h"

#include "cJSON.h"
#include <stdio.h>

namespace VI
{

namespace Json
{
	cJSON* load(const char* path)
	{
		FILE* f;
		f = fopen(path, "rb");
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

	void json_free(cJSON* json)
	{
		cJSON_Delete(json);
	}

	Vec3 get_vec3(cJSON* parent, const char* key, const Vec3& default_value)
	{
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
		cJSON* json = cJSON_GetObjectItem(parent, key);
		if (!json)
			return default_value;
		cJSON* w = json->child;
		cJSON* x = w->next;
		cJSON* y = x->next;
		cJSON* z = y->next;
		return Quat(w->valuedouble, x->valuedouble, y->valuedouble, z->valuedouble);
	}

	const int get_int(cJSON* parent, const char* key, const int default_value)
	{
		cJSON* json = cJSON_GetObjectItem(parent, key);
		if (json)
			return json->valueint;
		else
			return default_value;
	}

	const char* get_string(cJSON* parent, const char* key, const char* default_value)
	{
		cJSON* json = cJSON_GetObjectItem(parent, key);
		if (json)
			return json->valuestring;
		else
			return default_value;
	}
}

}
