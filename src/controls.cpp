// Include GLFW
#include <GLFW/glfw3.h>
extern GLFWwindow* window; // The "extern" keyword here is to access the variable "window" declared in tutorialXXX.cpp. This is a hack to keep the tutorials simple. Please avoid this.

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

void Controls::exec(GameTime t)
{
	// Get mouse position
	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);

	// Reset mouse position for next frame
	glfwSetCursorPos(window, 1024/2, 768/2);

	// Compute new orientation
	angle_horizontal += speed_mouse * float(1024/2 - xpos );
	angle_vertical   += speed_mouse * float( 768/2 - ypos );

	// Direction : Spherical coordinates to Cartesian coordinates conversion
	Vec3 direction(
		cos(angle_vertical) * sin(angle_horizontal), 
		sin(angle_vertical),
		cos(angle_vertical) * cos(angle_horizontal)
	);

	
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT))
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
	if (glfwGetKey( window, GLFW_KEY_W ) == GLFW_PRESS){
		position += direction * t.delta * speed;
	}
	// Move backward
	if (glfwGetKey( window, GLFW_KEY_S ) == GLFW_PRESS){
		position -= direction * t.delta * speed;
	}
	// Strafe right
	if (glfwGetKey( window, GLFW_KEY_D ) == GLFW_PRESS){
		position += right * t.delta * speed;
	}
	// Strafe left
	if (glfwGetKey( window, GLFW_KEY_A ) == GLFW_PRESS){
		position -= right * t.delta * speed;
	}

	float FoV = fov_initial;// - 5 * glfwGetMouseWheel(); // Now GLFW 3 requires setting up a callback for this. It's a bit too complicated for this beginner's tutorial, so it's disabled instead.

	// Projection matrix : 45° Field of View, 4:3 ratio, display range : 0.1 unit <-> 100 units
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	projection = glm::perspective(FoV, (float)width / (float)height, 0.1f, 1000.0f);
	// Camera matrix
	view       = glm::lookAt(
								position,           // Camera is here
								position+direction, // and looks here : at the same position, plus "direction"
								up                  // Head is up (set to 0,-1,0 to look upside-down)
						   );
}
