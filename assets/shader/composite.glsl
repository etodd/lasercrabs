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
uniform mat4 p;
uniform vec3 wall_normal;
uniform float range;
uniform vec3 range_center;

float bump_contrast(float x)
{
	return ((x - 0.5f) * 1.5f) + 0.5f;
}

out vec4 out_color;

void main()
{
	vec4 color = texture(color_buffer, uv);
	float clip_depth = texture(depth_buffer, uv).x;
	float clip_depth_scaled = clip_depth * 2.0 - 1.0;
	float depth = p[3][2] / (clip_depth_scaled - p[2][2]);

	vec4 lighting = texture(lighting_buffer, uv);
	lighting.rgb += ambient_color * texture(ssao_buffer, uv).x;
	vec3 lighting_color = color.rgb * lighting.rgb;
	vec3 final_color;
	if (range == 0.0f)
		final_color = lighting_color;
	else
	{
		vec3 pos = (view_ray * depth) - range_center;
		if (dot(pos, pos) < range * range)
			final_color = lighting_color * (dot(pos, wall_normal) > 0.0 ? 1.0 : 0.4);
		else
			final_color = vec3(0, 0, 0);
	}
	out_color = vec4(final_color, 1);
}

#endif
