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

// Interpolated values from the vertex shaders
in vec2 uv;

uniform sampler2D depth_buffer;
uniform sampler2D color_buffer;

void main()
{
	gl_FragColor = texture(color_buffer, uv);
	gl_FragDepth = texture(depth_buffer, uv).x;
}

#endif
