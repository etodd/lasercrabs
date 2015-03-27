#include "view.h"

void View::exec(RenderParams* params)
{
	SyncData* sync = params->sync;
	sync->op(RenderOp_View);
	sync->write<Asset::ID>(&mesh);
	sync->write<Asset::ID>(&shader);
	sync->write<Asset::ID>(&texture);

	Mat4 m;
	transform->mat(&m);
	Mat4 mvp = m * params->view * params->projection;
	sync->write<Mat4>(&mvp);
	sync->write<Mat4>(&m);
	sync->write<Mat4>(&params->view);
}

void View::awake(Entities* e)
{
	e->system<ViewSys>()->add(this);
	transform = entity->get<Transform>();
}

ViewSys::ViewSys(Entities* e)
{
	e->draw.add(this);
}
