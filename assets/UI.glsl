#ifdef VERTEX

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;

// Output data ; will be interpolated for each fragment.
out vec4 color;

// Values that stay constant for the whole mesh.
uniform mat4 MVP;
uniform mat4 V;
uniform mat4 M;

void main()
{
	// Output position of the vertex, in clip space : MVP * position
	gl_Position = MVP * vec4(in_position, 1);

	color = in_color;
}

#else

// Interpolated values from the vertex shaders
in vec4 color;
out vec4 out_color;

void main()
{
	out_color = color;
}

#endif
