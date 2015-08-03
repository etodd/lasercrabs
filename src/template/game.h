#include "render/render.h"

namespace VI
{

struct Game
{
static void loop(RenderSync::Swapper*);
static void execute(const Update&, const char*);
};

}