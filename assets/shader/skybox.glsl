#ifdef VERTEX

layout(location = 0) in vec3 in_position;
layout(location = 2) in vec2 in_uv;

uniform mat4 mvp;

out vec2 uv;
out vec4 clip_position;

void main()
{
	gl_Position = mvp * vec4(in_position, 1);

	clip_position = gl_Position;

	uv = in_uv;
}

#else

vec3 lerp3(vec3 a, vec3 b, float w)
{
	return a + w * (b - a);
}

in vec2 uv;
in vec4 clip_position;

uniform vec3 frustum[4];
uniform vec3 diffuse_color;
uniform mat4 p;
uniform sampler2D diffuse_map;
uniform sampler2D depth_buffer;
uniform sampler2D noise_sampler;
uniform float fog_start;
uniform float fog_extent;
uniform float far_plane;
uniform vec2 uv_offset;
uniform vec2 uv_scale;
uniform sampler2D shadow_map;
uniform mat4 light_vp;
uniform bool fog;

out vec4 out_color;

void main()
{
	vec3 color = texture(diffuse_map, uv).rgb * diffuse_color;

	vec2 original_uv = ((clip_position.xy / clip_position.w) * 0.5 + 0.5);
	vec2 screen_uv = uv_offset + original_uv * uv_scale;
	float clip_depth = texture(depth_buffer, screen_uv).x * 2.0 - 1.0;
	float depth = p[3][2] / (clip_depth - p[2][2]);

	vec3 view_ray_top = lerp3(frustum[0], frustum[1], original_uv.x);
	vec3 view_ray_bottom = lerp3(frustum[2], frustum[3], original_uv.x);
	vec3 view_ray = lerp3(view_ray_top, view_ray_bottom, original_uv.y);

	float view_ray_length = length(view_ray);
	float final_depth = view_ray_length * depth;

	if (fog)
	{
#ifdef SHADOW

		#define FOG_SHADOW_SAMPLES 16
		#define FOG_SHADOW_EXTINCTION 1.0f
		#define FOG_SHADOW_STRENGTH 0.6f

		float diff = final_depth - fog_start;
		if (diff < 0.0f)
			discard;
		float interval = diff / FOG_SHADOW_SAMPLES;

		float shadow_value = 0;
		vec3 s = view_ray * fog_start / view_ray_length;
		view_ray *= interval;

		float noise_strength = 1.0f;
		float last_value = 0.0f;
		for (int i = 0; i < FOG_SHADOW_SAMPLES; i++)
		{
			s += view_ray;
			noise_strength += interval;
			vec3 shadow_sample_pos = s + (texture(noise_sampler, original_uv * noise_strength).xyz - 0.5f) * noise_strength * 0.5f;
			vec4 shadow_projected = light_vp * vec4(shadow_sample_pos, 1.0f);
			shadow_projected.xy /= shadow_projected.w;

			float new_value;
			if (abs(shadow_projected.x) < 1.0f && abs(shadow_projected.y) < 1.0f)
				new_value = float(texture(shadow_map, shadow_projected.xy * 0.5f + 0.5f).x > shadow_projected.z * 0.5f + 0.5f);
			else
				new_value = 1.0f;

			shadow_value += (new_value + last_value) * 0.5f; // midpoint integration
			last_value = new_value;
		}
		color *= (1.0f - FOG_SHADOW_STRENGTH) + (shadow_value / FOG_SHADOW_SAMPLES) * FOG_SHADOW_STRENGTH;
#endif

		out_color = vec4(color, (final_depth - fog_start) / fog_extent);
	}
	else
	{
		out_color = vec4(color, final_depth < far_plane ? 0.0f : 1.0f);
	}
}

#endif
