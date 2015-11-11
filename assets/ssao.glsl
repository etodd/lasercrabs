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
uniform vec2 inv_buffer_size;

uniform sampler2D noise_sampler;
uniform sampler2D normal_buffer;
uniform sampler2D depth_buffer;
uniform mat4 p;

const float noise_tile = 10.0;

uniform vec3 frustum[4];
uniform float far_plane;
uniform vec2 uv_offset;
uniform vec2 inv_uv_scale;

const float max_distance_threshold = 2.0;
const float const_filter_radius = 4.0;
const int sample_count = 16;
// These are the Poisson Disk Samples
const vec2 poisson[sample_count] = vec2[sample_count]
(
	vec2(-0.94201624, -0.39906216) * const_filter_radius,
	vec2( 0.94558609, -0.76890725) * const_filter_radius,
	vec2(-0.09418410, -0.92938870) * const_filter_radius,
	vec2( 0.34495938,  0.29387760) * const_filter_radius,
	vec2(-0.91588581,  0.45771432) * const_filter_radius,
	vec2(-0.81544232, -0.87912464) * const_filter_radius,
	vec2(-0.38277543,  0.27676845) * const_filter_radius,
	vec2( 0.97484398,  0.75648379) * const_filter_radius,
	vec2( 0.44323325, -0.97511554) * const_filter_radius,
	vec2( 0.53742981, -0.47373420) * const_filter_radius,
	vec2(-0.26496911, -0.41893023) * const_filter_radius,
	vec2( 0.79197514,  0.19090188) * const_filter_radius,
	vec2(-0.24188840,  0.99706507) * const_filter_radius,
	vec2(-0.81409955,  0.91437590) * const_filter_radius,
	vec2( 0.19984126,  0.78641367) * const_filter_radius,
	vec2( 0.14383161, -0.14100790) * const_filter_radius
);

vec3 lerp3(vec3 a, vec3 b, float w)
{
	return a + w * (b - a);
}

out vec4 out_color;
 
void main()
{
	float clip_depth = texture(depth_buffer, uv).x;
	float clip_depth_scaled = clip_depth * 2.0 - 1.0;
	float depth = p[3][2] / (clip_depth_scaled - p[2][2]);
	vec3 view_pos = view_ray * depth;
	
	vec3 normal = texture(normal_buffer, uv).xyz * 2.0 - 1.0;

	float filter_radius = far_plane / depth;

	vec2 noise = texture(noise_sampler, uv * noise_tile).xy;

	float ao = 0;
	for (int i = 0; i < sample_count; i++)
	{
		// sample at an offset specified by the current Poisson-Disk sample and scale it by a radius (has to be in Texture-Space)
		vec2 sample_uv = uv + (reflect(poisson[i], noise) * inv_buffer_size * filter_radius);
		float sample_clip_depth = texture(depth_buffer, sample_uv).x;
		float sample_clip_depth_scaled = sample_clip_depth * 2.0 - 1.0;
		float sample_depth = p[3][2] / (sample_clip_depth_scaled - p[2][2]);

		vec2 lerp_uv = (sample_uv - uv_offset) * inv_uv_scale;
		vec3 sample_view_ray_top = lerp3(frustum[0], frustum[1], lerp_uv.x);
		vec3 sample_view_ray_bottom = lerp3(frustum[2], frustum[3], lerp_uv.x);
		vec3 sample_view_ray = lerp3(sample_view_ray_top, sample_view_ray_bottom, lerp_uv.y);

		vec3 sample_view_pos = sample_view_ray * sample_depth;
		vec3 sample_dir = sample_view_pos - view_pos;
		float sample_distance = length(sample_dir);
		sample_dir /= sample_distance;
 
		float attenuation_distance = max(0, 1.0 - (sample_distance / max_distance_threshold));
		float attenuation_normal = max(0, dot(normal, sample_dir)) * (1.0 - abs(dot(normal, texture(normal_buffer, sample_uv).xyz * 2.0 - 1.0)));
 
		ao += attenuation_distance * attenuation_normal;
	}
 
	float final = 1.0 - ao * (2.5 / sample_count);
	out_color = vec4(final, final, final, 1);
}

#endif
