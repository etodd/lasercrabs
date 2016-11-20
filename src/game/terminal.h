#pragma once
#include "types.h"

struct cJSON;

namespace VI
{

struct EntityFinder;
struct RenderParams;
struct Camera;

namespace Terminal
{

void init(cJSON*);
void update(const Update&);
void draw_opaque(const RenderParams&);
void draw_hollow(const RenderParams&);
void draw_override(const RenderParams&);
void draw_ui(const RenderParams&);
void show(Camera*);
void conversation_finished();
void clear();
void execute(const char*);
b8 active();

}

}
