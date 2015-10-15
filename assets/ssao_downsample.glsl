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

uniform sampler2D normal_buffer;
uniform sampler2D depth_buffer;
uniform vec2 inv_buffer_size;

void main()
{
	float depth0 = texture(depth_buffer, uv).x;
	float depth1 = texture(depth_buffer, uv + vec2(inv_buffer_size.x, 0)).x;
	float depth2 = texture(depth_buffer, uv + vec2(0, inv_buffer_size.y)).x;
	float depth3 = texture(depth_buffer, uv + vec2(inv_buffer_size.x, inv_buffer_size.y)).x;
	gl_FragDepth = max(max(depth0, depth1), max(depth2, depth3));

	gl_FragColor = texture(normal_buffer, uv);
}

#endif
