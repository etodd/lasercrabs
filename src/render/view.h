#pragma once

#include "data/entity.h"
#include "data/components.h"
#include "exec.h"
#include "render.h"

struct View : public ComponentType<View>, ExecStatic<View, RenderParams*>
{
	AssetID mesh;
	AssetID shader;
	AssetID texture;
	void exec(RenderParams*);
	void awake();
	View()
		: mesh(), shader(), texture()
	{
	}
	~View();
};

struct ViewSys : View::System, ExecSystemStatic<View, RenderParams*>
{
	ViewSys();
};
