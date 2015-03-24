#ifndef CONTROLS_HPP
#define CONTROLS_HPP
#include "exec.hpp"
#include <glm/glm.hpp>
#include "physics.hpp"

class Controls : public Exec<float>
{
public:
	Controls();
	glm::mat4 view;
	glm::mat4 projection;

	glm::vec3 position; 
	float angle_horizontal;
	float angle_vertical;
	float fov_initial;

	float speed;
	float speed_mouse;

	void exec(float);

	btDiscreteDynamicsWorld* world;
};

#endif
