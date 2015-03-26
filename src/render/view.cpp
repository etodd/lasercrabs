#include "view.h"

void View::exec(RenderParams* params)
{
	SyncData* sync = params->sync;
	sync->op(RenderOp_View);
	sync->send<Asset::ID>(&mesh);
	sync->send<Asset::ID>(&shader);
	sync->send<Asset::ID>(&texture);

	Mat4 ModelMatrix = Mat4(1.0);
	Mat4 MVP = params->projection * params->view * ModelMatrix;
	sync->send<Mat4>(&MVP);
	sync->send<Mat4>(&ModelMatrix);
	sync->send<Mat4>(&params->view);
}

void View::awake(Entities* e)
{
	e->system<ViewSys>()->add(this);
}

ViewSys::ViewSys(Entities* e)
{
	e->draw.add(this);
}
