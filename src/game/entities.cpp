#include "entities.h"
#include "render/view.h"

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

	model->mesh = Loader::mesh(mesh_id);
	model->shader = Loader::shader(Asset::Shader::Standard);
	model->texture = Loader::texture(Asset::Texture::test);

	Mesh* mesh = &Loader::meshes[mesh_id].data;

	btBvhTriangleMeshShape* btMesh = new btBvhTriangleMeshShape(&mesh->physics, true, btVector3(-1000, -1000, -1000), btVector3(1000, 1000, 1000));
	
	RigidBody* body = create<RigidBody>(0.0f, transform, btMesh);
	body->set_kinematic();
}

void StaticGeom::awake()
{
}