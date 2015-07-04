#include "armature.h"

Armature::Armature()
	: mesh(), shader(), texture()
{
}

void Armature::draw(RenderParams* params)
{
	SyncData* sync = params->sync;

	sync->op(RenderOp_View);
	sync->write<AssetID>(&mesh);
	sync->write<AssetID>(&shader);
	sync->write<AssetID>(&texture);
	Mat4 m;
	get<Transform>()->mat(&m);
	Mat4 mvp = m * params->view * params->projection;
	sync->write<Mat4>(&mvp);
	sync->write<Mat4>(&m);
	sync->write<Mat4>(&params->view);
}

void Armature::awake()
{
}
