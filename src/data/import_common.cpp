#include "import_common.h"

#include "cJSON.h"
#include <stdio.h>

#include <GL/glew.h>

namespace VI
{

char* technique_prefixes[] =
{
	"", // Default
	"#define SHADOW\n", // Shadow
};

bool compile_shader(const char* prefix, const char* code, int code_length, unsigned int* program_id, const char* path)
{
	bool success = true;

	GLuint vertex_id = glCreateShader(GL_VERTEX_SHADER);
	GLuint frag_id = glCreateShader(GL_FRAGMENT_SHADER);

	// Compile Vertex Shader
	GLint prefix_length = strlen(prefix);
	char const* vertex_code[] = { "#version 330 core\n#define VERTEX\n", prefix, code };
	const GLint vertex_code_length[] = { 33, prefix_length, (GLint)code_length };
	glShaderSource(vertex_id, 3, vertex_code, vertex_code_length);
	glCompileShader(vertex_id);

	// Check Vertex Shader
	GLint result;
	glGetShaderiv(vertex_id, GL_COMPILE_STATUS, &result);
	int msg_length;
	glGetShaderiv(vertex_id, GL_INFO_LOG_LENGTH, &msg_length);
	if (msg_length > 1)
	{
		Array<char> msg(msg_length);
		glGetShaderInfoLog(vertex_id, msg_length, NULL, msg.data);
		fprintf(stderr, "Error: vertex shader '%s': %s\n", path, msg.data);
		success = false;
	}

	// Compile Fragment Shader
	const char* frag_code[] = { "#version 330 core\n", prefix, code };
	const GLint frag_code_length[] = { 18, prefix_length, (GLint)code_length };
	glShaderSource(frag_id, 3, frag_code, frag_code_length);
	glCompileShader(frag_id);

	// Check Fragment Shader
	glGetShaderiv(frag_id, GL_COMPILE_STATUS, &result);
	glGetShaderiv(frag_id, GL_INFO_LOG_LENGTH, &msg_length);
	if (msg_length > 1)
	{
		Array<char> msg(msg_length + 1);
		glGetShaderInfoLog(frag_id, msg_length, NULL, msg.data);
		fprintf(stderr, "Error: fragment shader '%s': %s\n", path, msg.data);
		success = false;
	}

	// Link the program
	*program_id = glCreateProgram();
	glAttachShader(*program_id, vertex_id);
	glAttachShader(*program_id, frag_id);
	glLinkProgram(*program_id);

	// Check the program
	glGetProgramiv(*program_id, GL_LINK_STATUS, &result);
	glGetProgramiv(*program_id, GL_INFO_LOG_LENGTH, &msg_length);
	if (msg_length > 1)
	{
		Array<char> msg(msg_length);
		glGetProgramInfoLog(*program_id, msg_length, NULL, msg.data);
		fprintf(stderr, "Error: shader program '%s': %s\n", path, msg.data);
		success = false;
	}

	glDeleteShader(vertex_id);
	glDeleteShader(frag_id);

	return success;
}

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

	float get_float(cJSON* parent, const char* key, const float default_value)
	{
		cJSON* json = cJSON_GetObjectItem(parent, key);
		if (json)
			return json->valuedouble;
		else
			return default_value;
	}

	int get_int(cJSON* parent, const char* key, const int default_value)
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
