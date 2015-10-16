#ifdef VERTEX

layout(location = 0) in vec3 in_position;

uniform mat4 mvp;

out vec4 clip_position;

void main()
{
	gl_Position = clip_position = mvp * vec4(in_position, 1);
}

#else

// Interpolated values from the vertex shaders
in vec4 clip_position;
out vec4 out_color;

uniform vec2 uv_offset;
uniform vec2 uv_scale;
uniform sampler2D normal_buffer;
uniform sampler2D depth_buffer;
uniform sampler2D shadow_map;
uniform mat4 p;
uniform vec3 light_pos;
uniform float light_radius;
uniform vec3 light_color;
uniform float light_fov_dot;
uniform vec3 light_direction;
uniform mat4 light_vp;
uniform vec3 frustum[4];

vec3 lerp3(vec3 a, vec3 b, float w)
{
	return a + w * (b - a);
}

void main()
{
	vec2 original_uv = ((clip_position.xy / clip_position.w) * 0.5 + 0.5);
	vec2 uv = uv_offset + original_uv * uv_scale;

	vec3 view_ray_top = lerp3(frustum[0], frustum[1], original_uv.x);
	vec3 view_ray_bottom = lerp3(frustum[2], frustum[3], original_uv.x);
	vec3 view_ray = lerp3(view_ray_top, view_ray_bottom, original_uv.y);

	float clip_depth = texture(depth_buffer, uv).x * 2.0 - 1.0;
	float depth = p[3][2] / (clip_depth - p[2][2]);
	vec3 pos = view_ray * depth;
	vec3 to_light = light_pos - pos;

	vec4 light_projected = light_vp * vec4(pos, 1.0);
	vec2 light_clip = (light_projected.xy / light_projected.w) * 0.5 + 0.5;

	float shadow_depth = texture(shadow_map, light_clip).x;

	float distance_to_light = length(to_light);
	to_light /= distance_to_light;

	float light_dot = dot(to_light, light_direction);

	float light_strength =
		float(light_dot > light_fov_dot)
		* float(shadow_depth > ((light_projected.z - 0.00001) / light_projected.w))
		* max(0, 1.0 - (distance_to_light / light_radius))
		* max(0, dot(texture(normal_buffer, uv).xyz * 2.0 - 1.0, to_light));

	out_color = vec4(light_color * light_strength, 1);
}

#endif
