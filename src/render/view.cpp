#include "view.h"

void View::bind(Mesh::GL* data)
{
	for (int i = 0; i < data->attribs.length; i++)
	{
		glEnableVertexAttribArray(i);
		glBindBuffer(GL_ARRAY_BUFFER, data->attribs.data[i].buffer);
		glVertexAttribPointer(
			i,                                    // attribute
			data->attribs.data[i].element_size,   // size
			data->attribs.data[i].type,           // type
			GL_FALSE,                             // normalized?
			0,                                    // stride
			(void*)0                              // array buffer offset
		);
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data->index_buffer);
}

void View::unbind(Mesh::GL* data)
{
	for (int i = 0; i < data->attribs.length; i++)
		glDisableVertexAttribArray(i);
}

void View::exec(RenderParams* params)
{
	if (data)
	{
		View::bind(data);
		// Draw the triangles !
		glDrawElements(
			GL_TRIANGLES,       // mode
			data->index_count,  // count
			GL_UNSIGNED_INT,    // type
			(void*)0            // element array buffer offset
		);
		View::unbind(data);
	}
}

void View::awake(Entities* e)
{
	e->system<ViewSys>()->add(this);
}

ViewSys::ViewSys(Entities* e)
{
	e->draw.add(this);
}
