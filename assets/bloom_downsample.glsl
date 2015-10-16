#ifdef VERTEX

layout(location = 0) in vec3 in_position;
layout(location = 2) in vec2 in_uv;

out vec2 uv;

void main()
{
	// Output position of the vertex, in clip space : MVP * position
	gl_Position = vec4(in_position, 1);

	uv = in_uv;
}

#else

in vec2 uv;

uniform sampler2D color_buffer;
uniform vec2 inv_buffer_size;

void main()
{
	float color0 = dot(texture(color_buffer, uv).rgb, vec3(0.333));
	float color1 = dot(texture(color_buffer, uv + vec2(inv_buffer_size.x, 0)).rgb, vec3(0.333));
	float color2 = dot(texture(color_buffer, uv + vec2(0, inv_buffer_size.y)).rgb, vec3(0.333));
	float color3 = dot(texture(color_buffer, uv + vec2(inv_buffer_size.x, inv_buffer_size.y)).rgb, vec3(0.333));
	float color = step(0.9, max(max(color0, color1), max(color2, color3)));
	gl_FragColor = vec4(color, color, color, 1);
}

#endif
