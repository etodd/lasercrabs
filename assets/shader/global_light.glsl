#ifdef VERTEX

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_ray;
layout(location = 2) in vec2 in_uv;

out vec2 uv;
out vec3 view_ray;

void main()
{
	gl_Position = vec4(in_position, 1);

	uv = in_uv;

	view_ray = in_ray;
}

#else

in vec2 uv;
in vec3 view_ray;

uniform sampler2D normal_buffer;
uniform sampler2D depth_buffer;
uniform mat4 p;

const int max_lights = 4;
uniform vec3 light_color[max_lights];
uniform vec3 light_direction[max_lights];
uniform mat4 v;

uniform bool tri_shadow_cascade;
uniform mat4 light_vp;
uniform sampler2DShadow shadow_map;
uniform mat4 detail_light_vp;
uniform sampler2DShadow detail_shadow_map;
uniform mat4 detail2_light_vp;
uniform sampler2DShadow detail2_shadow_map;

uniform sampler2D cloud_map;
uniform float camera_light_radius;
uniform float camera_light_strength;
uniform float far_plane;
uniform float range;
uniform vec3 range_center;
uniform vec2 cloud_uv_offset;
uniform float cloud_inv_uv_scale;
uniform float cloud_alpha;

out vec4 out_color;

void main()
{
	float clip_depth = texture(depth_buffer, uv).x;
	float clip_depth_scaled = clip_depth * 2.0 - 1.0;
	float depth = p[3][2] / (clip_depth_scaled - p[2][2]);
	vec3 view_pos = view_ray * depth;
	float view_distance = length(view_pos);
	
	vec3 normal = texture(normal_buffer, uv).xyz * 2.0 - 1.0;

	{
		// player light
		float normal_attenuation = dot(normal, view_pos / -view_distance);
		float distance_attenuation = 1.0f - view_distance / camera_light_radius;
		float light = max(0, distance_attenuation) * max(0, normal_attenuation);
		out_color = vec4(vec3(camera_light_strength * light), 1);
	}

#ifdef SHADOW
	{
		vec3 full_light = light_color[0] * max(0, dot(normal, light_direction[0]));

		float shadow = 0.0f;

		vec4 detail_light_projected = detail_light_vp * vec4(view_pos, 1.0f);
		detail_light_projected.xy /= detail_light_projected.w;
		if (abs(detail_light_projected.x) < 1.0f && abs(detail_light_projected.y) < 1.0f)
		{
			detail_light_projected.z -= 0.0005f;
			shadow = texture(detail_shadow_map, detail_light_projected.xyz * 0.5f + 0.5f);
		}
		else
		{
			bool detail2 = false;
			if (tri_shadow_cascade)
			{
				vec4 detail2_light_projected = detail2_light_vp * vec4(view_pos, 1.0f);
				detail2_light_projected.xy /= detail2_light_projected.w;
				if (abs(detail2_light_projected.x) < 1.0f && abs(detail2_light_projected.y) < 1.0f)
				{
					detail2_light_projected.z -= 0.001f;
					shadow = texture(detail2_shadow_map, detail2_light_projected.xyz * 0.5f + 0.5f);
					detail2 = true;
				}
			}

			if (!detail2)
			{
				vec4 light_projected = light_vp * vec4(view_pos, 1.0f);
				light_projected.xy /= light_projected.w;
				if (abs(light_projected.x) < 1.0f && abs(light_projected.y) < 1.0f)
				{
					light_projected.z -= 0.002f;
					shadow = texture(shadow_map, light_projected.xyz * 0.5f + 0.5f);
				}
				else
					shadow = 1.0f;
			}
		}

		if (cloud_alpha > 0.0f)
		{
			vec3 dir = (v * vec4(light_direction[0], 0)).xyz;
			vec3 world_pos = (v * vec4(view_pos, 1)).xyz;
			float t = -world_pos.y / dir.y;
			vec2 p = world_pos.xz + t * dir.xz;
			shadow = max(0, shadow - texture(cloud_map, p * cloud_inv_uv_scale + cloud_uv_offset).a * cloud_alpha);
		}

		out_color.xyz += full_light * shadow;
	}

	const int start_light_index = 1;
#else
	const int start_light_index = 0;
#endif

	for (int i = start_light_index; i < max_lights; i++)
		out_color.xyz += light_color[i] * max(0, dot(normal, light_direction[i]));
}

#endif