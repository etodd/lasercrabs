#include "render.h"
#include "load.h"
#include "data/mesh.h"
#include "vi_assert.h"

namespace VI
{

Camera Camera::main = Camera();

RenderSync::Swapper RenderSync::swapper(size_t index)
{
	RenderSync::Swapper q;
	q.data = data;
	q.current = index;
	return q;
}

template<typename T> void add_attrib(GLData::Mesh* gl, T* data, size_t count, size_t element_count, GLuint type)
{
	GLData::Mesh::Attrib a;
	a.element_size = sizeof(T) * element_count / 4;
	a.type = type;
	glBindVertexArray(gl->vertex_array);
	glGenBuffers(1, &a.handle);
	glBindBuffer(GL_ARRAY_BUFFER, a.handle);
	glBufferData(GL_ARRAY_BUFFER, count * sizeof(T) * element_count, data, GL_STATIC_DRAW);
	gl->attribs.add(a);
}

void load_mesh(SyncData* sync, GLData* data, GLData::Mesh* mesh)
{
	size_t vertex_count = *(sync->read<size_t>());

	glGenVertexArrays(1, &mesh->vertex_array);
	glBindVertexArray(mesh->vertex_array);

	size_t attrib_count = *(sync->read<size_t>());
	for (size_t i = 0; i < attrib_count; i++)
	{
		RenderDataType attrib_type = *(sync->read<RenderDataType>());
		size_t attrib_element_count = *(sync->read<size_t>());
		switch (attrib_type)
		{
			case RenderDataType_Float:
			{
				float* data = sync->read<float>(vertex_count * attrib_element_count);
				add_attrib<float>(mesh, data, vertex_count, attrib_element_count, GL_FLOAT);
				break;
			}
			case RenderDataType_Vec2:
			{
				Vec2* data = sync->read<Vec2>(vertex_count * attrib_element_count);
				add_attrib<Vec2>(mesh, data, vertex_count, attrib_element_count, GL_FLOAT);
				break;
			}
			case RenderDataType_Vec3:
			{
				Vec3* data = sync->read<Vec3>(vertex_count * attrib_element_count);
				add_attrib<Vec3>(mesh, data, vertex_count, attrib_element_count, GL_FLOAT);
				break;
			}
			case RenderDataType_Vec4:
			{
				Vec4* data = sync->read<Vec4>(vertex_count * attrib_element_count);
				add_attrib<Vec4>(mesh, data, vertex_count, attrib_element_count, GL_FLOAT);
				break;
			}
			case RenderDataType_Int:
			{
				int* data = sync->read<int>(vertex_count * attrib_element_count);
				add_attrib<int>(mesh, data, vertex_count, attrib_element_count, GL_INT);
				break;
			}
			case RenderDataType_Mat4:
			{
				Mat4* data = sync->read<Mat4>(vertex_count * attrib_element_count);
				add_attrib<Mat4>(mesh, data, vertex_count, attrib_element_count, GL_FLOAT);
				break;
			}
		}
	}

	size_t index_count = *(sync->read<size_t>());
	int* indices = sync->read<int>(index_count);

	glGenBuffers(1, &mesh->index_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->index_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(int), indices, GL_STATIC_DRAW);
	mesh->index_count = index_count;
}

void unload_mesh(GLData::Mesh* mesh)
{
	for (size_t i = 0; i < mesh->attribs.length; i++)
		glDeleteBuffers(1, &mesh->attribs.data[i].handle);
	glDeleteVertexArrays(1, &mesh->vertex_array);
	mesh->~Mesh();
}

void render_mesh(SyncData* sync, GLData* data, GLData::Mesh* mesh)
{
	AssetID shader_asset = *(sync->read<AssetID>());

	GLuint programID = data->shaders[shader_asset].handle;

	glUseProgram(programID);

	int texture_index = 0;

	int uniform_count = *(sync->read<int>());
	for (int i = 0; i < uniform_count; i++)
	{
		AssetID uniform_asset = *(sync->read<AssetID>());
		GLuint uniform_id = data->shaders[shader_asset].uniforms[uniform_asset];
		RenderDataType uniform_type = *(sync->read<RenderDataType>());
		int uniform_count = *(sync->read<int>());
		switch (uniform_type)
		{
			case RenderDataType_Float:
				glUniform1fv(uniform_id, uniform_count, sync->read<float>(uniform_count));
				break;
			case RenderDataType_Vec2:
				glUniform2fv(uniform_id, uniform_count, (float*)sync->read<Vec2>(uniform_count));
				break;
			case RenderDataType_Vec3:
				glUniform3fv(uniform_id, uniform_count, (float*)sync->read<Vec3>(uniform_count));
				break;
			case RenderDataType_Vec4:
				glUniform4fv(uniform_id, uniform_count, (float*)sync->read<Vec4>(uniform_count));
				break;
			case RenderDataType_Int:
				glUniform1iv(uniform_id, uniform_count, sync->read<int>(uniform_count));
				break;
			case RenderDataType_Mat4:
				glUniformMatrix4fv(uniform_id, uniform_count, GL_FALSE, (float*)sync->read<Mat4>(uniform_count));
				break;
			case RenderDataType_Texture:
				vi_assert(uniform_count == 1); // Only single textures supported for now
				AssetID texture_asset = *(sync->read<AssetID>(uniform_count));
				GLuint texture_id = data->textures[texture_asset];
				glActiveTexture(GL_TEXTURE0 + texture_index);
				GLenum texture_type = *(sync->read<GLenum>(uniform_count));
				glBindTexture(texture_type, texture_id);
				glUniform1i(uniform_id, texture_index);
				texture_index++;
				break;
		}
	}

	for (size_t i = 0; i < mesh->attribs.length; i++)
	{
		glEnableVertexAttribArray(i);
		glBindBuffer(GL_ARRAY_BUFFER, mesh->attribs.data[i].handle);
		if (mesh->attribs.data[i].type == GL_INT)
		{
			glVertexAttribIPointer
			(
				i,                                    // attribute
				mesh->attribs.data[i].element_size,     // size
				mesh->attribs.data[i].type,             // type
				0,                                    // stride
				(void*)0                              // array buffer offset
			);
		}
		else
		{
			glVertexAttribPointer
			(
				i,                                    // attribute
				mesh->attribs.data[i].element_size,     // size
				mesh->attribs.data[i].type,             // type
				GL_FALSE,                             // normalized?
				0,                                    // stride
				(void*)0                              // array buffer offset
			);
		}
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->index_buffer);

	glDrawElements(
		GL_TRIANGLES,       // mode
		mesh->index_count,    // count
		GL_UNSIGNED_INT,    // type
		(void*)0            // element array buffer offset
	);

	for (size_t i = 0; i < mesh->attribs.length; i++)
		glDisableVertexAttribArray(i);
}

void render(SyncData* sync, GLData* data)
{
	sync->read_pos = 0;
	while (sync->read_pos < sync->queue.length)
	{
		RenderOp op = *(sync->read<RenderOp>());
		switch (op)
		{
			case RenderOp_LoadMesh:
			{
				AssetID id = *(sync->read<AssetID>());
				GLData::Mesh* mesh = &data->meshes[id];
				new (mesh) GLData::Mesh();
				load_mesh(sync, data, mesh);
				break;
			}
			case RenderOp_UnloadMesh:
			{
				AssetID id = *(sync->read<AssetID>());
				GLData::Mesh* mesh = &data->meshes[id];
				unload_mesh(mesh);
				break;
			}
			case RenderOp_LoadDynamicMesh:
			{
				size_t id = *(sync->read<size_t>());
				GLData::Mesh* mesh = &data->dynamic_meshes[id];
				new (mesh) GLData::Mesh();
				load_mesh(sync, data, mesh);
				break;
			}
			case RenderOp_UnloadDynamicMesh:
			{
				size_t id = *(sync->read<size_t>());
				GLData::Mesh* mesh = &data->dynamic_meshes[id];
				unload_mesh(mesh);
				break;
			}
			case RenderOp_LoadTexture:
			{
				AssetID id = *(sync->read<AssetID>());
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

				data->textures[id] = textureID;
				break;
			}
			case RenderOp_UnloadTexture:
			{
				AssetID id = *(sync->read<AssetID>());
				glDeleteTextures(1, &data->textures[id]);
				break;
			}
			case RenderOp_LoadShader:
			{
				AssetID id = *(sync->read<AssetID>());
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

				for (int i = 0; i < Asset::Uniform::count; i++)
					data->shaders[id].uniforms[i] = glGetUniformLocation(program_id, Asset::Uniform::filenames[i]);

				glDeleteShader(vertex_id);
				glDeleteShader(frag_id);

				data->shaders[id].handle = program_id;
				break;
			}
			case RenderOp_UnloadShader:
			{
				AssetID id = *(sync->read<AssetID>());
				glDeleteProgram(data->shaders[id].handle);
				break;
			}
			case RenderOp_Clear:
			{
				// Clear the screen
				GLbitfield clear_flags = *(sync->read<GLbitfield>());
				glClear(clear_flags);
				break;
			}
			case RenderOp_Mesh:
			{
				AssetID id = *(sync->read<AssetID>());
				GLData::Mesh* gl = &data->meshes[id];
				render_mesh(sync, data, gl);
				break;
			}
			case RenderOp_DynamicMesh:
			{
				size_t id = *(sync->read<size_t>());
				GLData::Mesh* gl = &data->dynamic_meshes[id];
				render_mesh(sync, data, gl);
				break;
			}
		}
	}
}

}