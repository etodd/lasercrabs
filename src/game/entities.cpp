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
	
	RigidBody* body = Entities::all.component<RigidBody>(this, btMesh, btRigidBody::btRigidBodyConstructionInfo(0.0f, transform, btMesh, btVector3(0, 0, 0)));
	body->set_kinematic();
}

void StaticGeom::awake()
{
}
