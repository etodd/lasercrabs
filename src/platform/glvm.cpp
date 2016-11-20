#include <glew/include/GL/glew.h>
#include <array>

#include "render/glvm.h"
#include "vi_assert.h"
#include "types.h"

namespace VI
{

b8 compile_shader(const char* prefix, const char* code, s32 code_length, u32* program_id, const char* path)
{
	b8 success = true;

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
	s32 msg_length;
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

struct GLData
{
	struct Mesh
	{
		struct Attrib
		{
			s32 total_element_size;
			s32 element_count;
			RenderDataType data_type;
			GLuint gl_type;
			GLuint handle;
		};

		Array<Attrib> attribs;
		GLuint index_buffer;
		GLuint edges_index_buffer;
		GLuint vertex_array;
		GLuint instance_array;
		GLuint instance_buffer;
		s32 index_count;
		s32 edges_index_count;
		b8 dynamic;
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
		s32 width;
		s32 height;
		RenderDynamicTextureType type;
		RenderTextureWrap wrap;
		RenderTextureFilter filter;
		RenderTextureCompare compare;
	};

	static Array<Texture> textures;
	static Array<Shader> shaders;
	static Array<Mesh> meshes;
	static Array<GLuint> framebuffers;
	static AssetID current_shader_asset;
	static RenderTechnique current_shader_technique;
	static Array<AssetID> samplers;

	static RenderColorMask color_mask;
	static b8 depth_mask;
	static b8 depth_test;
	static RenderCullMode cull_mode;
	static RenderFillMode fill_mode;
	static RenderBlendMode blend_mode;
	static r32 point_size;
	static r32 line_width;
	static AssetID current_framebuffer;
	static Rect2 viewport;

	static Array<char> uniform_name_buffer;
	static Array<AssetID> uniform_names;

	static const char* uniform_name(AssetID index)
	{
		AssetID buffer_index = GLData::uniform_names[index];
		return &GLData::uniform_name_buffer[buffer_index];
	}
};

Array<GLData::Texture> GLData::textures;
Array<GLData::Shader> GLData::shaders;
Array<GLData::Mesh> GLData::meshes;
Array<GLuint> GLData::framebuffers;
AssetID GLData::current_shader_asset = AssetNull;
RenderTechnique GLData::current_shader_technique = RenderTechnique::Default;
Array<AssetID> GLData::samplers;
Array<char> GLData::uniform_name_buffer;
Array<AssetID> GLData::uniform_names;
RenderColorMask GLData::color_mask = RENDER_COLOR_MASK_DEFAULT;
b8 GLData::depth_mask = true;
b8 GLData::depth_test = true;
RenderCullMode GLData::cull_mode = RenderCullMode::Back;
RenderFillMode GLData::fill_mode = RenderFillMode::Fill;
RenderBlendMode GLData::blend_mode = RenderBlendMode::Opaque;
r32 GLData::point_size = 1.0f;
r32 GLData::line_width = 1.0f;
AssetID GLData::current_framebuffer = 0;
Rect2 GLData::viewport = { Vec2::zero, Vec2::zero };

void render_init()
{
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); 
	glEnable(GL_CULL_FACE);
	glEnable(GL_LINE_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
}

void bind_attrib_pointers(Array<GLData::Mesh::Attrib>& attribs)
{
	for (s32 i = 0; i < attribs.length; i++)
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

void update_attrib_buffer(RenderSync* sync, const GLData::Mesh::Attrib* attrib, s32 count, b8 dynamic)
{
	glBindBuffer(GL_ARRAY_BUFFER, attrib->handle);

	GLenum usage = dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;

	switch (attrib->data_type)
	{
		case RenderDataType::R32:
		{
			glBufferData(GL_ARRAY_BUFFER, count * sizeof(r32) * attrib->element_count, sync->read<r32>(count * attrib->element_count), usage);
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
		case RenderDataType::S32:
		{
			glBufferData(GL_ARRAY_BUFFER, count * sizeof(s32) * attrib->element_count, sync->read<s32>(count * attrib->element_count), usage);
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

void update_attrib_sub_buffer(RenderSync* sync, const GLData::Mesh::Attrib* attrib, s32 offset, s32 count)
{
	glBindBuffer(GL_ARRAY_BUFFER, attrib->handle);

	switch (attrib->data_type)
	{
		case RenderDataType::R32:
		{
			glBufferSubData(GL_ARRAY_BUFFER, offset * sizeof(r32) * attrib->element_count, count * sizeof(r32) * attrib->element_count, sync->read<r32>(count * attrib->element_count));
			break;
		}
		case RenderDataType::Vec2:
		{
			glBufferSubData(GL_ARRAY_BUFFER, offset * sizeof(Vec2) * attrib->element_count, count * sizeof(Vec2) * attrib->element_count, sync->read<Vec2>(count * attrib->element_count));
			break;
		}
		case RenderDataType::Vec3:
		{
			glBufferSubData(GL_ARRAY_BUFFER, offset * sizeof(Vec3) * attrib->element_count, count * sizeof(Vec3) * attrib->element_count, sync->read<Vec3>(count * attrib->element_count));
			break;
		}
		case RenderDataType::Vec4:
		{
			glBufferSubData(GL_ARRAY_BUFFER, offset * sizeof(Vec4) * attrib->element_count, count * sizeof(Vec4) * attrib->element_count, sync->read<Vec4>(count * attrib->element_count));
			break;
		}
		case RenderDataType::S32:
		{
			glBufferSubData(GL_ARRAY_BUFFER, offset * sizeof(s32) * attrib->element_count, count * sizeof(s32) * attrib->element_count, sync->read<s32>(count * attrib->element_count));
			break;
		}
		case RenderDataType::Mat4:
		{
			glBufferSubData(GL_ARRAY_BUFFER, offset * sizeof(Mat4) * attrib->element_count, count * sizeof(Mat4) * attrib->element_count, sync->read<Mat4>(count * attrib->element_count));
			break;
		}
		default:
			vi_assert(false);
			break;
	}
}

GLenum gl_primitive_modes[] =
{
	GL_TRIANGLES, // RenderPrimitiveMode::Triangles
	GL_TRIANGLE_STRIP, // RenderPrimitiveMode::TriangleStrip
	GL_TRIANGLE_FAN, // RenderPrimitiveMode::TriangleFan
	GL_LINES, // RenderPrimitiveMode::Lines
	GL_LINE_STRIP, // RenderPrimitiveMode::LineStrip
	GL_LINE_LOOP, // RenderPrimitiveMode::LineLoop
	GL_POINTS, // RenderPrimitiveMode::Points
};

void render(RenderSync* sync)
{
	sync->read_pos = 0;
	while (sync->read_pos < sync->queue.length)
	{
#if DEBUG
		GLenum error;
#define debug_check() vi_assert((error = glGetError()) == GL_NO_ERROR)
#else
#define debug_check() {}
#endif

		RenderOp op = *(sync->read<RenderOp>());
		switch (op)
		{
			case RenderOp::AllocUniform:
			{
				AssetID id = *sync->read<AssetID>();
				s32 length = *sync->read<s32>();
				const char* name = sync->read<char>(length);

				if (id + 1 > GLData::uniform_names.length)
					GLData::uniform_names.resize(id + 1);

				s32 buffer_index = GLData::uniform_name_buffer.length;
				GLData::uniform_names[id] = buffer_index;
				GLData::uniform_name_buffer.resize(GLData::uniform_name_buffer.length + length + 1); // Extra character - null-terminated string
				memcpy(&GLData::uniform_name_buffer[buffer_index], name, length);
				break;
			}
			case RenderOp::Viewport:
			{
				GLData::viewport = *sync->read<Rect2>();
				glViewport(GLData::viewport.pos.x, GLData::viewport.pos.y, GLData::viewport.size.x, GLData::viewport.size.y);
				debug_check();
				break;
			}
			case RenderOp::AllocMesh:
			{
				AssetID id = *(sync->read<AssetID>());
				if (id >= GLData::meshes.length)
					GLData::meshes.resize(id + 1);
				GLData::Mesh* mesh = &GLData::meshes[id];
				new (mesh) GLData::Mesh();
				mesh->dynamic = sync->read<b8>();

				glGenVertexArrays(1, &mesh->vertex_array);
				glBindVertexArray(mesh->vertex_array);

				s32 attrib_count = *(sync->read<s32>());
				for (s32 i = 0; i < attrib_count; i++)
				{
					GLData::Mesh::Attrib a;
					glGenBuffers(1, &a.handle);
					a.data_type = *(sync->read<RenderDataType>());
					a.element_count = *(sync->read<s32>());

					switch (a.data_type)
					{
						case RenderDataType::S32:
							a.total_element_size = a.element_count;
							a.gl_type = GL_INT;
							break;
						case RenderDataType::R32:
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

				glGenBuffers(1, &mesh->edges_index_buffer);

				debug_check();
				break;
			}
			case RenderOp::AllocInstances:
			{
				AssetID id = *(sync->read<AssetID>());

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
				AssetID id = *(sync->read<AssetID>());
				GLData::Mesh* mesh = &GLData::meshes[id];
				glBindVertexArray(mesh->vertex_array);

				s32 count = *(sync->read<s32>());

				for (s32 i = 0; i < mesh->attribs.length; i++)
				{
					update_attrib_buffer(sync, &mesh->attribs[i], count, mesh->dynamic);
					debug_check();
				}
				break;
			}
			case RenderOp::UpdateAttribSubBuffers:
			{
				AssetID id = *(sync->read<AssetID>());
				GLData::Mesh* mesh = &GLData::meshes[id];
				glBindVertexArray(mesh->vertex_array);

				s32 offset = *(sync->read<s32>());
				s32 count = *(sync->read<s32>());

				for (s32 i = 0; i < mesh->attribs.length; i++)
				{
					update_attrib_sub_buffer(sync, &mesh->attribs[i], offset, count);
					debug_check();
				}
				break;
			}
			case RenderOp::UpdateAttribBuffer:
			{
				AssetID id = *(sync->read<AssetID>());
				GLData::Mesh* mesh = &GLData::meshes[id];
				glBindVertexArray(mesh->vertex_array);

				s32 attrib_index = *(sync->read<s32>());
				s32 count = *(sync->read<s32>());

				update_attrib_buffer(sync, &mesh->attribs[attrib_index], count, mesh->dynamic);
				debug_check();

				break;
			}
			case RenderOp::UpdateAttribSubBuffer:
			{
				AssetID id = *(sync->read<AssetID>());
				GLData::Mesh* mesh = &GLData::meshes[id];
				glBindVertexArray(mesh->vertex_array);

				s32 attrib_index = *(sync->read<s32>());
				s32 offset = *(sync->read<s32>());
				s32 count = *(sync->read<s32>());

				update_attrib_sub_buffer(sync, &mesh->attribs[attrib_index], offset, count);
				debug_check();

				break;
			}
			case RenderOp::UpdateIndexBuffer:
			{
				AssetID id = *(sync->read<AssetID>());
				GLData::Mesh* mesh = &GLData::meshes[id];
				s32 index_count = *(sync->read<s32>());
				const s32* indices = sync->read<s32>(index_count);

				glBindVertexArray(mesh->vertex_array);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->index_buffer);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(s32), indices, mesh->dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
				mesh->index_count = index_count;
				debug_check();
				break;
			}
			case RenderOp::UpdateEdgesIndexBuffer:
			{
				AssetID id = *(sync->read<AssetID>());
				GLData::Mesh* mesh = &GLData::meshes[id];
				s32 index_count = *(sync->read<s32>());
				const s32* indices = sync->read<s32>(index_count);

				glBindVertexArray(mesh->vertex_array);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->edges_index_buffer);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(s32), indices, mesh->dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
				mesh->edges_index_count = index_count;
				debug_check();
				break;
			}
			case RenderOp::FreeMesh:
			{
				AssetID id = *(sync->read<AssetID>());
				GLData::Mesh* mesh = &GLData::meshes[id];
				for (s32 i = 0; i < mesh->attribs.length; i++)
					glDeleteBuffers(1, &mesh->attribs.data[i].handle);
				glDeleteBuffers(1, &mesh->index_buffer);
				glDeleteBuffers(1, &mesh->edges_index_buffer);
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
				s32 width = *(sync->read<s32>());
				s32 height = *(sync->read<s32>());
				RenderDynamicTextureType type = *(sync->read<RenderDynamicTextureType>());
				RenderTextureWrap wrap = *(sync->read<RenderTextureWrap>());
				RenderTextureFilter filter = *(sync->read<RenderTextureFilter>());
				RenderTextureCompare compare = *(sync->read<RenderTextureCompare>());
				if (GLData::textures[id].width != width
					|| GLData::textures[id].height != height
					|| type != GLData::textures[id].type
					|| wrap != GLData::textures[id].wrap
					|| filter != GLData::textures[id].filter
					|| compare != GLData::textures[id].compare)
				{
					glBindTexture(type == RenderDynamicTextureType::ColorMultisample || type == RenderDynamicTextureType::DepthMultisample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, GLData::textures[id].handle);
					GLData::textures[id].width = width;
					GLData::textures[id].height = height;
					GLData::textures[id].type = type;
					GLData::textures[id].wrap = wrap;
					GLData::textures[id].filter = filter;
					GLData::textures[id].compare = compare;
					switch (type)
					{
						case RenderDynamicTextureType::ColorMultisample:
							glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 8, GL_RGBA8, width, height, false);
							break;
						case RenderDynamicTextureType::Color:
							glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
							break;
						case RenderDynamicTextureType::Depth:
							glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
							break;
						case RenderDynamicTextureType::DepthMultisample:
							glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 8, GL_DEPTH_COMPONENT, width, height, false);
							break;
						default:
							vi_assert(false);
							break;
					}
					
					switch (wrap)
					{
						case RenderTextureWrap::Clamp:
						{
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
							break;
						}
						case RenderTextureWrap::Repeat:
						{
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
							break;
						}
					}

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

					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, compare == RenderTextureCompare::RefToTexture ? GL_COMPARE_REF_TO_TEXTURE : GL_NONE);
				}
				debug_check();
				break;
			}
			case RenderOp::LoadTexture:
			{
				AssetID id = *(sync->read<AssetID>());
				RenderTextureWrap wrap = *(sync->read<RenderTextureWrap>());
				RenderTextureFilter filter = *(sync->read<RenderTextureFilter>());
				u32 width = *(sync->read<u32>());
				u32 height = *(sync->read<u32>());
				const u8* buffer = sync->read<u8>(4 * width * height);
				glBindTexture(GL_TEXTURE_2D, GLData::textures[id].handle);

				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

				switch (wrap)
				{
					case RenderTextureWrap::Clamp:
					{
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
						break;
					}
					case RenderTextureWrap::Repeat:
					{
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
						break;
					}
				}

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
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
						break;
					}
				}

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

				s32 code_length = *(sync->read<s32>());
				const char* code = sync->read<char>(code_length);

				for (s32 i = 0; i < (s32)RenderTechnique::count; i++)
				{
					b8 success = compile_shader(TechniquePrefixes::all[i], code, code_length, &GLData::shaders[id][i].handle);
					vi_assert(success);

					GLData::shaders[id][i].uniforms.resize(GLData::uniform_names.length);
					for (s32 j = 0; j < GLData::uniform_names.length; j++)
						GLData::shaders[id][i].uniforms[j] = glGetUniformLocation(GLData::shaders[id][i].handle, GLData::uniform_name(j));
				}

				debug_check();
				break;
			}
			case RenderOp::FreeShader:
			{
				AssetID id = *(sync->read<AssetID>());
				for (s32 i = 0; i < (s32)RenderTechnique::count; i++)
					glDeleteProgram(GLData::shaders[id][i].handle);
				debug_check();
				break;
			}
			case RenderOp::ColorMask:
			{
				GLData::color_mask = *sync->read<RenderColorMask>();
				glColorMask(GLData::color_mask & (1 << 0), GLData::color_mask & (1 << 1), GLData::color_mask & (1 << 2), GLData::color_mask & (1 << 3));
				debug_check();
				break;
			}
			case RenderOp::DepthMask:
			{
				glDepthMask(GLData::depth_mask = *(sync->read<b8>()));
				debug_check();
				break;
			}
			case RenderOp::DepthTest:
			{
				b8 enable = GLData::depth_test = *(sync->read<b8>());
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
				if (*(sync->read<b8>()))
					clear_flags |= GL_COLOR_BUFFER_BIT;
				if (*(sync->read<b8>()))
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
					GLuint program_id = GLData::shaders[shader_asset][(s32)technique].handle;
					glUseProgram(program_id);
					debug_check();
				}
				break;
			}
			case RenderOp::Uniform:
			{
				AssetID uniform_asset = *(sync->read<AssetID>());

				GLuint uniform_id = GLData::shaders[GLData::current_shader_asset][(s32)GLData::current_shader_technique].uniforms[uniform_asset];
				RenderDataType uniform_type = *(sync->read<RenderDataType>());
				s32 uniform_count = *(sync->read<s32>());
				switch (uniform_type)
				{
					case RenderDataType::R32:
					{
						const r32* value = sync->read<r32>(uniform_count);
						glUniform1fv(uniform_id, uniform_count, value);
						debug_check();
						break;
					}
					case RenderDataType::Vec2:
					{
						const r32* value = (r32*)sync->read<Vec2>(uniform_count);
						glUniform2fv(uniform_id, uniform_count, value);
						debug_check();
						break;
					}
					case RenderDataType::Vec3:
					{
						const r32* value = (r32*)sync->read<Vec3>(uniform_count);
						glUniform3fv(uniform_id, uniform_count, value);
						debug_check();
						break;
					}
					case RenderDataType::Vec4:
					{
						const r32* value = (r32*)sync->read<Vec4>(uniform_count);
						glUniform4fv(uniform_id, uniform_count, value);
						debug_check();
						break;
					}
					case RenderDataType::S32:
					{
						const s32* value = sync->read<s32>(uniform_count);
						glUniform1iv(uniform_id, uniform_count, value);
						debug_check();
						break;
					}
					case RenderDataType::Mat4:
					{
						r32* value = (r32*)sync->read<Mat4>(uniform_count);
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

						s32 sampler_index = -1;
						for (s32 i = 0; i < GLData::samplers.length; i++)
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
								case RenderTextureType::Texture2D:
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
				RenderPrimitiveMode primitive_mode = *(sync->read<RenderPrimitiveMode>());
				AssetID id = *(sync->read<AssetID>());

				GLData::Mesh* mesh = &GLData::meshes[id];

				glBindVertexArray(mesh->vertex_array);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->index_buffer);

				glDrawElements(
					gl_primitive_modes[s32(primitive_mode)], // mode
					mesh->index_count, // count
					GL_UNSIGNED_INT, // type
					0 // element array buffer offset
				);

				debug_check();
				break;
			}
			case RenderOp::MeshEdges:
			{
				AssetID id = *(sync->read<AssetID>());

				GLData::Mesh* mesh = &GLData::meshes[id];

				glBindVertexArray(mesh->vertex_array);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->edges_index_buffer);

				glDrawElements(
					GL_LINES, // RenderPrimitiveMode::Lines
					mesh->edges_index_count, // count
					GL_UNSIGNED_INT, // type
					0 // element array buffer offset
				);

				debug_check();
				break;
			}
			case RenderOp::SubMesh:
			{
				AssetID id = *(sync->read<AssetID>());
				GLData::Mesh* mesh = &GLData::meshes[id];

				s32 index_offset = *(sync->read<s32>());
				s32 index_count = *(sync->read<s32>());

				glBindVertexArray(mesh->vertex_array);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->index_buffer);

				glDrawElements(
					GL_TRIANGLES, // mode
					index_count, // count
					GL_UNSIGNED_INT, // type
					(void*)(index_offset * sizeof(u32)) // element array buffer offset
				);

				debug_check();
				break;
			}
			case RenderOp::Instances:
			{
				AssetID id = *(sync->read<AssetID>());
				GLData::Mesh* mesh = &GLData::meshes[id];

				s32 count = *(sync->read<s32>());

				glBindBuffer(GL_ARRAY_BUFFER, mesh->instance_buffer);
				glBufferData(GL_ARRAY_BUFFER, sizeof(Mat4) * count, sync->read<Mat4>(count), GL_DYNAMIC_DRAW);

				glBindVertexArray(mesh->instance_array);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->index_buffer);

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
				RenderBlendMode mode = GLData::blend_mode = *(sync->read<RenderBlendMode>());
				switch (mode)
				{
					case RenderBlendMode::Opaque:
					{
						glDisablei(GL_BLEND, 0);
						break;
					}
					case RenderBlendMode::Alpha:
					{
						glEnablei(GL_BLEND, 0);
						glBlendFunci(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
						break;
					}
					case RenderBlendMode::Additive:
					{
						glEnablei(GL_BLEND, 0);
						glBlendFunci(0, GL_SRC_ALPHA, GL_ONE);
						break;
					}
					case RenderBlendMode::AlphaDestination:
					{
						glEnablei(GL_BLEND, 0);
						glBlendFuncSeparatei(0, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_ZERO, GL_ONE);
						break;
					}
					case RenderBlendMode::Multiply:
					{
						glEnablei(GL_BLEND, 0);
						glBlendFunci(0, GL_ZERO, GL_SRC_ALPHA);
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
			case RenderOp::CullMode:
			{
				RenderCullMode mode = GLData::cull_mode = *(sync->read<RenderCullMode>());
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
				RenderFillMode mode = GLData::fill_mode = *(sync->read<RenderFillMode>());
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
				r32 size = GLData::point_size = *(sync->read<r32>());
				glPointSize(size);
				debug_check();
				break;
			}
			case RenderOp::LineWidth:
			{
				r32 size = GLData::line_width = *(sync->read<r32>());
				glLineWidth(size);
				debug_check();
				break;
			}
			case RenderOp::AllocFramebuffer:
			{
				AssetID id = *(sync->read<AssetID>());
				if (id >= GLData::framebuffers.length)
					GLData::framebuffers.resize(id + 1);

				glGenFramebuffers(1, &GLData::framebuffers[id]);
				glBindFramebuffer(GL_FRAMEBUFFER, GLData::framebuffers[id]);

				s32 attachments = *(sync->read<s32>());

				GLenum color_buffers[4];

				s32 color_buffer_index = 0;
				for (s32 i = 0; i < attachments; i++)
				{
					RenderFramebufferAttachment attachment_type = *(sync->read<RenderFramebufferAttachment>());
					AssetID texture_id = *(sync->read<AssetID>());
					GLuint gl_texture_id = GLData::textures[texture_id].handle;

					GLenum gl_texture_type;
					switch (GLData::textures[texture_id].type)
					{
						case RenderDynamicTextureType::Color:
						case RenderDynamicTextureType::Depth:
							gl_texture_type = GL_TEXTURE_2D;
							break;
						case RenderDynamicTextureType::ColorMultisample:
						case RenderDynamicTextureType::DepthMultisample:
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
				AssetID id = GLData::current_framebuffer = *(sync->read<AssetID>());
				if (id == AssetNull)
					glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
				else
					glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GLData::framebuffers[id]);
				debug_check();
				break;
			}
			case RenderOp::FreeFramebuffer:
			{
				AssetID id = *(sync->read<AssetID>());
				glDeleteFramebuffers(1, &GLData::framebuffers[id]);
				debug_check();
				break;
			}
			case RenderOp::BlitFramebuffer:
			{
				AssetID id = *(sync->read<AssetID>());
				glBindFramebuffer(GL_READ_FRAMEBUFFER, GLData::framebuffers[id]);
				const Rect2* src = sync->read<Rect2>();
				const Rect2* dst = sync->read<Rect2>();
				glBlitFramebuffer(src->pos.x, src->pos.y, src->pos.x + src->size.x, src->pos.y + src->size.y, dst->pos.x, dst->pos.y, dst->pos.x + dst->size.x, dst->pos.y + dst->size.y, GL_COLOR_BUFFER_BIT, GL_NEAREST);
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
