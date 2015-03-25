#include "load.h"
#include <stdio.h>
#include <GL/glew.h>
#include "lodepng.h"

Loader::Loader()
{
	memset(this, 0, sizeof(Loader));
}

Mesh* Loader::mesh(Asset::ID id)
{
	if (meshes[id].vertices.length == 0)
	{
		const char* path = Asset::Model::filenames[id];
		FILE* f = fopen(path, "rb");
		if (!f)
		{
			fprintf(stderr, "Can't open file");
			return 0;
		}

		Mesh* mesh = &meshes[id];

		// Read indices
		int count;
		fread(&count, sizeof(int), 1, f);
		fprintf(stderr, "Indices: %u\n", count);

		// Fill face indices
		mesh->indices.reserve(count);
		mesh->indices.length = count;
		fread(mesh->indices.data, sizeof(int), count, f);

		fread(&count, sizeof(int), 1, f);
		fprintf(stderr, "Vertices: %u\n", count);

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
	}
	return &meshes[id];
}

GLuint Loader::texture(Asset::ID id)
{
	if (!textures[id])
	{
		const char* path = Asset::Texture::filenames[id];
		unsigned char* buffer;
		unsigned width, height;

		unsigned error = lodepng_decode32_file(&buffer, &width, &height, path);

		if (error)
		{
			fprintf(stderr, "%s - %s\n", lodepng_error_text(error), path);
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

		textures[id] = textureID;
	}
	return textures[id];
}

GLuint Loader::shader(Asset::ID id)
{
	if (!shaders[id])
	{
		const char* path = Asset::Shader::filenames[id];

		Array<char> code;
		FILE* f = fopen(path, "r");
		if (!f)
		{
			fprintf(stderr, "Can't open file");
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
		GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
		GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

		GLint Result = GL_FALSE;
		int InfoLogLength;

		// Compile Vertex Shader
		printf("Compiling shader : %s\n", path);
		char const * VertexSourcePointer[] = { "#version 330 core\n#define VERTEX\n", code.data };
		glShaderSource(VertexShaderID, 2, VertexSourcePointer, NULL);
		glCompileShader(VertexShaderID);

		// Check Vertex Shader
		glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
		glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
		if ( InfoLogLength > 0 ){
			Array<char> VertexShaderErrorMessage(InfoLogLength + 1);
			glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, VertexShaderErrorMessage.data);
			printf("%s\n", VertexShaderErrorMessage.data);
		}



		// Compile Fragment Shader
		printf("Compiling shader : %s\n", path);
		char const * FragmentSourcePointer[2] = { "#version 330 core\n", code.data };
		glShaderSource(FragmentShaderID, 2, FragmentSourcePointer, NULL);
		glCompileShader(FragmentShaderID);

		// Check Fragment Shader
		glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
		glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
		if (InfoLogLength > 0)
		{
			Array<char> FragmentShaderErrorMessage(InfoLogLength + 1);
			glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, FragmentShaderErrorMessage.data);
			printf("%s\n", FragmentShaderErrorMessage.data);
		}

		// Link the program
		printf("Linking program\n");
		GLuint ProgramID = glCreateProgram();
		glAttachShader(ProgramID, VertexShaderID);
		glAttachShader(ProgramID, FragmentShaderID);
		glLinkProgram(ProgramID);

		// Check the program
		glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
		glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
		if ( InfoLogLength > 0 ){
			Array<char> ProgramErrorMessage(InfoLogLength+1);
			glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, ProgramErrorMessage.data);
			printf("%s\n", ProgramErrorMessage.data);
		}

		glDeleteShader(VertexShaderID);
		glDeleteShader(FragmentShaderID);

		shaders[id] = ProgramID;
	}
	return shaders[id];
}
