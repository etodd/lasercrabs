#include "entities.h"
#include "render/view.h"
#include "render/armature.h"

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
	
	RigidBody* body = create<RigidBody>(0.0f, transform, btMesh);
	body->set_kinematic();
}

void StaticGeom::awake()
{
}

Prop::Prop(ID id, AssetID mesh_id, AssetID anim_id)
	: Entity(id)
{
	Transform* transform = create<Transform>();

	Armature* armature = create<Armature>();

	armature->animation = Loader::animation(anim_id);

	armature->mesh = mesh_id;
	armature->shader = Asset::Shader::Armature;
	armature->texture = Asset::Texture::test;
}

void Prop::awake()
{
}