#include "entity.h"
#include "vi_assert.h"

Family Entity::families = 0;
Family ComponentBase::families = 0;

Entities Entities::all = Entities();
