#pragma once
#include "types.h"

namespace VI
{

struct EntityFinder;
struct RenderParams;


namespace Terminal
{

void init(const Update&, const EntityFinder&);
void update(const Update&);
void draw(const RenderParams&);
void show();

}

}
