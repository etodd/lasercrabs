#ifdef VERTEX

layout(location = 0) in vec3 in_position;

uniform mat4 mvp;

out vec4 clip_position;

void main()
{
	gl_Position = clip_position = mvp * vec4(in_position, 1);
}

#else

in vec4 clip_position;
out vec4 out_color;

uniform vec2 uv_offset;
uniform vec2 uv_scale;
uniform sampler2D normal_buffer;
uniform sampler2D depth_buffer;
uniform mat4 p;
uniform vec3 light_pos;
uniform float light_radius;
uniform vec3 light_color;
uniform vec3 frustum[4];
const int type_normal = 0;
const int type_shockwave = 1;
uniform int type;

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
	float distance_to_light = length(to_light);
	to_light /= distance_to_light;

	float normal_attenuation = dot(texture(normal_buffer, uv).xyz * 2.0 - 1.0, to_light);

	float light_strength;
	if (type == type_shockwave)
	{
		const float shockwave_size = 0.5f;
		const float shockwave_multiplier = 1.0f / shockwave_size;
		light_strength = (1.0f - step(light_radius, distance_to_light)) * (distance_to_light - (light_radius - shockwave_size)) * shockwave_multiplier;
	}
	else
	{
		float distance_attenuation = max(0, 1.0 - (distance_to_light / light_radius));
		light_strength = distance_attenuation * max(0, normal_attenuation);
	}
	out_color = vec4(light_color * light_strength, 1);
}

#endif
