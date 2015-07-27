#include "ui.h"

namespace VI
{

UIText::UIText()
	: indices(),
	vertices(),
	colors(),
	font(),
	string(),
	transform(Mat4::identity)
{
}

void UIText::text(const char* string)
{
}

void UIText::color(const Vec4& color)
{
	
}

void UIText::draw()
{
}

void UI::draw(const RenderParams& p)
{
}

}
