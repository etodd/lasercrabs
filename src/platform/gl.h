#include "render/render.h"
#include "vi_assert.h"
#include "asset/lookup.h"
#include <GL/glew.h>

namespace VI
{

struct GLData
{
	struct Mesh
	{
		struct Attrib
		{
			int total_element_size;
			int element_count;
			RenderDataType data_type;
			GLuint gl_type;
			GLuint handle;
		};

		Array<Attrib> attribs;
		GLuint index_buffer;
		GLuint vertex_array;
		int index_count;
		bool dynamic;

		Mesh()
			: attribs(), index_buffer(), vertex_array(), index_count()
		{
			
		}
	};

	struct Shader
	{
		GLuint handle;
		Array<GLuint> uniforms;
	};

	static Array<GLuint> textures;
	static Array<Shader> shaders;
	static Array<Mesh> meshes;
	static Array<GLuint> framebuffers;
};

Array<GLuint> GLData::textures = Array<GLuint>();
Array<GLData::Shader> GLData::shaders = Array<GLData::Shader>();
Array<GLData::Mesh> GLData::meshes = Array<GLData::Mesh>();
Array<GLuint> GLData::framebuffers = Array<GLuint>();

void render(RenderSync* sync)
{
	sync->read_pos = 0;
	while (sync->read_pos < sync->queue.length)
	{
		RenderOp op = *(sync->read<RenderOp>());
		switch (op)
		{
			case RenderOp_Viewport:
			{
				const ScreenRect* rect = sync->read<ScreenRect>();
				glViewport(rect->x, rect->y, rect->width, rect->height);
				break;
			}
			case RenderOp_AllocMesh:
			{
				int id = *(sync->read<int>());
				if (id >= GLData::meshes.length)
					GLData::meshes.resize(id + 1);
				GLData::Mesh* mesh = &GLData::meshes[id];
				new (mesh) GLData::Mesh();
				mesh->dynamic = sync->read<bool>();

				glGenVertexArrays(1, &mesh->vertex_array);
				glBindVertexArray(mesh->vertex_array);

				int attrib_count = *(sync->read<int>());
				for (int i = 0; i < attrib_count; i++)
				{
					GLData::Mesh::Attrib a;
					glGenBuffers(1, &a.handle);
					a.data_type = *(sync->read<RenderDataType>());
					a.element_count = *(sync->read<int>());
					switch (a.data_type)
					{
						case RenderDataType_Int:
							a.total_element_size = a.element_count;
							a.gl_type = GL_INT;
							break;
						case RenderDataType_Float:
							a.total_element_size = a.element_count;
							a.gl_type = GL_FLOAT;
							break;
						case RenderDataType_Vec2:
							a.total_element_size = a.element_count * sizeof(Vec2) / 4;
							a.gl_type = GL_FLOAT;
							break;
						case RenderDataType_Vec3:
							a.total_element_size = a.element_count * sizeof(Vec3) / 4;
							a.gl_type = GL_FLOAT;
							break;
						case RenderDataType_Vec4:
							a.total_element_size = a.element_count * sizeof(Vec4) / 4;
							a.gl_type = GL_FLOAT;
							break;
						case RenderDataType_Mat4:
							a.total_element_size = a.element_count * sizeof(Mat4) / 4;
							a.gl_type = GL_FLOAT;
							break;
						default:
							vi_assert(false);
							break;
					}
					mesh->attribs.add(a);
				}
				glGenBuffers(1, &mesh->index_buffer);

				break;
			}
			case RenderOp_UpdateAttribBuffers:
			{
				int id = *(sync->read<int>());
				GLData::Mesh* mesh = &GLData::meshes[id];
				glBindVertexArray(mesh->vertex_array);

				GLenum usage = mesh->dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;

				int count = *(sync->read<int>());

				for (int i = 0; i < mesh->attribs.length; i++)
				{
					GLData::Mesh::Attrib* attrib = &mesh->attribs[i];

					glBindBuffer(GL_ARRAY_BUFFER, attrib->handle);
					switch (attrib->data_type)
					{
						case RenderDataType_Float:
						{
							glBufferData(GL_ARRAY_BUFFER, count * sizeof(float) * attrib->element_count, sync->read<float>(count * attrib->element_count), usage);
							break;
						}
						case RenderDataType_Vec2:
						{
							glBufferData(GL_ARRAY_BUFFER, count * sizeof(Vec2) * attrib->element_count, sync->read<Vec2>(count * attrib->element_count), usage);
							break;
						}
						case RenderDataType_Vec3:
						{
							glBufferData(GL_ARRAY_BUFFER, count * sizeof(Vec3) * attrib->element_count, sync->read<Vec3>(count * attrib->element_count), usage);
							break;
						}
						case RenderDataType_Vec4:
						{
							glBufferData(GL_ARRAY_BUFFER, count * sizeof(Vec4) * attrib->element_count, sync->read<Vec4>(count * attrib->element_count), usage);
							break;
						}
						case RenderDataType_Int:
						{
							glBufferData(GL_ARRAY_BUFFER, count * sizeof(int) * attrib->element_count, sync->read<int>(count * attrib->element_count), usage);
							break;
						}
						case RenderDataType_Mat4:
						{
							glBufferData(GL_ARRAY_BUFFER, count * sizeof(Mat4) * attrib->element_count, sync->read<Mat4>(count * attrib->element_count), usage);
							break;
						}
						default:
							vi_assert(false);
							break;
					}
				}
				break;
			}
			case RenderOp_UpdateIndexBuffer:
			{
				int id = *(sync->read<int>());
				GLData::Mesh* mesh = &GLData::meshes[id];
				int index_count = *(sync->read<int>());
				const int* indices = sync->read<int>(index_count);

				glBindVertexArray(mesh->vertex_array);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->index_buffer);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(int), indices, mesh->dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
				mesh->index_count = index_count;
				break;
			}
			case RenderOp_FreeMesh:
			{
				AssetID id = *(sync->read<AssetID>());
				GLData::Mesh* mesh = &GLData::meshes[id];
				for (int i = 0; i < mesh->attribs.length; i++)
					glDeleteBuffers(1, &mesh->attribs.data[i].handle);
				glDeleteVertexArrays(1, &mesh->vertex_array);
				mesh->~Mesh();
				break;
			}
			case RenderOp_AllocTexture:
			{
				AssetID id = *(sync->read<AssetID>());
				if (id >= GLData::textures.length)
					GLData::textures.resize(id + 1);
				glGenTextures(1, &GLData::textures[id]);
				break;
			}
			case RenderOp_DynamicTexture:
			{
				AssetID id = *(sync->read<AssetID>());
				unsigned width = *(sync->read<unsigned>());
				unsigned height = *(sync->read<unsigned>());
				RenderDynamicTextureType type = *(sync->read<RenderDynamicTextureType>());
				glBindTexture(GL_TEXTURE_2D, GLData::textures[id]);
				switch (type)
				{
					case RenderDynamicTexture_Color:
						glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
						break;
					case RenderDynamicTexture_Depth:
						glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 0);
						break;
					default:
						vi_assert(false);
						break;
				}

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST); 
				break;
			}
			case RenderOp_LoadTexture:
			{
				AssetID id = *(sync->read<AssetID>());
				unsigned width = *(sync->read<unsigned>());
				unsigned height = *(sync->read<unsigned>());
				const unsigned char* buffer = sync->read<unsigned char>(4 * width * height);
				glBindTexture(GL_TEXTURE_2D, GLData::textures[id]);

				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); 
				glGenerateMipmap(GL_TEXTURE_2D);

				break;
			}
			case RenderOp_FreeTexture:
			{
				AssetID id = *(sync->read<AssetID>());
				glDeleteTextures(1, &GLData::textures[id]);
				break;
			}
			case RenderOp_LoadShader:
			{
				AssetID id = *(sync->read<AssetID>());
				const char* path = AssetLookup::Shader::values[id];
				int code_length = *(sync->read<int>());
				const char* code = sync->read<char>(code_length);

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

				if (id >= GLData::shaders.length)
					GLData::shaders.resize(id + 1);
				GLData::shaders[id].handle = program_id;
				break;
			}
			case RenderOp_FreeShader:
			{
				AssetID id = *(sync->read<AssetID>());
				glDeleteProgram(GLData::shaders[id].handle);
				break;
			}
			case RenderOp_DepthMask:
			{
				glDepthMask(*(sync->read<bool>()));
				break;
			}
			case RenderOp_Clear:
			{
				// Clear the screen
				GLbitfield clear_flags = 0;
				if (*(sync->read<bool>()))
					clear_flags |= GL_COLOR_BUFFER_BIT;
				if (*(sync->read<bool>()))
					clear_flags |= GL_DEPTH_BUFFER_BIT;
				glClear(clear_flags);
				break;
			}
			case RenderOp_Mesh:
			{
				int id = *(sync->read<int>());
				GLData::Mesh* mesh = &GLData::meshes[id];
				AssetID shader_asset = *(sync->read<AssetID>());

				GLuint program_id = GLData::shaders[shader_asset].handle;

				glUseProgram(program_id);

				int texture_index = 0;

				int uniform_count = *(sync->read<int>());
				for (int i = 0; i < uniform_count; i++)
				{
					AssetID uniform_asset = *(sync->read<AssetID>());

					if (uniform_asset >= GLData::shaders[shader_asset].uniforms.length)
					{
						int old_length = GLData::shaders[shader_asset].uniforms.length;
						GLData::shaders[shader_asset].uniforms.resize(uniform_asset + 1);
						for (int j = old_length; j < uniform_asset + 1; j++)
							GLData::shaders[shader_asset].uniforms[j] = glGetUniformLocation(program_id, AssetLookup::Uniform::values[j]);
					}

					GLuint uniform_id = GLData::shaders[shader_asset].uniforms[uniform_asset];
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
						{
							vi_assert(uniform_count == 1); // Only single textures supported for now
							AssetID texture_asset = *(sync->read<AssetID>(uniform_count));
							GLuint texture_id;
							if (texture_asset == AssetNull)
								texture_id = 0;
							else
								texture_id = GLData::textures[texture_asset];
							glActiveTexture(GL_TEXTURE0 + texture_index);
							RenderTextureType texture_type = *(sync->read<RenderTextureType>());
							GLenum gl_texture_type;
							switch (texture_type)
							{
								case RenderTexture2D:
									gl_texture_type = GL_TEXTURE_2D;
									break;
								default:
									vi_assert(false); // Only 2D textures supported for now
									break;
							}
							glBindTexture(gl_texture_type, texture_id);
							glUniform1i(uniform_id, texture_index);
							texture_index++;
							break;
						}
						default:
						{
							vi_assert(false);
							break;
						}
					}
				}

				for (int i = 0; i < mesh->attribs.length; i++)
				{
					glEnableVertexAttribArray(i);
					glBindBuffer(GL_ARRAY_BUFFER, mesh->attribs.data[i].handle);
					if (mesh->attribs.data[i].gl_type == GL_INT)
					{
						glVertexAttribIPointer
						(
							i,                                    // attribute
							mesh->attribs.data[i].total_element_size,     // size
							mesh->attribs.data[i].gl_type,             // type
							0,                                    // stride
							(void*)0                              // array buffer offset
						);
					}
					else
					{
						glVertexAttribPointer
						(
							i,                                    // attribute
							mesh->attribs.data[i].total_element_size,     // size
							mesh->attribs.data[i].gl_type,             // type
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

				for (int i = 0; i < mesh->attribs.length; i++)
					glDisableVertexAttribArray(i);
				break;
			}
			case RenderOp_BlendMode:
			{
				RenderBlendMode mode = *(sync->read<RenderBlendMode>());
				switch (mode)
				{
					case RenderBlend_Opaque:
						glDisablei(GL_BLEND, 0);
						break;
					case RenderBlend_Alpha:
						glEnablei(GL_BLEND, 0);
						glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
						break;
					case RenderBlend_Additive:
						glEnablei(GL_BLEND, 0);
						glBlendFunc(GL_SRC_ALPHA, GL_ONE);
						break;
					default:
						vi_assert(false);
						break;
				}
				break;
			}
			case RenderOp_CullMode:
			{
				RenderCullMode mode = *(sync->read<RenderCullMode>());
				switch (mode)
				{
					case RenderCull_Back:
						glEnable(GL_CULL_FACE);
						glCullFace(GL_BACK);
						break;
					case RenderCull_Front:
						glEnable(GL_CULL_FACE);
						glCullFace(GL_FRONT);
						break;
					case RenderCull_None:
						glDisable(GL_CULL_FACE);
						break;
					default:
						vi_assert(false);
						break;
				}
				break;
			}
			case RenderOp_AllocFramebuffer:
			{
				int id = *(sync->read<int>());
				if (id >= GLData::framebuffers.length)
					GLData::framebuffers.resize(id + 1);

				glGenFramebuffersEXT(1, &GLData::framebuffers[id]);
				glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, GLData::framebuffers[id]);

				int attachments = *(sync->read<int>());
				for (int i = 0; i < attachments; i++)
				{
					RenderFramebufferAttachment attachment_type = *(sync->read<RenderFramebufferAttachment>());
					int texture_id = *(sync->read<int>());
					GLuint gl_texture_id = GLData::textures[texture_id];
					switch (attachment_type)
					{
						case RenderFramebufferAttachment_Color0:
							glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, gl_texture_id, 0);
							break;
						case RenderFramebufferAttachment_Color1:
							glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_2D, gl_texture_id, 0);
							break;
						case RenderFramebufferAttachment_Color2:
							glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT, GL_TEXTURE_2D, gl_texture_id, 0);
							break;
						case RenderFramebufferAttachment_Color3:
							glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT3_EXT, GL_TEXTURE_2D, gl_texture_id, 0);
							break;
						case RenderFramebufferAttachment_Depth:
							glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, gl_texture_id, 0);
							break;
						default:
							vi_assert(false);
							break;
					}
				}

				vi_assert(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT);

				glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
				break;
			}
			case RenderOp_BindFramebuffer:
			{
				int id = *(sync->read<int>());
				glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, GLData::framebuffers[id]);
				break;
			}
			case RenderOp_FreeFramebuffer:
			{
				int id = *(sync->read<int>());
				glDeleteFramebuffersEXT(1, &GLData::framebuffers[id]);
				break;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}
	}
}


}