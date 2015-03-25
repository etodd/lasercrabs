#include "load.h"
#include <stdio.h>
#include <GL/glew.h>
#include "lodepng.h"
#include "vi_assert.h"

Loader::Loader()
{
	memset(this, 0, sizeof(Loader));
}

Mesh* Loader::mesh(Asset::ID id)
{
	if (!meshes[id].refs)
	{
		const char* path = Asset::Model::filenames[id];
		FILE* f = fopen(path, "rb");
		if (!f)
		{
			fprintf(stderr, "Can't open mdl file '%s'\n", path);
			return 0;
		}

		Mesh* mesh = &meshes[id].data;
		new (mesh) Mesh();

		// Read indices
		int count;
		fread(&count, sizeof(int), 1, f);

		// Fill face indices
		mesh->indices.reserve(count);
		mesh->indices.length = count;
		fread(mesh->indices.data, sizeof(int), count, f);

		fread(&count, sizeof(int), 1, f);

		// Fill vertices positions
		mesh->vertices.reserve(count);
		mesh->vertices.length = count;
		fread(mesh->vertices.data, sizeof(Vec3), count, f);

		// Fill vertices texture coordinates
		mesh->uvs.reserve(count);
		mesh->uvs.length = count;
		fread(mesh->uvs.data, sizeof(Vec2), count, f);

		// Fill vertices normals
		mesh->normals.reserve(count);
		mesh->normals.length = count;
		fread(mesh->normals.data, sizeof(Vec3), count, f);

		fclose(f);

		// Physics
		new (&mesh->physics) btTriangleIndexVertexArray(mesh->indices.length / 3, mesh->indices.data, 3 * sizeof(int), mesh->vertices.length, (btScalar*)mesh->vertices.data, sizeof(Vec3));

		// GL
		mesh->gl.add_attrib<Vec3>(&mesh->vertices, GL_FLOAT);
		mesh->gl.add_attrib<Vec2>(&mesh->uvs, GL_FLOAT);
		mesh->gl.add_attrib<Vec3>(&mesh->normals, GL_FLOAT);
		mesh->gl.set_indices(&mesh->indices);
	}
	meshes[id].refs++;
	return &meshes[id].data;
}

void Loader::unload_mesh(Asset::ID id)
{
	vi_assert(meshes[id].refs > 0);
	if (--meshes[id].refs == 0)
		meshes[id].data.~Mesh();
}

GLuint Loader::texture(Asset::ID id)
{
	if (!textures[id].refs)
	{
		const char* path = Asset::Texture::filenames[id];
		unsigned char* buffer;
		unsigned width, height;

		unsigned error = lodepng_decode32_file(&buffer, &width, &height, path);

		if (error)
		{
			fprintf(stderr, "Error loading texture '%s': %s\n", path, lodepng_error_text(error));
			return 0;
		}

		// Make power of two version of the image.

		GLuint textureID;
		glGenTextures(1, &textureID);
		
		// "Bind" the newly created texture : all future texture functions will modify this texture
		glBindTexture(GL_TEXTURE_2D, textureID);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

		free(buffer);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); 
		glGenerateMipmap(GL_TEXTURE_2D);

		textures[id].data = textureID;
	}
	textures[id].refs++;
	return textures[id].data;
}

void Loader::unload_texture(Asset::ID id)
{
	vi_assert(textures[id].refs > 0);
	if (--textures[id].refs == 0)
		glDeleteTextures(1, &textures[id].data);
}

GLuint Loader::shader(Asset::ID id)
{
	if (!shaders[id].refs)
	{
		const char* path = Asset::Shader::filenames[id];

		Array<char> code;
		FILE* f = fopen(path, "r");
		if (!f)
		{
			fprintf(stderr, "Can't open shader source file '%s'", path);
			return 0;
		}

		const size_t chunk_size = 4096;
		int i = 1;
		while (true)
		{
			code.reserve(i * chunk_size + 1); // extra char since this will be a null-terminated string
			size_t read = fread(code.data, sizeof(char), chunk_size, f);
			if (read < chunk_size)
			{
				code.length = ((i - 1) * chunk_size) + read;
				break;
			}
			i++;
		}
		fclose(f);

		// Create the shaders
		GLuint vertex_id = glCreateShader(GL_VERTEX_SHADER);
		GLuint frag_id = glCreateShader(GL_FRAGMENT_SHADER);

		// Compile Vertex Shader
		char const* vertex_code[] = { "#version 330 core\n#define VERTEX\n", code.data };
		glShaderSource(vertex_id, 2, vertex_code, NULL);
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
			fprintf(stderr, "Vertex shader error in '%s': %s\n", path, msg.data);
		}

		// Compile Fragment Shader
		const char* frag_code[2] = { "#version 330 core\n", code.data };
		glShaderSource(frag_id, 2, frag_code, NULL);
		glCompileShader(frag_id);

		// Check Fragment Shader
		glGetShaderiv(frag_id, GL_COMPILE_STATUS, &result);
		glGetShaderiv(frag_id, GL_INFO_LOG_LENGTH, &msg_length);
		if (msg_length > 1)
		{
			Array<char> msg(msg_length + 1);
			glGetShaderInfoLog(frag_id, msg_length, NULL, msg.data);
			fprintf(stderr, "Fragment shader error in '%s': %s\n", path, msg.data);
		}

		// Link the program
		GLuint program_id = glCreateProgram();
		glAttachShader(program_id, vertex_id);
		glAttachShader(program_id, frag_id);
		glLinkProgram(program_id);

		// Check the program
		glGetProgramiv(program_id, GL_LINK_STATUS, &result);
		glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &msg_length);
		if (msg_length > 1)
		{
			Array<char> msg(msg_length);
			glGetProgramInfoLog(program_id, msg_length, NULL, msg.data);
			fprintf(stderr, "Error creating shader program '%s': %s\n", path, msg.data);
		}

		glDeleteShader(vertex_id);
		glDeleteShader(frag_id);

		shaders[id].data = program_id;;
	}
	shaders[id].refs++;
	return shaders[id].data;
}

void Loader::unload_shader(Asset::ID id)
{
	vi_assert(shaders[id].refs > 0);
	if (--shaders[id].refs == 0)
		glDeleteProgram(shaders[id].data);
}
