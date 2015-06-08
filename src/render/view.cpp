#include "view.h"

void View::exec(RenderParams* params)
{
	SyncData* sync = params->sync;
	sync->op(RenderOp_View);
	sync->write<AssetID>(&mesh);
	sync->write<AssetID>(&shader);
	sync->write<AssetID>(&texture);

	Mat4 m;
	transform->mat(&m);
	Mat4 mvp = m * params->view * params->projection;
	sync->write<Mat4>(&mvp);
	sync->write<Mat4>(&m);
	sync->write<Mat4>(&params->view);
}

void View::awake()
{
	Entities::all.system<ViewSys>()->add(this);
	transform = entity->get<Transform>();
}

View::~View()
{
	Entities::all.system<ViewSys>()->remove(this);
}

ViewSys::ViewSys()
{
	Entities::all.draw.add(this);
}
