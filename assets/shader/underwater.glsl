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

uniform vec3 diffuse_color;
uniform mat4 p;
uniform sampler2D depth_buffer;

out vec4 out_color;

void main()
{
	float clip_depth = texture(depth_buffer, uv).x;
	float clip_depth_scaled = clip_depth * 2.0 - 1.0;
	float depth = p[3][2] / (clip_depth_scaled - p[2][2]);

	float view_ray_length = length(view_ray);
	float final_depth = view_ray_length * depth;

	out_color = vec4(diffuse_color, 1.0f - exp(final_depth * -0.2f));
}

#endif
