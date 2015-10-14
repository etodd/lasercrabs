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
out vec4 out_color;

uniform sampler2D normal_buffer;
uniform sampler2D depth_buffer;
uniform sampler2D shadow_map;
uniform mat4 p;
uniform vec3 camera_pos;
uniform vec3 light_pos;
uniform float light_radius;
uniform vec3 light_color;
uniform float light_fov_dot;
uniform vec3 light_direction;
uniform mat4 light_vp;

void main()
{
	float clip_depth = texture(depth_buffer, uv).x * 2.0 - 1.0;
	float depth = p[3][2] / (clip_depth - p[2][2]);
	vec3 pos = camera_pos + (view_ray * depth);
	vec3 to_light = light_pos - pos;

	vec4 light_projected = light_vp * vec4(pos, 1.0);
	vec2 light_clip = (light_projected.xy / light_projected.w) * 0.5 + 0.5;

	float shadow_depth = texture(shadow_map, light_clip).x;

	float distance_to_light = length(to_light);
	to_light /= distance_to_light;

	float light_dot = dot(to_light, light_direction);

	float light_strength =
		float(light_dot > light_fov_dot)
		* float(shadow_depth > ((light_projected.z - 0.001) / light_projected.w))
		* float(distance_to_light < light_radius)
		* float(dot(texture(normal_buffer, uv).xyz * 2.0 - 1.0, to_light) > 0.0);

	out_color = vec4(light_color * light_strength, 1);
}

#endif
