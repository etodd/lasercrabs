#ifdef VERTEX

layout(location = 0) in vec3 in_position;
layout(location = 2) in vec2 in_uv;

uniform mat4 v;
uniform mat4 mvp;

out vec2 uv;
out vec4 view_ray;
out vec4 clip_position;

void main()
{
	gl_Position = mvp * vec4(in_position, 1);

	view_ray = v * vec4(in_position, 0);

	clip_position = gl_Position;

	uv = in_uv;
}

#else

in vec2 uv;
in vec4 view_ray;
in vec4 clip_position;

// Values that stay constant for the whole mesh.
uniform vec4 diffuse_color;
uniform mat4 p;
uniform sampler2D diffuse_map;
uniform sampler2D depth_buffer;
uniform float fog_start;
uniform float fog_extent;
uniform vec2 uv_offset;
uniform vec2 uv_scale;

void main()
{
	vec4 color = texture(diffuse_map, uv) * diffuse_color;

	vec2 screen_uv = uv_offset + ((clip_position.xy / clip_position.w) * 0.5 + 0.5) * uv_scale;
	float clip_depth = texture(depth_buffer, screen_uv).x * 2.0 - 1.0;
	float depth = p[3][2] / (clip_depth - p[2][2]);
	float final_depth = length(view_ray * depth);
	color.a = (final_depth - fog_start) / fog_extent;

	gl_FragColor = color;
}

#endif
