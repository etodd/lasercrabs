#include "view.h"
#include "load.h"

namespace VI
{

View::View()
	: mesh(), shader(), texture(), offset(Mat4::identity)
{
}

void View::draw(RenderParams* params)
{
	SyncData* sync = params->sync;
	sync->write(RenderOp_Mesh);
	sync->write(&mesh);
	sync->write(&shader);

	Mat4 m;
	get<Transform>()->mat(&m);
	m = offset * m;
	Mat4 mvp = m * params->view * params->projection;

	sync->write<int>(4); // Uniform count

	sync->write(Asset::Uniform::MVP);
	sync->write(RenderDataType_Mat4);
	sync->write<int>(1);
	sync->write(&mvp);

	sync->write(Asset::Uniform::M);
	sync->write(RenderDataType_Mat4);
	sync->write<int>(1);
	sync->write(&m);

	sync->write(Asset::Uniform::V);
	sync->write(RenderDataType_Mat4);
	sync->write<int>(1);
	sync->write(&params->view);

	sync->write(Asset::Uniform::myTextureSampler);
	sync->write(RenderDataType_Texture);
	sync->write<int>(1);
	sync->write(&texture);
	sync->write<GLenum>(GL_TEXTURE_2D);
}

void View::awake()
{
	Loader::mesh(mesh);
	Loader::shader(shader);
	Loader::texture(texture);
}

}
