#pragma once

#include "data/entity.h"
#include "exec.h"
#include "render.h"
#include "data/mesh.h"

struct View : public Component<View>, ExecStatic<View, RenderParams*>
{
	Mesh::GL* data;
	static void bind(Mesh::GL*);
	static void unbind(Mesh::GL*);
	void exec(RenderParams*);
	void awake(Entities*);
};

struct ViewSys : View::System, ExecSystemStatic<View, RenderParams*>
{
	ViewSys(Entities*);
};
