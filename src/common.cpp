#include "common.h"
#include "render/views.h"
#include <GLFW/glfw3.h>
#include "console.h"
#include "data/animator.h"
#include "asset.h"

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

Prop::Prop(ID id, AssetID mesh_id)
	: Entity(id)
{
	Transform* transform = create<Transform>();
	View* model = create<View>();

	model->mesh = mesh_id;
	model->shader = Asset::Shader::Standard;
}

void Prop::awake() { }

void StaticGeom::init(AssetID mesh_id, btTriangleIndexVertexArray** mesh_data, btBvhTriangleMeshShape** shape)
{
	Transform* transform = create<Transform>();
	View* model = create<View>();

	model->mesh = mesh_id;
	model->shader = Asset::Shader::Standard;

	Mesh* mesh = Loader::mesh(model->mesh);

	*mesh_data = new btTriangleIndexVertexArray(mesh->indices.length / 3, mesh->indices.data, 3 * sizeof(int), mesh->vertices.length, (btScalar*)mesh->vertices.data, sizeof(Vec3));
	*shape = new btBvhTriangleMeshShape(*mesh_data, true, btVector3(-1000, -1000, -1000), btVector3(1000, 1000, 1000));
}

StaticGeom::StaticGeom(ID id, AssetID mesh_id)
	: Entity(id)
{
	btTriangleIndexVertexArray* mesh_data;
	btBvhTriangleMeshShape* shape;
	init(mesh_id, &mesh_data, &shape);
	RigidBody* body = create<RigidBody>(get<Transform>()->pos, Quat::identity, 0.0f, shape);
	body->btMesh = mesh_data;
}

StaticGeom::StaticGeom(ID id, AssetID mesh_id, short group, short mask)
	: Entity(id)
{
	btTriangleIndexVertexArray* mesh_data;
	btBvhTriangleMeshShape* shape;
	init(mesh_id, &mesh_data, &shape);
	RigidBody* body = create<RigidBody>(get<Transform>()->pos, Quat::identity, 0.0f, shape, group, mask);
	body->btMesh = mesh_data;
}

void StaticGeom::awake()
{
}

Level::Level(ID id, AssetID armature_id)
	: Entity(id)
{
	create<Transform>();
	Animator* animator = create<Animator>();
	animator->armature = armature_id;
}

void Level::awake() { }

Box::Box(ID id, Vec3 pos, Quat quat, float mass, Vec3 scale)
	: Entity(id)
{
	Transform* transform = create<Transform>();
	transform->pos = pos;
	transform->rot = quat;

	View* model = create<View>();
	model->offset = Mat4::make_scale(scale);
	model->mesh = Asset::Mesh::cube;
	model->shader = Asset::Shader::Standard;
	
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

	if (!Console::visible)
	{
		float speed = u.input->keys[GLFW_KEY_LEFT_SHIFT] == GLFW_PRESS ? 16.0f : 8.0f;
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

		if (u.input->mouse_buttons[1] == GLFW_PRESS && u.input->last_mouse_buttons[1] != GLFW_PRESS)
		{
			Entity* box = World::create<Box>(get<Transform>()->absolute_pos() + look_quat * Vec3(0, 0, 0.25f), look_quat, 1.0f, Vec3(0.1f, 0.2f, 0.1f));
			box->get<RigidBody>()->btBody->setLinearVelocity(look_quat * Vec3(0, 0, 15));
		}
	}
	
	float FoV = fov_initial;

	float aspect = u.input->height == 0 ? 1 : (float)u.input->width / (float)u.input->height;
	Camera::main.projection = Mat4::perspective(FoV, aspect, 0.01f, 1000.0f);

	// Camera matrix
	Vec3 pos = get<Transform>()->absolute_pos();
	Vec3 look = look_quat * Vec3(0, 0, 1);
	Camera::main.pos = pos;
	Camera::main.rot = look_quat;
}

}
