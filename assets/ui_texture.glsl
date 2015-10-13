#ifdef VERTEX

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_uv;

// Output data ; will be interpolated for each fragment.
out vec4 color;
out vec2 uv;

void main()
{
	// Output position of the vertex, in clip space : MVP * position
	gl_Position = vec4(in_position, 1);

	color = in_color;
	uv = in_uv;
}

#else

// Interpolated values from the vertex shaders
in vec4 color;
in vec2 uv;
out vec4 out_color;

uniform sampler2D diffuse_map;

void main()
{
	out_color = texture(diffuse_map, uv) * color;
}

#endif
