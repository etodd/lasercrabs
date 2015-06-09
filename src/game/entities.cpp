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
	
	RigidBody* body = Entities::all.component<RigidBody>(this, 0.0f, transform, btMesh);
	body->set_kinematic();
}

void StaticGeom::awake()
{
	Entities::all.update.add(this);
}

StaticGeom::~StaticGeom()
{
	Entities::all.update.remove(this);
}

void StaticGeom::exec(EntityUpdate up)
{
	get<Transform>()->rot = Quat::normalize(get<Transform>()->rot * Quat::euler(0, up.time.delta * 0.1f, 0));
}