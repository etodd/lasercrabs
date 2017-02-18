#ifdef VERTEX

layout(location = 0) in vec3 in_position;
layout(location = 2) in vec2 in_uv;

uniform mat4 mvp;
uniform vec2 cloud_uv_offset;
uniform float cloud_inv_uv_scale;

out vec2 uv;
out vec2 uv_raw;
out vec4 clip_position;

void main()
{
	gl_Position = mvp * vec4(in_position, 1);

	clip_position = gl_Position;

	uv_raw = (in_uv - 0.5f) * 2.0f;
	uv = cloud_uv_offset + in_uv * cloud_inv_uv_scale;
}

#else

vec3 lerp3(vec3 a, vec3 b, float w)
{
	return a + w * (b - a);
}

in vec2 uv_raw;
in vec2 uv;
in vec4 clip_position;

uniform vec3 frustum[4];
uniform vec4 diffuse_color;
uniform mat4 p;
uniform sampler2D cloud_map;
uniform sampler2D depth_buffer;
uniform float fog_start;
uniform float fog_extent;
uniform vec2 uv_offset;
uniform vec2 uv_scale;
uniform float cloud_height_diff_scaled;

out vec4 out_color;

void main()
{
	vec4 color = texture(cloud_map, uv) * diffuse_color;
	color.a *= 1.0f - length(vec3(uv_raw.x, cloud_height_diff_scaled, uv_raw.y));

	vec2 original_uv = ((clip_position.xy / clip_position.w) * 0.5 + 0.5);
	vec2 screen_uv = uv_offset + original_uv * uv_scale;
	float clip_depth = texture(depth_buffer, screen_uv).x * 2.0 - 1.0;
	float depth = p[3][2] / (clip_depth - p[2][2]);

	vec3 view_ray_top = lerp3(frustum[0], frustum[1], original_uv.x);
	vec3 view_ray_bottom = lerp3(frustum[2], frustum[3], original_uv.x);
	vec3 view_ray = lerp3(view_ray_top, view_ray_bottom, original_uv.y);

	float view_ray_length = length(view_ray);
	float final_depth = view_ray_length * depth;

	if (final_depth < fog_start)
		discard;

	out_color = vec4(color.rgb, color.a * (final_depth - fog_start) / fog_extent);
}

#endif