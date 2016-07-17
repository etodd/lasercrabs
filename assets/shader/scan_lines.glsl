#ifdef VERTEX

layout(location = 0) in vec3 in_position;
layout(location = 2) in vec2 in_uv;

out vec2 uv;

void main()
{
	gl_Position = vec4(in_position, 1);
	uv = in_uv;
}

#else

in vec2 uv;

uniform vec2 buffer_size;
uniform sampler2D depth_buffer;
uniform float time;
uniform mat4 p;

out vec4 out_color;

void main()
{
	float y = time * 16 + uv.y * buffer_size.y;
	int y0 = int(y);
	int y1 = y0 + 1;
	float line1 = float(y0 % 4 == 0);
	float line2 = float(y1 % 4 == 0);
	float value = mix(line1, line2, y - y0);
	float clip_depth = texture(depth_buffer, uv).x;
	float clip_depth_scaled = clip_depth * 2.0 - 1.0;
	float depth = p[3][2] / (clip_depth_scaled - p[2][2]);
	float strength = 0.05 + clamp((depth - 5.0) / 40.0, 0, 1) * 0.3;
	float add = value * strength;
	out_color = vec4(add, add, add, 1);
}

#endif
