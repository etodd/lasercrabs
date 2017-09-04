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
layout (location = 0) out vec4 out_color;

uniform sampler2D normal_buffer;
uniform vec2 inv_buffer_size;

void main()
{
	float a = texture(normal_buffer, uv).w;
	float b = texture(normal_buffer, uv + inv_buffer_size).w;
	out_color = vec4(0, 0, 0, 1.0f - min(a, b));
}

#endif