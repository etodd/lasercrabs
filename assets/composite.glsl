#ifdef VERTEX

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_ray;
layout(location = 2) in vec2 in_uv;

// Output data ; will be interpolated for each fragment.
out vec2 uv;
out vec3 view_ray;

void main()
{
	// Output position of the vertex, in clip space : MVP * position
	gl_Position = vec4(in_position, 1);

	uv = in_uv;

	view_ray = in_ray;
}

#else

// Interpolated values from the vertex shaders
in vec2 uv;
in vec3 view_ray;

uniform vec3 ambient_color;
uniform sampler2D color_buffer;
uniform sampler2D lighting_buffer;
uniform sampler2D depth_buffer;
uniform sampler2D ssao_buffer;
uniform vec2 uv_offset;
uniform vec2 buffer_size;
uniform mat4 p;

float rand(vec2 co)
{
	return fract(sin(dot(co.xy, vec2(12.9898,78.233))) * 43758.5453);
}

out vec4 out_color;

void main()
{
	vec4 color = texture(color_buffer, uv);
	vec3 final_color;
	if (color.a == 0.0)
		final_color = color.rgb;
	else
	{
		float clip_depth = texture(depth_buffer, uv).x;
		float clip_depth_scaled = clip_depth * 2.0 - 1.0;
		float depth = p[3][2] / (clip_depth_scaled - p[2][2]);

		vec4 lighting = texture(lighting_buffer, uv);
		lighting.rgb += ambient_color * texture(ssao_buffer, uv).x;
		vec3 lighting_color = color.rgb * lighting.rgb;
		vec3 pos = view_ray * depth;
		const vec3 luminance_weights = vec3(0.3333, 0.3333, 0.3333);
		const vec3 out_of_range_color = vec3(0.6, 0.2, 0.7);
		final_color = length(pos) < 25.0f ? lighting_color : out_of_range_color * (0.2 + dot(lighting_color, luminance_weights));
	}
	out_color = vec4(final_color + (rand(uv_offset + uv * buffer_size * 0.01) - 0.5) * 0.1, 1);
}

#endif
