#include "common.h"
#include "render/views.h"
#include "input.h"
#include "console.h"
#include "data/components.h"
#include "data/animator.h"
#include "asset/shader.h"
#include "asset/mesh.h"

#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>

namespace VI
{

#define fov_initial PI * 0.25f
#define speed_mouse 0.0025f

Empty::Empty(ID id)
	: Entity(id)
{
	create<Transform>();
}

Prop::Prop(ID id, AssetID mesh_id)
	: Entity(id)
{
	Transform* transform = create<Transform>();
	View* model = create<View>();

	model->mesh = mesh_id;
	model->shader = Asset::Shader::Standard;
}

void StaticGeom::init(const AssetID mesh_id, btTriangleIndexVertexArray** mesh_data, btBvhTriangleMeshShape** shape)
{
	Transform* transform = create<Transform>();
	View* model = create<View>();

	model->mesh = mesh_id;
	model->shader = Asset::Shader::Standard;

	Mesh* mesh = Loader::mesh(model->mesh);

	*mesh_data = new btTriangleIndexVertexArray(mesh->indices.length / 3, mesh->indices.data, 3 * sizeof(int), mesh->vertices.length, (btScalar*)mesh->vertices.data, sizeof(Vec3));
	*shape = new btBvhTriangleMeshShape(*mesh_data, true, mesh->bounds_min, mesh->bounds_max);
	(*shape)->setUserIndex(model->mesh);
}

StaticGeom::StaticGeom(const ID id, const AssetID mesh_id, const Vec3& absolute_pos, const Quat& absolute_rot)
	: Entity(id)
{
	btTriangleIndexVertexArray* mesh_data;
	btBvhTriangleMeshShape* shape;
	init(mesh_id, &mesh_data, &shape);
	get<Transform>()->absolute(absolute_pos, absolute_rot);
	RigidBody* body = create<RigidBody>(absolute_pos, absolute_rot, 0.0f, shape);
	body->btMesh = mesh_data;
}

StaticGeom::StaticGeom(const ID id, const AssetID mesh_id, const Vec3& absolute_pos, const Quat& absolute_rot, const short group, const short mask)
	: Entity(id)
{
	btTriangleIndexVertexArray* mesh_data;
	btBvhTriangleMeshShape* shape;
	init(mesh_id, &mesh_data, &shape);
	get<Transform>()->absolute(absolute_pos, absolute_rot);
	RigidBody* body = create<RigidBody>(absolute_pos, absolute_rot, 0.0f, shape, group, mask);
	body->btMesh = mesh_data;
}

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

Noclip::Noclip(ID id)
	: Entity(id)
{
	Transform* transform = create<Transform>();
	create<NoclipControl>();
}

NoclipControl::NoclipControl()
	: angle_horizontal(),
	angle_vertical()
{
	camera = Camera::add();
}

NoclipControl::~NoclipControl()
{
	camera->remove();
}

void NoclipControl::update(const Update& u)
{
	angle_horizontal -= speed_mouse * (float)u.input->cursor_x;
	angle_vertical += speed_mouse * (float)u.input->cursor_y;

	if (angle_vertical < PI * -0.495f)
		angle_vertical = PI * -0.495f;
	if (angle_vertical > PI * 0.495f)
		angle_vertical = PI * 0.495f;

	Quat look_quat = Quat::euler(0, angle_horizontal, angle_vertical);

	if (!Console::visible)
	{
		float speed = u.input->keys[KEYCODE_LSHIFT] ? 16.0f : 8.0f;
		if (u.input->keys[KEYCODE_SPACE])
			get<Transform>()->pos += Vec3(0, 1, 0) * u.time.delta * speed;
		if (u.input->keys[KEYCODE_LCTRL])
			get<Transform>()->pos += Vec3(0, -1, 0) * u.time.delta * speed;
		if (u.input->keys[KEYCODE_W])
			get<Transform>()->pos += look_quat * Vec3(0, 0, 1) * u.time.delta * speed;
		if (u.input->keys[KEYCODE_S])
			get<Transform>()->pos += look_quat * Vec3(0, 0, -1) * u.time.delta * speed;
		if (u.input->keys[KEYCODE_D])
			get<Transform>()->pos += look_quat * Vec3(-1, 0, 0) * u.time.delta * speed;
		if (u.input->keys[KEYCODE_A])
			get<Transform>()->pos += look_quat * Vec3(1, 0, 0) * u.time.delta * speed;

		if ((u.input->mouse_buttons & 1) && (u.input->last_mouse_buttons | 1))
		{
			Entity* box = World::create<Box>(get<Transform>()->absolute_pos() + look_quat * Vec3(0, 0, 0.25f), look_quat, 1.0f, Vec3(0.1f, 0.2f, 0.1f));
			box->get<RigidBody>()->btBody->setLinearVelocity(look_quat * Vec3(0, 0, 15));
		}
	}
	
	float FoV = fov_initial;

	camera->viewport = { 0, 0, u.input->width, u.input->height };
	float aspect = camera->viewport.height == 0 ? 1 : (float)camera->viewport.width / (float)camera->viewport.height;
	camera->projection = Mat4::perspective(FoV, aspect, 0.01f, 1000.0f);

	// Camera matrix
	Vec3 pos = get<Transform>()->absolute_pos();
	Vec3 look = look_quat * Vec3(0, 0, 1);
	camera->pos = pos;
	camera->rot = look_quat;
}

void Debug::draw(const RenderParams& params)
{
	Vec2 pos;
	if (UI::project(params, get<Transform>()->absolute_pos(), pos))
		UI::centered_box(params, pos, Vec2(4, 4) * UI::scale, Vec4(1, 1, 1, 1), 0);
}

}
