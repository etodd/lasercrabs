#ifndef RENDER_H
#define RENDER_H

#include <GL/glew.h>
#include <glm/glm.hpp>

enum RenderTechnique
{
	RenderTechnique_Default,
};

struct RenderParams
{
	glm::mat4 view;
	glm::mat4 projection;
	GLbitfield clear;
	RenderTechnique technique;
};

#endif
