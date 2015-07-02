#include "types.h"
#include "vi_assert.h"

#include "render/view.h"
#include "render/render.h"
#include "data/array.h"
#include "data/entity.h"
#include "data/components.h"
#include "game/awk.h"
#include "game/player.h"
#include "data/mesh.h"
#include "exec.h"
#include "physics.h"
#include "game/entities.h"
#include "game/player.h"

void game_loop(RenderSync::Swapper* swapper)
{
	Loader::main.swapper = swapper;

	StaticGeom* a = Entities::main.create<StaticGeom>(Asset::Model::city3);

	Player* player = Entities::main.create<Player>();

	RenderParams render_params;

	SyncData* sync = swapper->get();

	Update u;

	while (!sync->quit)
	{
		u.input = &sync->input;
		u.time = sync->time;

		// Update
		{
			Physics::main.update(u);
			Entities::main.system<Awk::System>()->execute<Update, &Awk::update>(u);
			Entities::main.system<PlayerControl::System>()->execute<Update, &PlayerControl::update>(u);
		}

		render_params.sync = sync;

		render_params.projection = Camera::main.projection;
		render_params.view = Camera::main.view;

		sync->op(RenderOp_Clear);
		
		GLbitfield clear_flags = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
		sync->write<GLbitfield>(&clear_flags);

		// Draw
		{
			Entities::main.system<View::System>()->execute<RenderParams*, &View::draw>(&render_params);
		}

		sync = swapper->swap<SwapType_Write>();
		sync->queue.length = 0;
	}
}
