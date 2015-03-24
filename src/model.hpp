#ifndef MODEL_H
#define MODEL_H

#include "exec.hpp"
#include "render.hpp"
#include "array.hpp"


class Model : public Exec<RenderParams*>
{
public:
	struct Attrib
	{
		int element_size;
		GLuint type;
		GLuint gl_buffer;
	};

	class Data
	{
	public:
		Data();
		~Data();
		void cleanup();
		Array<Attrib> attribs;
		GLuint gl_index_buffer;
		GLuint gl_vertex_array;
		int index_count;

		template<typename T> void add_attrib(Array<T>& data, GLuint type)
		{
			Attrib a;
			a.element_size = sizeof(T) / 4;
			a.type = type;
			glBindVertexArray(gl_vertex_array);
			glGenBuffers(1, &a.gl_buffer);
			glBindBuffer(GL_ARRAY_BUFFER, a.gl_buffer);
			glBufferData(GL_ARRAY_BUFFER, data.length * sizeof(T), data.d, GL_STATIC_DRAW);
			attribs.add(a);
		}

		void set_indices(Array<int>&);

		void bind();
		void unbind();
	};

	Data* data;
	void exec(RenderParams*);
};

#endif
