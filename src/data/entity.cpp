#include "entity.h"
#include "vi_assert.h"

Family Entity::families = 0;

void Transform::mat(Mat4* m)
{
	*m = Mat4(rot);
	m->translate(pos);
}

void Entity::exec(EntityUpdate up)
{
}
