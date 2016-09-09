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
uniform sampler2D normal_buffer;
uniform sampler2D depth_buffer;

layout (location = 0) out vec4 out_color;
layout (location = 1) out vec4 out_normal;

void main()
{
	out_color = texture(color_buffer, uv);
	out_normal = texture(normal_buffer, uv);
	gl_FragDepth = texture(depth_buffer, uv).x;
}

#endif
