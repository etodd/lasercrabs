#include "types.h"
#include "vi_assert.h"

#include <stdio.h>
#include <stdlib.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

GLFWwindow* window;

#include "model.h"
#include "controls.h"
#include "load.h"
#include "array.h"
#include "exec.h"
#include "physics.h"
#include "render.h"
#include "entity.h"

struct Transform : public Component<Transform>
{
	Vec3 pos;
	Quat rot;
};

struct Test : public Component<Test>
{
	bool is_this_real;
};

struct Empty : public Entity
{
	Empty(Entities* e, ID id)
		: Entity(id)
	{
		e->add<Transform>(this);
	}
};

struct TestEntity : public Entity
{
	TestEntity(Entities* e, ID id)
		: Entity(id)
	{
		fprintf(stderr, "test\n");
		e->add<Transform>(this);
		e->add<Test>(this);
	}
};

#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <BulletCollision/CollisionShapes/btTriangleIndexVertexArray.h>

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
	Entity* a = e.create<Empty>();
	fprintf(stderr, "A %d\n", a->id);
	fprintf(stderr, "A1 %d\n", a->get<Transform>()->id);
	e.remove(a);

	Entity* b = e.create<TestEntity>();
	fprintf(stderr, "B %d\n", b->id);
	fprintf(stderr, "B1 %d\n", b->get<Transform>()->id);
	fprintf(stderr, "B2 %d\n", b->get<Test>()->id);

	Entity* c = e.create<Empty>();
	fprintf(stderr, "C %d\n", c->id);
	fprintf(stderr, "C1 %d\n", c->get<Transform>()->id);
	fprintf(stderr, "C2 %d\n", c->get<Test>());

	Entity* d = e.create<Empty>();
	fprintf(stderr, "D %d\n", d->id);
	fprintf(stderr, "D1 %d\n", d->get<Transform>()->id);
	e.remove(d);

	e.remove(b);

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetCursorPos(window, 1024/2, 768/2);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

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

	// Create and compile our GLSL program from the shaders
	GLuint programID = load_shader("../shaders/StandardShading.vertexshader", "../shaders/StandardShading.fragmentshader");

	// Get a handle for our "MVP" uniform
	GLuint MatrixID = glGetUniformLocation(programID, "MVP");
	GLuint ViewMatrixID = glGetUniformLocation(programID, "V");
	GLuint ModelMatrixID = glGetUniformLocation(programID, "M");

	// Load the texture
	GLuint Texture = load_png("../assets/test.png");
	if (!Texture)
	{
		fprintf(stderr, "Error loading texture!");
		return -1;
	}
	
	// Get a handle for our "myTextureSampler" uniform
	GLuint TextureID  = glGetUniformLocation(programID, "myTextureSampler");

	Physics physics;

	// Read our .obj file
	Model::Data model_data;

	Model model;
	model.data = &model_data;

	btTriangleIndexVertexArray* meshData;
	btBvhTriangleMeshShape* mesh;
	{
		Array<int> indices;
		Array<Vec3> vertices;
		Array<Vec2> uvs;
		Array<Vec3> normals;
		load_mdl("../assets/city3.mdl", indices, vertices, uvs, normals);

		model_data.add_attrib<Vec3>(vertices, GL_FLOAT);
		model_data.add_attrib<Vec2>(uvs, GL_FLOAT);
		model_data.add_attrib<Vec3>(normals, GL_FLOAT);
		model_data.set_indices(indices);

		meshData = new btTriangleIndexVertexArray(indices.length / 3, indices.data, 3 * sizeof(int), vertices.length, (btScalar*)vertices.data, sizeof(Vec3));
		mesh = new btBvhTriangleMeshShape(meshData, true, btVector3(-1000,-1000,-1000), btVector3(1000,1000,1000));
	}

	btTransform startTransform;
	startTransform.setIdentity();
	btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
	btRigidBody::btRigidBodyConstructionInfo cInfo(0.0f, myMotionState, mesh, btVector3(0, 0, 0));
	btRigidBody* body = new btRigidBody(cInfo);
	body->setContactProcessingThreshold(BT_LARGE_FLOAT);
	physics.world->addRigidBody(body);

	// Get a handle for our "LightPosition" uniform
	glUseProgram(programID);
	GLuint LightID = glGetUniformLocation(programID, "LightPosition_worldspace");

	ExecSystem<GameTime> update;
	RenderParams render_params;
	ExecSystem<RenderParams*> draw;

	Controls controls;
	controls.world = physics.world;

	update.add(&controls);
	draw.add(&model);

	double lastTime = glfwGetTime();
	GameTime t;
	do
	{
		double currentTime = glfwGetTime();
		t.total = currentTime;
		t.delta = currentTime - lastTime;
		lastTime = currentTime;

		update.go(t);
		physics.world->stepSimulation(t.delta, 10);

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

		draw.go(&render_params);

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();

	} // Check if the ESC key was pressed or the window was closed
	while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS && !glfwWindowShouldClose(window));

	// Cleanup VBO and shader
	glDeleteProgram(programID);
	glDeleteTextures(1, &Texture);
	
	delete mesh;
	delete meshData;

	// Close OpenGL window and terminate GLFW
	glfwTerminate();

	return 0;
}
