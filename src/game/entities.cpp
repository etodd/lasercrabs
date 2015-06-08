#include "entities.h"
#include "render/view.h"

StaticGeom::StaticGeom(Loader* loader, AssetID id)
{
	Transform* transform = Entities::all.component<Transform>(this);
	View* model = Entities::all.component<View>(this);

	model->mesh = loader->mesh(id);
	model->shader = loader->shader(Asset::Shader::Standard);
	model->texture = loader->texture(Asset::Texture::test);

	Mesh* mesh = &loader->meshes[id].data;

	btBvhTriangleMeshShape* btMesh = new btBvhTriangleMeshShape(&mesh->physics, true, btVector3(-1000, -1000, -1000), btVector3(1000, 1000, 1000));
	btRigidBody::btRigidBodyConstructionInfo cInfo(0.0f, transform, btMesh, btVector3(0, 0, 0));
	RigidBody* body = Entities::all.component<RigidBody>(this, cInfo);
	body->btBody.setCollisionFlags(body->btBody.getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
	body->btBody.setActivationState(DISABLE_DEACTIVATION);
	Physics::world.btWorld->addRigidBody(&body->btBody);
}

void StaticGeom::awake()
{
	Entities::all.update.add(this);
}

void StaticGeom::exec(EntityUpdate up)
{
	Transform* transform = this->get<Transform>();
	transform->rot = transform->rot * Quat(up.time.delta * 0.1f, Vec3(0, 1, 0));
}

StaticGeom::~StaticGeom()
{
	printf("hey\n");
}
