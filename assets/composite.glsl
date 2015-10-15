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

uniform vec3 ambient_color;
uniform sampler2D color_buffer;
uniform sampler2D lighting_buffer;
uniform sampler2D depth_buffer;
uniform sampler2D ssao_buffer;
uniform mat4 p;

void main()
{
	float clip_depth = texture(depth_buffer, uv).x;
	float clip_depth_scaled = clip_depth * 2.0 - 1.0;
	float depth = p[3][2] / (clip_depth_scaled - p[2][2]);
	gl_FragDepth = clip_depth;

	vec4 lighting = texture(lighting_buffer, uv);
	lighting.rgb += ambient_color * texture(ssao_buffer, uv).x;
	vec4 color = texture(color_buffer, uv);
	vec3 lighting_color = color.rgb * lighting.rgb;
	vec3 pos = view_ray * depth;
	const vec3 luminance_weights = vec3(0.3333, 0.3333, 0.3333);
	vec3 final_color = length(pos) < 30.0f ? lighting_color : vec3(dot(lighting_color, luminance_weights));
	out_color = vec4(final_color, 1);
}

#endif
