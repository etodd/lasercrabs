#include <GLFW/glfw3.h>

#include "types.h"
#include "controls.h"
#include <glm/gtc/matrix_transform.hpp>

Controls::Controls()
{
	position = Vec3(0, 0, 5); 
	angle_horizontal = 3.14f;
	angle_vertical = 0.0f;
	fov_initial = 40.0f;
	speed = 10.0f; // 3 units / second
	speed_mouse = 0.0025f;
}

void Controls::exec(Update u)
{
	// Compute new orientation
	angle_horizontal += speed_mouse * float(1024/2 - u.input->cursor_x );
	angle_vertical   += speed_mouse * float( 768/2 - u.input->cursor_y );

	// Direction : Spherical coordinates to Cartesian coordinates conversion
	Vec3 direction(
		cos(angle_vertical) * sin(angle_horizontal), 
		sin(angle_vertical),
		cos(angle_vertical) * cos(angle_horizontal)
	);

	
	if (u.input->mouse)
	{
		const float radius = 10000.0f;
		btVector3 rayStart(position.x, position.y, position.z);
		btVector3 rayEnd(position.x + direction.x * radius, position.y + direction.y * radius, position.z + direction.z * radius);
		btCollisionWorld::ClosestRayResultCallback rayCallback(rayStart, rayEnd);

		// Perform raycast
		world->rayTest(rayStart, rayEnd, rayCallback);

		if (rayCallback.hasHit())
		{
			rayEnd = rayCallback.m_hitPointWorld;
			position.x = rayEnd.x();
			position.y = rayEnd.y();
			position.z = rayEnd.z();
		}
	}
	
	// Right vector
	Vec3 right = Vec3(
		sin(angle_horizontal - 3.14f/2.0f), 
		0,
		cos(angle_horizontal - 3.14f/2.0f)
	);
	
	// Up vector
	Vec3 up = cross( right, direction );

	// Move forward
	if (u.input->keys[GLFW_KEY_W] == GLFW_PRESS)
		position += direction * u.time.delta * speed;
	// Move backward
	if (u.input->keys[GLFW_KEY_S] == GLFW_PRESS)
		position -= direction * u.time.delta * speed;
	// Strafe right
	if (u.input->keys[GLFW_KEY_D] == GLFW_PRESS)
		position += right * u.time.delta * speed;
	// Strafe left
	if (u.input->keys[GLFW_KEY_A] == GLFW_PRESS)
		position -= right * u.time.delta * speed;

	float FoV = fov_initial;

	// Projection matrix : 45° Field of View, 4:3 ratio, display range : 0.1 unit <-> 100 units
	float aspect = u.input->height == 0 ? 1 : (float)u.input->width / (float)u.input->height;
	projection = glm::perspective(FoV, aspect, 0.1f, 1000.0f);
	// Camera matrix
	view       = glm::lookAt(
								position,           // Camera is here
								position+direction, // and looks here : at the same position, plus "direction"
								up                  // Head is up (set to 0,-1,0 to look upside-down)
						   );
}
