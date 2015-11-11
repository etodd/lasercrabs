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

uniform sampler2D color_buffer;

void main()
{
/*
	float clip_depth = texture(color_buffer, uv).x * 2.0 - 1.0;
	float d = (-0.01f / (clip_depth - 1.0f) * (1.0f / 20.0f));
*/
	float d = texture(color_buffer, uv).x;
	out_color = vec4(d, d, d, 1) * color;
}

#endif
