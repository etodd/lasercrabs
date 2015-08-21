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
		free(data);
		return output;
	}

	void json_free(cJSON* json)
	{
		cJSON_Delete(json);
	}

	Vec3 get_vec3(cJSON* json)
	{
		cJSON* x = json->child;
		cJSON* y = x->next;
		cJSON* z = y->next;
		return Vec3(x->valuedouble, y->valuedouble, z->valuedouble);
	}

	Quat get_quat(cJSON* json)
	{
		cJSON* w = json->child;
		cJSON* x = w->next;
		cJSON* y = x->next;
		cJSON* z = y->next;
		return Quat(w->valuedouble, x->valuedouble, y->valuedouble, z->valuedouble);
	}
}

}
