#include "types.h"
#include "vi_assert.h"

#include "render/view.h"
#include "render/render.h"
#include "data/array.h"
#include "data/entity.h"
#include "data/components.h"
#include "data/mesh.h"
#include "exec.h"
#include "physics.h"
#include "asset.h"
#include "game/entities.h"
#include "game/player.h"

void game_loop(RenderSync::Swapper* swapper)
{
	Loader::main.swapper = swapper;

	ExecSystemDynamic<Update> update;
	ExecSystemDynamic<RenderParams*> draw;

	update.add(&Entities::main);
	update.add(&Physics::main);
	draw.add(&Entities::main.draw);

	StaticGeom* a = Entities::main.create<StaticGeom>(Asset::Model::city3);

	Player* player = Entities::main.create<Player>();

	RenderParams render_params;

	SyncData* sync = swapper->get();

	Update u;

	while (!sync->quit)
	{
		u.input = &sync->input;
		u.time = sync->time;
		update.exec(u);

		render_params.sync = sync;

		render_params.projection = player->get<PlayerControl>()->projection;
		render_params.view = player->get<PlayerControl>()->view;

		sync->op(RenderOp_Clear);
		
		GLbitfield clear_flags = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
		sync->write<GLbitfield>(&clear_flags);

		draw.exec(&render_params);

		sync = swapper->swap<SwapType_Write>();
		sync->queue.length = 0;
	}
}
