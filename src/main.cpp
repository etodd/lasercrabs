#include "types.h"
#include "vi_assert.h"

#include <thread>

#include <stdio.h>
#include <stdlib.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

GLFWwindow* window;

#include "render/view.h"
#include "controls.h"
#include "load.h"
#include "data/array.h"
#include "data/entity.h"
#include "data/mesh.h"
#include "exec.h"
#include "physics.h"
#include "render/render.h"
#include "asset.h"

struct Empty : public Entity
{
	Empty(Entities* e)
	{
		e->add<Transform>(this);
	}
	void awake(Entities* e) {}
};

struct StaticGeom : public Entity
{
	StaticGeom(Entities* e)
	{
		e->add<Transform>(this);
		e->add<View>(this);
	}

	void awake(Entities* e)
	{
	}
};

#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>

void resize(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

int main()
{
	// Initialise GLFW
	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		return -1;
	}

	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	window = glfwCreateWindow( 1024, 768, "grepr", NULL, NULL);
	if (!window)
	{
		fprintf(stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Sorry.\n");
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, resize);

	// Initialize GLEW
	glewExperimental = true; // Needed for core profile
	if (glewInit() != GLEW_OK)
	{
		fprintf(stderr, "Failed to initialize GLEW\n");
		return -1;
	}

	Entities e;

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetCursorPos(window, 1024/2, 768/2);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// Dark blue background
	glClearColor(0.0f, 0.0f, 0.4f, 0.0f);

	// Enable depth test
	glEnable(GL_DEPTH_TEST);
	// Accept fragment if it closer to the camera than the former one
	glDepthFunc(GL_LESS); 

	// Cull triangles which normal is not towards the camera
	glEnable(GL_CULL_FACE);

	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);

	Loader loader;

	// Create and compile our GLSL program from the shaders
	GLuint programID = loader.shader(Asset::Shader::Standard);
	if (!programID)
	{
		fprintf(stderr, "Failed to load shader\n");
		return -1;
	}

	// Get a handle for our "MVP" uniform
	GLuint MatrixID = glGetUniformLocation(programID, "MVP");
	GLuint ViewMatrixID = glGetUniformLocation(programID, "V");
	GLuint ModelMatrixID = glGetUniformLocation(programID, "M");

	// Load the texture
	GLuint Texture = loader.texture(Asset::Texture::test);
	if (!Texture)
	{
		fprintf(stderr, "Error loading texture!");
		return -1;
	}
	
	// Get a handle for our "myTextureSampler" uniform
	GLuint TextureID  = glGetUniformLocation(programID, "myTextureSampler");

	Physics physics;

	// Read our .obj file

	btTriangleIndexVertexArray* btMeshData;
	btBvhTriangleMeshShape* btMesh;

	Mesh* mesh = loader.mesh(Asset::Model::city3);
	if (!mesh)
	{
		fprintf(stderr, "Error loading mesh!");
		return -1;
	}

	btMesh = new btBvhTriangleMeshShape(&mesh->physics, true, btVector3(-1000, -1000, -1000), btVector3(1000, 1000, 1000));

	btTransform startTransform;
	startTransform.setIdentity();
	btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
	btRigidBody::btRigidBodyConstructionInfo cInfo(0.0f, myMotionState, btMesh, btVector3(0, 0, 0));
	btRigidBody* body = new btRigidBody(cInfo);
	body->setContactProcessingThreshold(BT_LARGE_FLOAT);
	physics.world->addRigidBody(body);

	// Get a handle for our "LightPosition" uniform
	glUseProgram(programID);
	GLuint LightID = glGetUniformLocation(programID, "LightPosition_worldspace");

	ExecSystemDynamic<GameTime> update;
	RenderParams render_params;
	ExecSystemDynamic<RenderParams*> draw;

	Controls controls;
	controls.world = physics.world;

	update.add(&controls);
	update.add(&e);
	draw.add(&e.draw);

	StaticGeom* a = e.create<StaticGeom>();
	View* model = a->get<View>();
	model->data = &mesh->gl;

	double lastTime = glfwGetTime();

	GameTime time;
	while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS && !glfwWindowShouldClose(window))
	{
		double currentTime = glfwGetTime();
		time.total = currentTime;
		time.delta = currentTime - lastTime;
		lastTime = currentTime;

		update.exec(time);
		physics.world->stepSimulation(time.delta, 10);

		// Clear the screen
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Use our shader
		glUseProgram(programID);

		Mat4 ProjectionMatrix = controls.projection;
		Mat4 ViewMatrix = controls.view;
		Mat4 ModelMatrix = Mat4(1.0);
		Mat4 MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;

		// Send our transformation to the currently bound shader, 
		// in the "MVP" uniform
		glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
		glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
		glUniformMatrix4fv(ViewMatrixID, 1, GL_FALSE, &ViewMatrix[0][0]);

		Vec3 lightPos = Vec3(4,4,4);
		glUniform3f(LightID, lightPos.x, lightPos.y, lightPos.z);

		// Bind our texture in Texture Unit 0
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, Texture);
		// Set our "myTextureSampler" sampler to user Texture Unit 0
		glUniform1i(TextureID, 0);

		draw.exec(&render_params);

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();

	} // Check if the ESC key was pressed or the window was closed

	// Cleanup VBO and shader
	loader.unload_shader(Asset::Shader::Standard);
	loader.unload_texture(Asset::Texture::test);
	
	delete btMesh;
	delete myMotionState;
	physics.world->removeRigidBody(body);
	delete body;

	// Close OpenGL window and terminate GLFW
	glfwTerminate();

	return 0;
}
