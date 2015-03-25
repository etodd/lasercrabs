#pragma once

#include <GL/glew.h>
#include "types.h"

enum RenderTechnique
{
	RenderTechnique_Default,
};

struct RenderParams
{
	Mat4 view;
	Mat4 projection;
	GLbitfield clear;
	RenderTechnique technique;
};
