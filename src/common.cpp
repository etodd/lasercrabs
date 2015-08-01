#include "common.h"
#include "render/view.h"
#include "render/armature.h"
#include <GLFW/glfw3.h>

#include <BulletCollision/CollisionShapes/btTriangleIndexVertexArray.h>

namespace VI
{

#define fov_initial PI * 0.25f
#define speed_mouse 0.0025f

Empty::Empty(ID id)
	: Entity(id)
{
}

void Empty::awake()
{
}

StaticGeom::StaticGeom(ID id, AssetID mesh_id)
	: Entity(id)
{
	Transform* transform = create<Transform>();
	View* model = create<View>();

	model->mesh = mesh_id;
	model->shader = Asset::Shader::Standard;
	model->texture = Asset::Texture::test;

	Mesh* mesh = Loader::mesh(model->mesh);

	btTriangleIndexVertexArray* mesh_data = new btTriangleIndexVertexArray(mesh->indices.length / 3, mesh->indices.data, 3 * sizeof(int), mesh->vertices.length, (btScalar*)mesh->vertices.data, sizeof(Vec3));
	btBvhTriangleMeshShape* shape = new btBvhTriangleMeshShape(mesh_data, true, btVector3(-1000, -1000, -1000), btVector3(1000, 1000, 1000));
	
	RigidBody* body = create<RigidBody>(transform->pos, Quat::identity, 0.0f, shape);
	body->btMesh = mesh_data;
}

void StaticGeom::awake()
{
}

Box::Box(ID id, Vec3 pos, Quat quat, float mass, Vec3 scale)
	: Entity(id)
{
	Transform* transform = create<Transform>();
	transform->pos = pos;
	transform->rot = quat;

	View* model = create<View>();
	model->offset = Mat4::make_scale(scale);
	model->mesh = Asset::Model::cube;
	model->shader = Asset::Shader::Standard;
	model->texture = Asset::Texture::test;
	
	RigidBody* body = create<RigidBody>(pos, quat, mass, new btBoxShape(scale));
}

void Box::awake()
{
}

Noclip::Noclip(ID id)
	: Entity(id)
{
	Transform* transform = create<Transform>();
	create<NoclipControl>();
}

void Noclip::awake()
{
}

NoclipControl::NoclipControl()
	: angle_horizontal(),
	angle_vertical()
{
}

void NoclipControl::awake()
{
}

void NoclipControl::update(const Update& u)
{
	angle_horizontal += speed_mouse * ((u.input->width / 2) - (float)u.input->cursor_x);
	angle_vertical -= speed_mouse * ((u.input->height / 2) - (float)u.input->cursor_y);

	if (angle_vertical < PI * -0.495f)
		angle_vertical = PI * -0.495f;
	if (angle_vertical > PI * 0.495f)
		angle_vertical = PI * 0.495f;

	Quat look_quat = Quat::euler(0, angle_horizontal, angle_vertical);

	const float speed = 8.0f;
	if (u.input->keys[GLFW_KEY_SPACE] == GLFW_PRESS)
		get<Transform>()->pos += Vec3(0, 1, 0) * u.time.delta * speed;
	if (u.input->keys[GLFW_KEY_LEFT_CONTROL] == GLFW_PRESS)
		get<Transform>()->pos += Vec3(0, -1, 0) * u.time.delta * speed;
	if (u.input->keys[GLFW_KEY_W] == GLFW_PRESS)
		get<Transform>()->pos += look_quat * Vec3(0, 0, 1) * u.time.delta * speed;
	if (u.input->keys[GLFW_KEY_S] == GLFW_PRESS)
		get<Transform>()->pos += look_quat * Vec3(0, 0, -1) * u.time.delta * speed;
	if (u.input->keys[GLFW_KEY_D] == GLFW_PRESS)
		get<Transform>()->pos += look_quat * Vec3(-1, 0, 0) * u.time.delta * speed;
	if (u.input->keys[GLFW_KEY_A] == GLFW_PRESS)
		get<Transform>()->pos += look_quat * Vec3(1, 0, 0) * u.time.delta * speed;
	
	float FoV = fov_initial;

	// Projection matrix : 45° Field of View, 4:3 ratio, display range : 0.1 unit <-> 100 units
	float aspect = u.input->height == 0 ? 1 : (float)u.input->width / (float)u.input->height;
	Camera::main.projection = Mat4::perspective(FoV, aspect, 0.01f, 1000.0f);

	// Camera matrix
	Vec3 pos = get<Transform>()->absolute_pos();
	Vec3 look = look_quat * Vec3(0, 0, 1);
	Camera::main.view = Mat4::look(pos, look);
}

}
