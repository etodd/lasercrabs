#ifdef VERTEX

layout(location = 0) in vec3 in_position;
layout(location = 2) in vec2 in_uv;

out vec2 uv;

void main()
{
	gl_Position = vec4(in_position, 1);
	uv = in_uv;
}

#else

in vec2 uv;

uniform vec2 buffer_size;
uniform float time;

out vec4 out_color;

void main()
{
	float value = float(int(time * 60 + uv.y * buffer_size.y) % 4 > 0);
	float add = value * 0.1;
	out_color = vec4(add, add, add, 0.8 + value * 0.2);
}

#endif
