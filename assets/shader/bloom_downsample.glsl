#ifdef VERTEX

layout(location = 0) in vec3 in_position;
layout(location = 2) in vec2 in_uv;

out vec2 uv;

uniform vec2 inv_buffer_size;

void main()
{
	// Output position of the vertex, in clip space : MVP * position
	gl_Position = vec4(in_position, 1);

	uv = in_uv + inv_buffer_size * 0.5f;
}

#else

in vec2 uv;

uniform sampler2D color_buffer;
uniform vec2 inv_buffer_size;

out vec4 out_color;

void main()
{
	vec3 color0 = texture(color_buffer, uv).rgb;
	vec3 color1 = texture(color_buffer, uv + vec2(inv_buffer_size.x, 0)).rgb;
	vec3 color2 = texture(color_buffer, uv + vec2(0, inv_buffer_size.y)).rgb;
	vec3 color3 = texture(color_buffer, uv + vec2(inv_buffer_size.x, inv_buffer_size.y)).rgb;
	vec3 color = (max(max(color0, color1), max(color2, color3)) - vec3(0.95)) * 10.0f;
	out_color = vec4(color, 1);
}

#endif