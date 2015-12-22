#include "common.h"
#include "render/views.h"
#include "input.h"
#include "console.h"
#include "data/components.h"
#include "data/animator.h"
#include "asset/shader.h"
#include "asset/mesh.h"
#include "asset/armature.h"
#include "render/skinned_model.h"
#include "load.h"

#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <btBulletDynamicsCommon.h>

namespace VI
{

#define fov_initial (PI * 0.25f)
#define speed_mouse 0.0025f

Empty::Empty()
{
	create<Transform>();
}

Prop::Prop(const AssetID mesh_id, const AssetID armature, const AssetID animation)
{
	Transform* transform = create<Transform>();
	if (armature == AssetNull)
	{
		View* model = create<View>();
		model->mesh = mesh_id;
		model->shader = Asset::Shader::standard;
	}
	else
	{
		SkinnedModel* model = create<SkinnedModel>();
		model->mesh = mesh_id;
		model->shader = Asset::Shader::armature;
		Animator* anim = create<Animator>();
		anim->armature = armature;
		anim->layers[0].animation = animation;
	}
}

StaticGeom::StaticGeom(AssetID mesh_id, const Vec3& absolute_pos, const Quat& absolute_rot, short group, short mask)
{
	Transform* transform = create<Transform>();
	View* model = create<View>();

	model->mesh = mesh_id;
	model->shader = Asset::Shader::standard;

	Mesh* mesh = Loader::mesh(model->mesh);

	get<Transform>()->absolute(absolute_pos, absolute_rot);
	RigidBody* body = create<RigidBody>(RigidBody::Type::Mesh, Vec3::zero, 0.0f, btBroadphaseProxy::StaticFilter | group, ~btBroadphaseProxy::StaticFilter & mask, mesh_id);
}

PhysicsEntity::PhysicsEntity(AssetID mesh, const Vec3& pos, const Quat& quat, RigidBody::Type type, const Vec3& scale, float mass, short filter_group, short filter_mask)
{
	Transform* transform = create<Transform>();
	transform->pos = pos;
	transform->rot = quat;

	if (mesh != AssetNull)
	{
		View* model = create<View>();
		model->offset = Mat4::make_scale(scale);
		model->mesh = mesh;
		model->shader = Asset::Shader::standard;
	}
	
	RigidBody* body = create<RigidBody>(type, scale, mass, filter_group, filter_mask, mesh);
}

Noclip::Noclip()
{
	Transform* transform = create<Transform>();
	create<NoclipControl>();
}

NoclipControl::NoclipControl()
	: angle_horizontal(),
	angle_vertical()
{
}

void NoclipControl::awake()
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
		float speed = u.input->keys[(int)KeyCode::LShift] ? 24.0f : 4.0f;
		if (u.input->keys[(int)KeyCode::Space])
			get<Transform>()->pos += Vec3(0, 1, 0) * u.time.delta * speed;
		if (u.input->keys[(int)KeyCode::LCtrl])
			get<Transform>()->pos += Vec3(0, -1, 0) * u.time.delta * speed;
		if (u.input->keys[(int)KeyCode::W])
			get<Transform>()->pos += look_quat * Vec3(0, 0, 1) * u.time.delta * speed;
		if (u.input->keys[(int)KeyCode::S])
			get<Transform>()->pos += look_quat * Vec3(0, 0, -1) * u.time.delta * speed;
		if (u.input->keys[(int)KeyCode::D])
			get<Transform>()->pos += look_quat * Vec3(-1, 0, 0) * u.time.delta * speed;
		if (u.input->keys[(int)KeyCode::A])
			get<Transform>()->pos += look_quat * Vec3(1, 0, 0) * u.time.delta * speed;

		if (u.input->keys[(int)KeyCode::MouseLeft] && !u.last_input->keys[(int)KeyCode::MouseLeft])
		{
			static const Vec3 scale = Vec3(0.1f, 0.2f, 0.1f);
			Entity* box = World::create<PhysicsEntity>(Asset::Mesh::cube, get<Transform>()->absolute_pos() + look_quat * Vec3(0, 0, 0.25f), look_quat, RigidBody::Type::Box, scale, 1.0f, btBroadphaseProxy::AllFilter, btBroadphaseProxy::AllFilter);
			box->get<RigidBody>()->btBody->setLinearVelocity(look_quat * Vec3(0, 0, 15));
		}
	}
	
	float FoV = fov_initial;

	camera->viewport =
	{
		Vec2(0, 0),
		Vec2(u.input->width, u.input->height),
	};
	float aspect = camera->viewport.size.y == 0 ? 1 : (float)camera->viewport.size.x / (float)camera->viewport.size.y;
	camera->perspective(FoV, aspect, 0.02f, Skybox::far_plane);

	// Camera matrix
	Vec3 pos = get<Transform>()->absolute_pos();
	Vec3 look = look_quat * Vec3(0, 0, 1);
	camera->pos = pos;
	camera->rot = look_quat;
}

}
