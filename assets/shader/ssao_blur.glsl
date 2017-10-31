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

const float gaussian_kernel[15] = float[15]
(
	0.000489,
	0.002403,
	0.009246,
	0.02784,
	0.065602,
	0.120999,
	0.174697,
	0.197448,
	0.174697,
	0.120999,
	0.065602,
	0.02784,
	0.009246,
	0.002403,
	0.000489
);

uniform sampler2D color_buffer;
uniform sampler2D depth_buffer;
uniform vec2 inv_buffer_size;

const float blur_discard_threshold = 1;

out vec4 out_color;

void main()
{
	float depth = texture(depth_buffer, uv).x;

	float sum = 0.0;
	for (int i = -7; i <= 7; i++)
	{
		vec2 tap = uv + (inv_buffer_size * i);
		if (abs(depth - texture(depth_buffer, tap).x) < blur_discard_threshold)
			sum += texture(color_buffer, tap).x * gaussian_kernel[i + 7];
	}
	
	out_color = vec4(sum, sum, sum, 1.0);
}

#endif
