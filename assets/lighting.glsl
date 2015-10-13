#ifdef VERTEX

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 in_position;
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

uniform sampler2D normal_buffer;
uniform sampler2D depth_buffer;
uniform mat4 p;

void main()
{
	float clip_depth = texture(depth_buffer, uv).x * 2.0 - 1.0;
	float depth = p[3][2] / (clip_depth - p[2][2]);
	out_color = texture(normal_buffer, uv) * (1.0 - (depth / 100.0));
}

#endif
