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

const float gaussian_kernel[16] =
{
	0.003829872,
	0.0088129551,
	0.0181463396,
	0.03343381,
	0.0551230286,
	0.0813255467,
	0.1073650667,
	0.1268369298,
	0.1340827751,
	0.1268369298,
	0.1073650667,
	0.0813255467,
	0.0551230286,
	0.03343381,
	0.0181463396,
	0.0088129551,
};

uniform sampler2D color_buffer;
uniform sampler2D depth_buffer;
uniform vec2 inv_buffer_size;

const float blur_discard_threshold = 1;

void main()
{
	float depth = texture(depth_buffer, uv).x;

	float sum = 0.0;
	float count = 0;
	for (int i = -8; i < 8; i++)
	{
		vec2 tap = uv + (inv_buffer_size * i);
		if (abs(depth - texture(depth_buffer, tap).x) < blur_discard_threshold)
		{
			sum += texture(color_buffer, tap).x * gaussian_kernel[i + 8];
			count += gaussian_kernel[i + 8];
		}
	}
	
	sum /= count;
	gl_FragColor = vec4(sum, sum, sum, 1.0);
}

#endif
