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

uniform sampler2D depth_buffer;
uniform vec2 inv_buffer_size;

void main()
{
	gl_FragDepth = max
	(
		max(texture(depth_buffer, uv + inv_buffer_size * vec2(-1, 0)).x, texture(depth_buffer, uv + inv_buffer_size * vec2(1, 0)).x),
		max(texture(depth_buffer, uv + inv_buffer_size * vec2(0, -1)).x, texture(depth_buffer, uv + inv_buffer_size * vec2(0, 1)).x)
	) + 0.000001f;
	out_color = vec4(1, 1, 1, 1);
}

#endif