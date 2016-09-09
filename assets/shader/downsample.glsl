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
out vec4 out_color;

uniform sampler2D color_buffer;
uniform vec2 inv_buffer_size;

void main()
{
	vec4 color0 = texture(color_buffer, uv + vec2(inv_buffer_size.x * -0.5f, inv_buffer_size.y * -0.5f));
	vec4 color1 = texture(color_buffer, uv + vec2(inv_buffer_size.x * 0.5f, inv_buffer_size.y * -0.5f));
	vec4 color2 = texture(color_buffer, uv + vec2(inv_buffer_size.x * -0.5f, inv_buffer_size.y * 0.5f));
	vec4 color3 = texture(color_buffer, uv + vec2(inv_buffer_size.x * 0.5f, inv_buffer_size.y * 0.5f));
	out_color = (color0 + color1 + color2 + color3) * 0.25f;
}

#endif
