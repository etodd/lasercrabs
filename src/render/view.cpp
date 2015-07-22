#include "view.h"
#include "load.h"

View::View()
	: mesh(), shader(), texture(), offset(Mat4::identity)
{
}

void View::draw(RenderParams* params)
{
	SyncData* sync = params->sync;
	sync->write(RenderOp_View);
	sync->write(&mesh);
	sync->write(&shader);
	sync->write(&texture);

	Mat4 m;
	get<Transform>()->mat(&m);
	m = offset * m;
	Mat4 mvp = m * params->view * params->projection;

	sync->write<int>(3); // Uniform count

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
}

void View::awake()
{
	Loader::mesh(mesh);
	Loader::shader(shader);
	Loader::texture(texture);
}
