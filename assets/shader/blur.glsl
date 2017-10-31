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

const float gaussian_kernel15[15] = float[15]
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

const float gaussian_kernel31[31] = float[31]
(
	0.0009,
	0.001604,
	0.002748,
	0.004523,
	0.007154,
	0.010873,
	0.01588,
	0.022285,
	0.030051,
	0.038941,
	0.048488,
	0.058016,
	0.066703,
	0.073694,
	0.078235,
	0.07981,
	0.078235,
	0.073694,
	0.066703,
	0.058016,
	0.048488,
	0.038941,
	0.030051,
	0.022285,
	0.01588,
	0.010873,
	0.007154,
	0.004523,
	0.002748,
	0.001604,
	0.0009
);

uniform sampler2D color_buffer;
uniform vec2 inv_buffer_size;
uniform int radius;

out vec4 out_color;

void main()
{
	vec3 sum = vec3(0);
	for (int i = -radius; i <= radius; i++)
	{
		vec2 tap = uv + (inv_buffer_size * i);
		float coefficient;
		if (radius > 7)
			coefficient = gaussian_kernel31[i + radius];
		else
			coefficient = gaussian_kernel15[i + radius];
		sum += texture(color_buffer, tap).rgb * coefficient;
	}
	
	out_color = vec4(sum, 1.0);
}

#endif
