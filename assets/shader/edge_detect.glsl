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

// Interpolated values from the vertex shaders
in vec2 uv;
in vec3 view_ray;

uniform vec2 inv_buffer_size;
uniform mat4 p;
uniform sampler2D depth_buffer;
uniform sampler2D normal_buffer;
uniform sampler2D color_buffer;

float linearize(float clip_depth)
{
	float clip_depth_scaled = clip_depth * 2.0 - 1.0;
	return p[3][2] / (clip_depth_scaled - p[2][2]);
}

out vec4 out_color;

void main()
{
	vec3 color = texture(color_buffer, uv).rgb;
	
	float depth = linearize(texture(depth_buffer, uv).x);

	vec4 depths;
	depths.x = linearize(texture(depth_buffer, uv + vec2(-inv_buffer_size.x, 0)).x);
	depths.y = linearize(texture(depth_buffer, uv + vec2(0, -inv_buffer_size.y)).x);
	depths.z = linearize(texture(depth_buffer, uv + vec2(inv_buffer_size.x, 0)).x);
	depths.w = linearize(texture(depth_buffer, uv + vec2(0, inv_buffer_size.y)).x);

	vec3 normal = texture(normal_buffer, uv).xyz * 2.0 - 1.0;
	vec3 normal1 = texture(normal_buffer, uv + vec2(-inv_buffer_size.x, 0)).xyz * 2.0 - 1.0;
	vec3 normal2 = texture(normal_buffer, uv + vec2(0, -inv_buffer_size.y)).xyz * 2.0 - 1.0;
	vec3 normal3 = texture(normal_buffer, uv + vec2(inv_buffer_size.x, 0)).xyz * 2.0 - 1.0;
	vec3 normal4 = texture(normal_buffer, uv + vec2(0, inv_buffer_size.y)).xyz * 2.0 - 1.0;

	float normal_delta = max(0, 2.0 - dot(normal1, normal3) - dot(normal2, normal4));

	float depth_delta = (4.0 * dot(abs(vec4(depth) - depths), vec4(1)) / depth) * -dot(view_ray, normal);
	vec3 final_color = color
		+ ((vec3(1) - color) * step(0.5, normal_delta * 16.0f + depth_delta) * float(length(view_ray)));
	out_color = vec4(final_color, 1);
}

#endif
