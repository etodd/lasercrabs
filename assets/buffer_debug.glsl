#ifdef VERTEX

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_ray;
layout(location = 2) in vec2 in_uv;

// Output data ; will be interpolated for each fragment.
out vec2 uv;

void main()
{
	// Output position of the vertex, in clip space : MVP * position
	gl_Position = vec4(in_position, 1);

	uv = in_uv;
}

#else

// Interpolated values from the vertex shaders
in vec2 uv;
out vec4 out_color;

uniform sampler2D color_buffer;

void main()
{
	out_color = texture(color_buffer, uv);
}

#endif
