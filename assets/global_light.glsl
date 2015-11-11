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

uniform sampler2D normal_buffer;
uniform sampler2D depth_buffer;
uniform mat4 p;

const int lights = 3;
uniform vec3 light_color[lights];
uniform vec3 light_direction[lights];
uniform bool shadowed;
uniform mat4 light_vp;
uniform sampler2D shadow_map;
uniform mat4 detail_light_vp;
uniform sampler2D detail_shadow_map;

out vec4 out_color;

void main()
{
	float clip_depth = texture(depth_buffer, uv).x;
	float clip_depth_scaled = clip_depth * 2.0 - 1.0;
	float depth = p[3][2] / (clip_depth_scaled - p[2][2]);
	vec3 view_pos = view_ray * depth;
	float view_distance = length(view_pos);
	
	vec3 normal = texture(normal_buffer, uv).xyz * 2.0 - 1.0;

	out_color = vec4(0, 0, 0, 1);

	if (shadowed)
	{
		vec3 full_light = light_color[0] * max(0, dot(normal, light_direction[0]));

		float shadow;

		vec4 detail_light_projected = detail_light_vp * vec4(view_pos, 1.0f);
		detail_light_projected.xy /= detail_light_projected.w;
		if (abs(detail_light_projected.x) < 1.0f && abs(detail_light_projected.y) < 1.0f)
		{
			vec2 light_clip = detail_light_projected.xy * 0.5f + 0.5f;
			float shadow_clip_depth = texture(detail_shadow_map, light_clip).x * 2.0 - 1.0;
			shadow = float(shadow_clip_depth > detail_light_projected.z - 0.001f);
		}
		else
		{
			vec4 light_projected = light_vp * vec4(view_pos, 1.0f);
			vec2 light_clip = (light_projected.xy / light_projected.w) * 0.5f + 0.5f;
			float shadow_clip_depth = texture(shadow_map, light_clip).x * 2.0 - 1.0;
			shadow = float(shadow_clip_depth > light_projected.z - 0.005f);
		}
		out_color.xyz += full_light * shadow;
	}

	for (int i = shadowed ? 1 : 0; i < lights; i++)
		out_color.xyz += light_color[i] * max(0, dot(normal, light_direction[i]));

	{
		// Player light
		float normal_attenuation = dot(normal, view_pos / -view_distance);
		float distance_attenuation = 1.0f - (view_distance / 10.0f);
		out_color.xyz += 0.5f * max(0, distance_attenuation) * max(0, normal_attenuation);
	}
}

#endif