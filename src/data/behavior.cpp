#include "behavior.h"

namespace VI
{


void Sequence::run()
{
	index = 0;
	children[0]->run();
}

void Sequence::child_done(Behavior*)
{
	index++;
	if (index < num_children)
		children[index]->run();
	else if (parent)
		parent->child_done(this);
}

void Parallel::run()
{
	done = 0;
	for (s32 i = 0; i < num_children; i++)
		children[i]->run();
}

void Parallel::child_done(Behavior*)
{
	done++;
	if (done == num_children && parent)
		parent->child_done(this);
}

void Repeat::run()
{
	index = 0;
	repeat_index = 0;
	children[0]->run();
}

void Repeat::child_done(Behavior* child)
{
	index++;
	if (index < num_children)
		children[index]->run();
	else if (repeat_index < repeat_count || repeat_count < 0)
	{
		index = 0;
		repeat_index++;
		children[0]->run();
	}
	else if (parent)
		parent->child_done(this);
}


}