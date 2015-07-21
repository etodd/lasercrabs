#include "entities.h"
#include "render/view.h"
#include "render/armature.h"
#include "walker.h"

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
	btBvhTriangleMeshShape* btMesh = new btBvhTriangleMeshShape(&mesh->physics, true, btVector3(-1000, -1000, -1000), btVector3(1000, 1000, 1000));
	
	RigidBody* body = create<RigidBody>(transform->pos, Quat::identity, 0.0f, btMesh);
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
	model->scale = scale;
	model->mesh = Asset::Model::cube;
	model->shader = Asset::Shader::Standard;
	model->texture = Asset::Texture::test;
	
	RigidBody* body = create<RigidBody>(pos, quat, mass, new btBoxShape(scale));
}

void Box::awake()
{
}

Sentinel::Sentinel(ID id, Vec3 pos, Quat quat)
	: Entity(id)
{
	Transform* transform = create<Transform>();
	transform->pos = pos;
	transform->rot = quat;

	Armature* armature = create<Armature>();

	armature->animation = Loader::animation(Asset::Animation::walk);

	armature->mesh = Asset::Model::Alpha;
	armature->shader = Asset::Shader::Armature;
	armature->texture = Asset::Texture::test;
	armature->scale = Vec3(0.01f, 0.01f, 0.01f);
	
	Walker* walker = create<Walker>(1.2f, 0.5f, 0.25f, 1.0f);
}

void Sentinel::awake()
{
}