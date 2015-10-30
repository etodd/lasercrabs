#include "render/render.h"
#include "vi_assert.h"
#include "asset/lookup.h"
#include <GL/glew.h>
#include <array>

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
		GLuint instance_array;
		GLuint instance_buffer;
		int index_count;
		bool dynamic;

		Mesh()
			: attribs(), index_buffer(), vertex_array(), index_count(), instance_array(), instance_buffer()
		{
			
		}
	};

	struct ShaderTechnique
	{
		GLuint handle;
		Array<GLuint> uniforms;
	};

	typedef std::array<ShaderTechnique, (size_t)RenderTechnique::count> Shader;

	struct Texture
	{
		GLuint handle;
		unsigned width;
		unsigned height;
		RenderDynamicTextureType type;
		RenderTextureFilter filter;
	};

	static Array<Texture> textures;
	static Array<Shader> shaders;
	static Array<Mesh> meshes;
	static Array<GLuint> framebuffers;
	static AssetID current_shader_asset;
	static RenderTechnique current_shader_technique;
	static Array<AssetID> samplers;
};

Array<GLData::Texture> GLData::textures = Array<GLData::Texture>();
Array<GLData::Shader> GLData::shaders = Array<GLData::Shader>();
Array<GLData::Mesh> GLData::meshes = Array<GLData::Mesh>();
Array<GLuint> GLData::framebuffers = Array<GLuint>();
AssetID GLData::current_shader_asset = AssetNull;
RenderTechnique GLData::current_shader_technique = RenderTechnique::Default;
Array<AssetID> GLData::samplers = Array<AssetID>();

void render_init()
{
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); 
	glEnable(GL_CULL_FACE);
}

void bind_attrib_pointers(Array<GLData::Mesh::Attrib>& attribs)
{
	for (int i = 0; i < attribs.length; i++)
	{
		glEnableVertexAttribArray(i);
		const GLData::Mesh::Attrib& a = attribs[i];
		glBindBuffer(GL_ARRAY_BUFFER, a.handle);

		if (a.gl_type == GL_INT)
		{
			glVertexAttribIPointer
			(
				i,                                    // attribute
				a.total_element_size,     // size
				a.gl_type,             // type
				0,                                    // stride
				(void*)0                              // array buffer offset
			);
		}
		else
		{
			glVertexAttribPointer
			(
				i,                                    // attribute
				a.total_element_size,     // size
				a.gl_type,             // type
				GL_FALSE,                             // normalized?
				0,                                    // stride
				(void*)0                              // array buffer offset
			);
		}
	}
}

void render(RenderSync* sync)
{
#if DEBUG
#define debug_check() vi_assert((error = glGetError()) == GL_NO_ERROR)
#else
#define debug_check() {}
#endif
	sync->read_pos = 0;
	while (sync->read_pos < sync->queue.length)
	{
		GLenum error;
		RenderOp op = *(sync->read<RenderOp>());
		switch (op)
		{
			case RenderOp::Viewport:
			{
				const ScreenRect* rect = sync->read<ScreenRect>();
				glViewport(rect->x, rect->y, rect->width, rect->height);
				debug_check();
				break;
			}
			case RenderOp::AllocMesh:
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
						case RenderDataType::Int:
							a.total_element_size = a.element_count;
							a.gl_type = GL_INT;
							break;
						case RenderDataType::Float:
							a.total_element_size = a.element_count;
							a.gl_type = GL_FLOAT;
							break;
						case RenderDataType::Vec2:
							a.total_element_size = a.element_count * sizeof(Vec2) / 4;
							a.gl_type = GL_FLOAT;
							break;
						case RenderDataType::Vec3:
							a.total_element_size = a.element_count * sizeof(Vec3) / 4;
							a.gl_type = GL_FLOAT;
							break;
						case RenderDataType::Vec4:
							a.total_element_size = a.element_count * sizeof(Vec4) / 4;
							a.gl_type = GL_FLOAT;
							break;
						case RenderDataType::Mat4:
							vi_assert(false); // Not supported yet
							break;
						default:
							vi_assert(false);
							break;
					}

					mesh->attribs.add(a);
					debug_check();
				}

				bind_attrib_pointers(mesh->attribs);

				glGenBuffers(1, &mesh->index_buffer);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->index_buffer);

				debug_check();
				break;
			}
			case RenderOp::AllocInstances:
			{
				int id = *(sync->read<int>());

				// Assume the mesh is already loaded
				GLData::Mesh* mesh = &GLData::meshes[id];

				glGenVertexArrays(1, &mesh->instance_array);
				glBindVertexArray(mesh->instance_array);

				bind_attrib_pointers(mesh->attribs);

				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->index_buffer);

				// Right now the only supported instanced attribute is the world matrix

				glGenBuffers(1, &mesh->instance_buffer);
				glBindBuffer(GL_ARRAY_BUFFER, mesh->instance_buffer);

				glEnableVertexAttribArray(mesh->attribs.length + 0); 
				glEnableVertexAttribArray(mesh->attribs.length + 1); 
				glEnableVertexAttribArray(mesh->attribs.length + 2); 
				glEnableVertexAttribArray(mesh->attribs.length + 3); 
				glVertexAttribPointer(mesh->attribs.length + 0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(Vec4), (GLvoid*)(sizeof(Vec4) * 0));
				glVertexAttribPointer(mesh->attribs.length + 1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(Vec4), (GLvoid*)(sizeof(Vec4) * 1));
				glVertexAttribPointer(mesh->attribs.length + 2, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(Vec4), (GLvoid*)(sizeof(Vec4) * 2));
				glVertexAttribPointer(mesh->attribs.length + 3, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(Vec4), (GLvoid*)(sizeof(Vec4) * 3));
				glVertexAttribDivisor(mesh->attribs.length + 0, 1);
				glVertexAttribDivisor(mesh->attribs.length + 1, 1);
				glVertexAttribDivisor(mesh->attribs.length + 2, 1);
				glVertexAttribDivisor(mesh->attribs.length + 3, 1);

				debug_check();
				break;
			}
			case RenderOp::UpdateAttribBuffers:
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
						case RenderDataType::Float:
						{
							glBufferData(GL_ARRAY_BUFFER, count * sizeof(float) * attrib->element_count, sync->read<float>(count * attrib->element_count), usage);
							break;
						}
						case RenderDataType::Vec2:
						{
							glBufferData(GL_ARRAY_BUFFER, count * sizeof(Vec2) * attrib->element_count, sync->read<Vec2>(count * attrib->element_count), usage);
							break;
						}
						case RenderDataType::Vec3:
						{
							glBufferData(GL_ARRAY_BUFFER, count * sizeof(Vec3) * attrib->element_count, sync->read<Vec3>(count * attrib->element_count), usage);
							break;
						}
						case RenderDataType::Vec4:
						{
							glBufferData(GL_ARRAY_BUFFER, count * sizeof(Vec4) * attrib->element_count, sync->read<Vec4>(count * attrib->element_count), usage);
							break;
						}
						case RenderDataType::Int:
						{
							glBufferData(GL_ARRAY_BUFFER, count * sizeof(int) * attrib->element_count, sync->read<int>(count * attrib->element_count), usage);
							break;
						}
						case RenderDataType::Mat4:
						{
							glBufferData(GL_ARRAY_BUFFER, count * sizeof(Mat4) * attrib->element_count, sync->read<Mat4>(count * attrib->element_count), usage);
							break;
						}
						default:
							vi_assert(false);
							break;
					}
				}
				debug_check();
				break;
			}
			case RenderOp::UpdateIndexBuffer:
			{
				int id = *(sync->read<int>());
				GLData::Mesh* mesh = &GLData::meshes[id];
				int index_count = *(sync->read<int>());
				const int* indices = sync->read<int>(index_count);

				glBindVertexArray(mesh->vertex_array);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->index_buffer);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(int), indices, mesh->dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
				mesh->index_count = index_count;
				debug_check();
				break;
			}
			case RenderOp::FreeMesh:
			{
				AssetID id = *(sync->read<AssetID>());
				GLData::Mesh* mesh = &GLData::meshes[id];
				for (int i = 0; i < mesh->attribs.length; i++)
					glDeleteBuffers(1, &mesh->attribs.data[i].handle);
				glDeleteBuffers(1, &mesh->index_buffer);
				glDeleteBuffers(1, &mesh->instance_buffer);
				glDeleteVertexArrays(1, &mesh->instance_array);
				glDeleteVertexArrays(1, &mesh->vertex_array);
				mesh->~Mesh();
				debug_check();
				break;
			}
			case RenderOp::AllocTexture:
			{
				AssetID id = *(sync->read<AssetID>());
				if (id >= GLData::textures.length)
					GLData::textures.resize(id + 1);
				glGenTextures(1, &GLData::textures[id].handle);
				debug_check();
				break;
			}
			case RenderOp::DynamicTexture:
			{
				AssetID id = *(sync->read<AssetID>());
				unsigned width = *(sync->read<unsigned>());
				unsigned height = *(sync->read<unsigned>());
				RenderDynamicTextureType type = *(sync->read<RenderDynamicTextureType>());
				RenderTextureFilter filter = *(sync->read<RenderTextureFilter>());
				if (GLData::textures[id].width != width
					|| GLData::textures[id].height != height
					|| type != GLData::textures[id].type
					|| filter != GLData::textures[id].filter)
				{
					glBindTexture(type == RenderDynamicTextureType::ColorMultisample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, GLData::textures[id].handle);
					GLData::textures[id].width = width;
					GLData::textures[id].height = height;
					GLData::textures[id].type = type;
					GLData::textures[id].filter = filter;
					switch (type)
					{
						case RenderDynamicTextureType::ColorMultisample:
							glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 8, GL_RGBA8, width, height, false);
							break;
						case RenderDynamicTextureType::Color:
							glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
							break;
						case RenderDynamicTextureType::Depth:
							glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
							break;
						default:
							vi_assert(false);
							break;
					}

					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
					
					switch (filter)
					{
						case RenderTextureFilter::Nearest:
						{
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
							break;
						}
						case RenderTextureFilter::Linear:
						{
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
							break;
						}
					}
				}
				debug_check();
				break;
			}
			case RenderOp::LoadTexture:
			{
				AssetID id = *(sync->read<AssetID>());
				unsigned width = *(sync->read<unsigned>());
				unsigned height = *(sync->read<unsigned>());
				const unsigned char* buffer = sync->read<unsigned char>(4 * width * height);
				glBindTexture(GL_TEXTURE_2D, GLData::textures[id].handle);

				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); 
				glGenerateMipmap(GL_TEXTURE_2D);

				debug_check();
				break;
			}
			case RenderOp::FreeTexture:
			{
				AssetID id = *(sync->read<AssetID>());
				glDeleteTextures(1, &GLData::textures[id].handle);
				debug_check();
				break;
			}
			case RenderOp::LoadShader:
			{
				AssetID id = *(sync->read<AssetID>());

				if (id >= GLData::shaders.length)
					GLData::shaders.resize(id + 1);

				int code_length = *(sync->read<int>());
				const char* code = sync->read<char>(code_length);

				for (int i = 0; i < (int)RenderTechnique::count; i++)
					compile_shader(TechniquePrefixes::all[i], code, code_length, &GLData::shaders[id][i].handle);

				debug_check();
				break;
			}
			case RenderOp::FreeShader:
			{
				AssetID id = *(sync->read<AssetID>());
				for (int i = 0; i < (int)RenderTechnique::count; i++)
					glDeleteProgram(GLData::shaders[id][i].handle);
				debug_check();
				break;
			}
			case RenderOp::ColorMask:
			{
				glColorMask(*(sync->read<bool>()), *(sync->read<bool>()), *(sync->read<bool>()), *(sync->read<bool>()));
				debug_check();
				break;
			}
			case RenderOp::DepthMask:
			{
				glDepthMask(*(sync->read<bool>()));
				debug_check();
				break;
			}
			case RenderOp::DepthTest:
			{
				bool enable = *(sync->read<bool>());
				if (enable)
					glEnable(GL_DEPTH_TEST);
				else
					glDisable(GL_DEPTH_TEST);
				debug_check();
				break;
			}
			case RenderOp::Clear:
			{
				// Clear the screen
				GLbitfield clear_flags = 0;
				if (*(sync->read<bool>()))
					clear_flags |= GL_COLOR_BUFFER_BIT;
				if (*(sync->read<bool>()))
					clear_flags |= GL_DEPTH_BUFFER_BIT;
				glClear(clear_flags);
				debug_check();
				break;
			}
			case RenderOp::Shader:
			{
				AssetID shader_asset = *(sync->read<AssetID>());
				RenderTechnique technique = *(sync->read<RenderTechnique>());
				if (GLData::current_shader_asset != shader_asset || GLData::current_shader_technique != technique)
				{
					GLData::current_shader_asset = shader_asset;
					GLData::current_shader_technique = technique;
					GLData::samplers.length = 0;
					GLuint program_id = GLData::shaders[shader_asset][(int)technique].handle;
					glUseProgram(program_id);
					debug_check();
				}
				break;
			}
			case RenderOp::Uniform:
			{
				AssetID uniform_asset = *(sync->read<AssetID>());

				if (uniform_asset >= GLData::shaders[GLData::current_shader_asset][(int)GLData::current_shader_technique].uniforms.length)
				{
					int old_length = GLData::shaders[GLData::current_shader_asset][(int)GLData::current_shader_technique].uniforms.length;
					GLData::shaders[GLData::current_shader_asset][(int)GLData::current_shader_technique].uniforms.resize(uniform_asset + 1);
					for (int j = old_length; j < uniform_asset + 1; j++)
					{
						GLuint uniform = glGetUniformLocation(GLData::shaders[GLData::current_shader_asset][(int)GLData::current_shader_technique].handle, AssetLookup::Uniform::values[j]);
						GLData::shaders[GLData::current_shader_asset][(int)GLData::current_shader_technique].uniforms[j] = uniform;
					}
				}

				GLuint uniform_id = GLData::shaders[GLData::current_shader_asset][(int)GLData::current_shader_technique].uniforms[uniform_asset];
				RenderDataType uniform_type = *(sync->read<RenderDataType>());
				int uniform_count = *(sync->read<int>());
				switch (uniform_type)
				{
					case RenderDataType::Float:
					{
						const float* value = sync->read<float>(uniform_count);
						glUniform1fv(uniform_id, uniform_count, value);
						debug_check();
						break;
					}
					case RenderDataType::Vec2:
					{
						const float* value = (float*)sync->read<Vec2>(uniform_count);
						glUniform2fv(uniform_id, uniform_count, value);
						debug_check();
						break;
					}
					case RenderDataType::Vec3:
					{
						const float* value = (float*)sync->read<Vec3>(uniform_count);
						glUniform3fv(uniform_id, uniform_count, value);
						debug_check();
						break;
					}
					case RenderDataType::Vec4:
					{
						const float* value = (float*)sync->read<Vec4>(uniform_count);
						glUniform4fv(uniform_id, uniform_count, value);
						debug_check();
						break;
					}
					case RenderDataType::Int:
					{
						const int* value = sync->read<int>(uniform_count);
						glUniform1iv(uniform_id, uniform_count, value);
						debug_check();
						break;
					}
					case RenderDataType::Mat4:
					{
						float* value = (float*)sync->read<Mat4>(uniform_count);
						glUniformMatrix4fv(uniform_id, uniform_count, GL_FALSE, value);
						debug_check();
						break;
					}
					case RenderDataType::Texture:
					{
						vi_assert(uniform_count == 1); // Only single textures supported for now
						RenderTextureType texture_type = *(sync->read<RenderTextureType>());
						AssetID texture_asset = *(sync->read<AssetID>());
						GLuint texture_id;
						if (texture_asset == AssetNull)
							texture_id = 0;
						else
							texture_id = GLData::textures[texture_asset].handle;

						int sampler_index = -1;
						for (int i = 0; i < GLData::samplers.length; i++)
						{
							if (GLData::samplers[i] == texture_asset)
							{
								sampler_index = i;
								break;
							}
						}
						if (sampler_index == -1)
						{
							sampler_index = GLData::samplers.length;
							GLData::samplers.add(texture_asset);

							glActiveTexture(GL_TEXTURE0 + sampler_index);
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
							glUniform1i(uniform_id, sampler_index);
							debug_check();
						}
						break;
					}
					default:
					{
						vi_assert(false);
						break;
					}
				}
				debug_check();
				break;
			}
			case RenderOp::Mesh:
			{
				int id = *(sync->read<int>());
				GLData::Mesh* mesh = &GLData::meshes[id];

				glBindVertexArray(mesh->vertex_array);

				glDrawElements(
					GL_TRIANGLES,       // mode
					mesh->index_count,    // count
					GL_UNSIGNED_INT,    // type
					(void*)0            // element array buffer offset
				);

				debug_check();
				break;
			}
			case RenderOp::Instances:
			{
				int id = *(sync->read<int>());
				GLData::Mesh* mesh = &GLData::meshes[id];

				int count = *(sync->read<int>());

				glBindBuffer(GL_ARRAY_BUFFER, mesh->instance_buffer);
				glBufferData(GL_ARRAY_BUFFER, sizeof(Mat4) * count, sync->read<Mat4>(count), GL_DYNAMIC_DRAW);

				glBindVertexArray(mesh->instance_array);

				glDrawElementsInstanced(
					GL_TRIANGLES,       // mode
					mesh->index_count,    // count
					GL_UNSIGNED_INT,    // type
					(void*)0,            // element array buffer offset
					count
				);

				debug_check();
				break;
			}
			case RenderOp::BlendMode:
			{
				RenderBlendMode mode = *(sync->read<RenderBlendMode>());
				switch (mode)
				{
					case RenderBlendMode::Opaque:
						glDisablei(GL_BLEND, 0);
						break;
					case RenderBlendMode::Alpha:
						glEnablei(GL_BLEND, 0);
						glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
						break;
					case RenderBlendMode::Additive:
						glEnablei(GL_BLEND, 0);
						glBlendFunc(GL_SRC_ALPHA, GL_ONE);
						break;
					default:
						vi_assert(false);
						break;
				}
				debug_check();
				break;
			}
			case RenderOp::CullMode:
			{
				RenderCullMode mode = *(sync->read<RenderCullMode>());
				switch (mode)
				{
					case RenderCullMode::Back:
						glEnable(GL_CULL_FACE);
						glCullFace(GL_BACK);
						break;
					case RenderCullMode::Front:
						glEnable(GL_CULL_FACE);
						glCullFace(GL_FRONT);
						break;
					case RenderCullMode::None:
						glDisable(GL_CULL_FACE);
						break;
					default:
						vi_assert(false);
						break;
				}
				debug_check();
				break;
			}
			case RenderOp::FillMode:
			{
				RenderFillMode mode = *(sync->read<RenderFillMode>());
				switch (mode)
				{
					case RenderFillMode::Fill:
						glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
						break;
					case RenderFillMode::Line:
						glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
						break;
					case RenderFillMode::Point:
						glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
						break;
				}
				debug_check();
				break;
			}
			case RenderOp::PointSize:
			{
				float size = *(sync->read<float>());
				glPointSize(size);
				debug_check();
				break;
			}
			case RenderOp::AllocFramebuffer:
			{
				int id = *(sync->read<int>());
				if (id >= GLData::framebuffers.length)
					GLData::framebuffers.resize(id + 1);

				glGenFramebuffers(1, &GLData::framebuffers[id]);
				glBindFramebuffer(GL_FRAMEBUFFER, GLData::framebuffers[id]);

				int attachments = *(sync->read<int>());

				GLenum color_buffers[4];

				int color_buffer_index = 0;
				for (int i = 0; i < attachments; i++)
				{
					RenderFramebufferAttachment attachment_type = *(sync->read<RenderFramebufferAttachment>());
					int texture_id = *(sync->read<int>());
					GLuint gl_texture_id = GLData::textures[texture_id].handle;

					GLenum gl_texture_type;
					switch (GLData::textures[texture_id].type)
					{
						case RenderDynamicTextureType::Color:
						case RenderDynamicTextureType::Depth:
							gl_texture_type = GL_TEXTURE_2D;
							break;
						case RenderDynamicTextureType::ColorMultisample:
							gl_texture_type = GL_TEXTURE_2D_MULTISAMPLE;
							break;
						default:
							vi_assert(false);
							break;
					}

					switch (attachment_type)
					{
						case RenderFramebufferAttachment::Color0:
							glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, gl_texture_type, gl_texture_id, 0);
							color_buffers[color_buffer_index] = GL_COLOR_ATTACHMENT0;
							color_buffer_index++;
							break;
						case RenderFramebufferAttachment::Color1:
							glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, gl_texture_type, gl_texture_id, 0);
							color_buffers[color_buffer_index] = GL_COLOR_ATTACHMENT1;
							color_buffer_index++;
							break;
						case RenderFramebufferAttachment::Color2:
							glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, gl_texture_type, gl_texture_id, 0);
							color_buffers[color_buffer_index] = GL_COLOR_ATTACHMENT2;
							color_buffer_index++;
							break;
						case RenderFramebufferAttachment::Color3:
							glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, gl_texture_type, gl_texture_id, 0);
							color_buffers[color_buffer_index] = GL_COLOR_ATTACHMENT3;
							color_buffer_index++;
							break;
						case RenderFramebufferAttachment::Depth:
							glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, gl_texture_type, gl_texture_id, 0);
							break;
						default:
							vi_assert(false);
							break;
					}
				}

				vi_assert(color_buffer_index <= 4);
				glDrawBuffers(color_buffer_index, color_buffers);

				GLenum framebuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
				vi_assert(framebuffer_status == GL_FRAMEBUFFER_COMPLETE);

				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				debug_check();
				break;
			}
			case RenderOp::BindFramebuffer:
			{
				int id = *(sync->read<int>());
				if (id == AssetNull)
					glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
				else
					glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GLData::framebuffers[id]);
				debug_check();
				break;
			}
			case RenderOp::FreeFramebuffer:
			{
				int id = *(sync->read<int>());
				glDeleteFramebuffers(1, &GLData::framebuffers[id]);
				debug_check();
				break;
			}
			case RenderOp::BlitFramebuffer:
			{
				int id = *(sync->read<int>());
				glBindFramebuffer(GL_READ_FRAMEBUFFER, GLData::framebuffers[id]);
				const ScreenRect* src = sync->read<ScreenRect>();
				const ScreenRect* dst = sync->read<ScreenRect>();
				glBlitFramebuffer(src->x, src->y, src->x + src->width, src->y + src->height, dst->x, dst->y, dst->x + dst->width, dst->y + dst->height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
				debug_check();
				break;
			};
			default:
			{
				vi_assert(false);
				break;
			}
		}
	}
}


}
