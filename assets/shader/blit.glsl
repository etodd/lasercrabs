#ifdef VERTEX

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_uv;

out vec2 uv;

void main()
{
	gl_Position = vec4(in_position, 1);
	uv = in_uv;
}

#else

in vec2 uv;
out vec4 out_color;

uniform sampler2D color_buffer;

void main()
{
	out_color = texture(color_buffer, uv);
}

#endif
