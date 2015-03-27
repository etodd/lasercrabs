#include "render.h"
#include "load.h"
#include "data/mesh.h"

RenderSync::RenderSync()
	: data()
{
}

Swapper RenderSync::swapper(size_t index)
{
	Swapper q;
	q.sync = this;
	q.current = index;
	return q;
}

SyncData* Swapper::data()
{
	return &sync->data[current];
}

template<typename T> void add_attrib(MeshGL* gl, T* data, size_t count, GLuint type)
{
	MeshGL::Attrib a;
	a.element_size = sizeof(T) / 4;
	a.type = type;
	glBindVertexArray(gl->vertex_array);
	glGenBuffers(1, &a.handle);
	glBindBuffer(GL_ARRAY_BUFFER, a.handle);
	glBufferData(GL_ARRAY_BUFFER, count * sizeof(T), data, GL_STATIC_DRAW);
	gl->attribs.add(a);
}

void render(SyncData* sync, Loader* loader)
{
	sync->read_pos = 0;
	while (sync->read_pos < sync->queue.length)
	{
		RenderOp op = *(sync->read<RenderOp>());
		switch (op)
		{
			case RenderOp_LoadMesh:
			{
				Asset::ID id = *(sync->read<Asset::ID>());
				size_t vertex_count = *(sync->read<size_t>());
				Vec3* vertices = sync->read<Vec3>(vertex_count);
				Vec2* uvs = sync->read<Vec2>(vertex_count);
				Vec3* normals = sync->read<Vec3>(vertex_count);
				size_t index_count = *(sync->read<size_t>());
				int* indices = sync->read<int>(index_count);

				MeshGL* mesh = &loader->gl_meshes[id];
				new (mesh) MeshGL();

				glGenVertexArrays(1, &mesh->vertex_array);
				glBindVertexArray(mesh->vertex_array);
				glGenBuffers(1, &mesh->index_buffer);

				add_attrib<Vec3>(mesh, vertices, vertex_count, GL_FLOAT);
				add_attrib<Vec2>(mesh, uvs, vertex_count, GL_FLOAT);
				add_attrib<Vec3>(mesh, normals, vertex_count, GL_FLOAT);
				
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->index_buffer);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(int), indices, GL_STATIC_DRAW);
				mesh->index_count = index_count;

				break;
			}
			case RenderOp_UnloadMesh:
			{
				Asset::ID id = *(sync->read<Asset::ID>());
				MeshGL* mesh = &loader->gl_meshes[id];
				for (int i = 0; i < mesh->attribs.length; i++)
					glDeleteBuffers(1, &mesh->attribs.data[i].handle);
				glDeleteVertexArrays(1, &mesh->vertex_array);
				mesh->~MeshGL();
				break;
			}
			case RenderOp_LoadTexture:
			{
				Asset::ID id = *(sync->read<Asset::ID>());
				unsigned width = *(sync->read<unsigned>());
				unsigned height = *(sync->read<unsigned>());
				unsigned char* buffer = sync->read<unsigned char>(4 * width * height);
				// Make power of two version of the image.

				GLuint textureID;
				glGenTextures(1, &textureID);
				
				// "Bind" the newly created texture : all future texture functions will modify this texture
				glBindTexture(GL_TEXTURE_2D, textureID);

				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); 
				glGenerateMipmap(GL_TEXTURE_2D);

				loader->textures[id].data = textureID;
				break;
			}
			case RenderOp_UnloadTexture:
			{
				Asset::ID id = *(sync->read<Asset::ID>());
				glDeleteTextures(1, &loader->textures[id].data);
				break;
			}
			case RenderOp_LoadShader:
			{
				Asset::ID id = *(sync->read<Asset::ID>());
				const char* path = Asset::Shader::filenames[id];
				size_t code_length = *(sync->read<size_t>());
				char* code = sync->read<char>(code_length);

				// Create the shaders
				GLuint vertex_id = glCreateShader(GL_VERTEX_SHADER);
				GLuint frag_id = glCreateShader(GL_FRAGMENT_SHADER);

				// Compile Vertex Shader
				char const* vertex_code[] = { "#version 330 core\n#define VERTEX\n", code };
				const GLint vertex_code_length[] = { 33, (GLint)code_length };
				glShaderSource(vertex_id, 2, vertex_code, vertex_code_length);
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
				const char* frag_code[2] = { "#version 330 core\n", code };
				const GLint frag_code_length[] = { 18, (GLint)code_length };
				glShaderSource(frag_id, 2, frag_code, frag_code_length);
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

				loader->shaders[id].data = program_id;
				break;
			}
			case RenderOp_UnloadShader:
			{
				Asset::ID id = *(sync->read<Asset::ID>());
				glDeleteProgram(loader->shaders[id].data);
				break;
			}
			case RenderOp_Clear:
			{
				// Clear the screen
				GLbitfield clear_flags = *(sync->read<GLbitfield>());
				glClear(clear_flags);
				break;
			}
			case RenderOp_View:
			{
				Asset::ID id = *(sync->read<Asset::ID>());
				MeshGL* gl = &loader->gl_meshes[id];
				Asset::ID shader_asset = *(sync->read<Asset::ID>());
				Asset::ID texture_asset = *(sync->read<Asset::ID>());
				Mat4* MVP = sync->read<Mat4>();
				Mat4* ModelMatrix = sync->read<Mat4>();
				Mat4* ViewMatrix = sync->read<Mat4>();

				GLuint programID = loader->shaders[shader_asset].data;
				GLuint Texture = loader->textures[texture_asset].data;

				// Get a handle for our "MVP" uniform
				GLuint MatrixID = glGetUniformLocation(programID, "MVP");
				GLuint ViewMatrixID = glGetUniformLocation(programID, "V");
				GLuint ModelMatrixID = glGetUniformLocation(programID, "M");

				// Get a handle for our "myTextureSampler" uniform
				GLuint TextureID  = glGetUniformLocation(programID, "myTextureSampler");

				// Get a handle for our "LightPosition" uniform
				GLuint LightID = glGetUniformLocation(programID, "LightPosition_worldspace");

				glUseProgram(programID);

				// Send our transformation to the currently bound shader, 
				// in the "MVP" uniform
				glUniformMatrix4fv(MatrixID, 1, GL_FALSE, (float*)MVP);
				glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, (float*)ModelMatrix);
				glUniformMatrix4fv(ViewMatrixID, 1, GL_FALSE, (float*)ViewMatrix);

				Vec3 lightPos = Vec3(4, 4, 4);
				glUniform3f(LightID, lightPos.x, lightPos.y, lightPos.z);

				// Bind our texture in Texture Unit 0
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, Texture);
				// Set our "myTextureSampler" sampler to user Texture Unit 0
				glUniform1i(TextureID, 0);

				for (int i = 0; i < gl->attribs.length; i++)
				{
					glEnableVertexAttribArray(i);
					glBindBuffer(GL_ARRAY_BUFFER, gl->attribs.data[i].handle);
					glVertexAttribPointer(
						i,                                    // attribute
						gl->attribs.data[i].element_size,     // size
						gl->attribs.data[i].type,             // type
						GL_FALSE,                             // normalized?
						0,                                    // stride
						(void*)0                              // array buffer offset
					);
				}

				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl->index_buffer);

				glDrawElements(
					GL_TRIANGLES,       // mode
					gl->index_count,    // count
					GL_UNSIGNED_INT,    // type
					(void*)0            // element array buffer offset
				);

				for (int i = 0; i < gl->attribs.length; i++)
					glDisableVertexAttribArray(i);

				break;
			}
		}
	}
}
