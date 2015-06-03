#pragma once

#include "data/entity.h"
#include "exec.h"
#include "render.h"
#include "asset.h"

struct View : public ComponentType<View>, ExecStatic<View, RenderParams*>
{
	Asset::ID mesh;
	Asset::ID shader;
	Asset::ID texture;
	Transform* transform;
	void exec(RenderParams*);
	void awake(Entities*);
	View()
		: mesh(), shader(), texture()
	{
	}
};

struct ViewSys : View::System, ExecSystemStatic<View, RenderParams*>
{
	ViewSys(Entities*);
};
